/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "launcher.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(AllocatorItf& allocator, const Array<RuntimeItf*>& runtimes,
    imagemanager::ImageManagerItf& imageManager, common::imagemanager::ItemStatusProviderItf& itemStatusProvider,
    SenderItf& sender, StorageItf& storage, oci::OCISpecItf& ociSpec,
    common::imagemanager::ItemInfoProviderItf& itemInfoProvider, cloudconnection::CloudConnectionItf& cloudConnection,
    networkmanager::NetworkManagerItf& networkManager, InstanceIDProviderItf& instanceIDProvider,
    resourcemanager::ResourceInfoProviderItf& resourceInfoProvider)
{
    LOG_DBG() << "Init launcher";

    mAllocator       = &allocator;
    mStorage         = &storage;
    mCloudConnection = &cloudConnection;

    if (auto err = mRuntimes.Init(allocator, runtimes); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mInstances.Init(instanceIDProvider);
    mRunner.Init(
        allocator, mRuntimes, storage, itemInfoProvider, ociSpec, networkManager, resourceInfoProvider, mMutex);
    mInstaller.Init(allocator, imageManager, itemStatusProvider);
    mStatusNotifier.Init(allocator, sender);

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LOG_DBG() << "Start launcher";

    if (auto err = mRuntimes.Start(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    {
        LockGuard lock {mMutex};

        mIsRunning = true;
    }

    mInstaller.Start();

    auto storedInstances = MakeUnique<InstanceInfoArray>(mAllocator);
    if (!storedInstances) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetAllInstancesInfos(*storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mRuntimes.InitInstances(*storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LoadStoredInstances(*storedInstances);

    // The module start job starts the stored instances.
    if (auto err = UpdateInstances({}, *storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mThread.Run([this](void*) { RunJobs(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOfflineHandler.Start(*mCloudConnection, *this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    LOG_DBG() << "Stop launcher";

    auto stopErr = mOfflineHandler.Stop();

    // The jobs not started yet are dropped, and the failure reported during the stop doesn't start a repair.
    {
        LockGuard lock {mMutex};

        mIsRunning = false;

        NotifyAll();
    }

    // Don't wait for the items being installed by the job in progress.
    mInstaller.Cancel();

    if (auto err = mThread.Join(); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    StopAllInstances();

    if (auto err = mRuntimes.Stop(); !err.IsNone() && stopErr.IsNone()) {
        stopErr = err;
    }

    return stopErr;
}

Error Launcher::UpdateInstances(const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances)
{
    LockGuard lock {mMutex};

    // The request received while the previous one is launched is rejected. The request received during the repair is
    // processed after it.
    if (mPendingUpdate || mUpdateInProgress) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    auto request = MakeUnique<UpdateRequest>(mAllocator);
    if (!request) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = request->mStopInstances.Assign(stopInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = request->mStartInstances.Assign(startInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mPendingUpdate = Move(request);

    NotifyAll();

    return ErrorEnum::eNone;
}

Error Launcher::OnInstancesStatusesReceived(const Array<InstanceStatus>& statuses)
{
    Error err;

    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Instances statuses received" << Log::Field("count", statuses.Size());

        for (const auto& status : statuses) {
            LOG_INF() << "Instance status received" << Log::Field("instance", status)
                      << Log::Field("runtimeID", status.mRuntimeID) << Log::Field("state", status.mState)
                      << Log::Field(status.mError);

            if (auto storeErr = HandleComponentStatus(status); err.IsNone() && !storeErr.IsNone()) {
                err = AOS_ERROR_WRAP(storeErr);
            }

            HandleBrokenInstance(status);
        }

        if (!mFirstStart) {
            if (auto sendErr = mStatusNotifier.SendUpdateStatuses(statuses); err.IsNone() && !sendErr.IsNone()) {
                err = AOS_ERROR_WRAP(sendErr);
            }
        }
    }

    mStatusNotifier.NotifyListeners(statuses);

    return err;
}

Error Launcher::RebootRequired(const String& runtimeID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Reboot required notification received" << Log::Field("runtimeID", runtimeID);

    if (mRebootQueue.Contains(runtimeID)) {
        return ErrorEnum::eNone;
    }

    if (auto err = mRebootQueue.EmplaceBack(runtimeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    NotifyAll();

    return ErrorEnum::eNone;
}

Error Launcher::GetInstancesStatuses(Array<InstanceStatus>& statuses)
{
    UniqueLock lock {mMutex};

    // The repair may wait for the CM, which is connected by the modules that get the statuses: don't wait for it. The
    // statuses changed by the repair are sent when it is finished.
    if (auto err = mCondVar.Wait(lock, [this]() { return !mPendingUpdate && !mUpdateInProgress; }); !err.IsNone()) {
        LOG_ERR() << "Can't wait for launch finish" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_DBG() << "Get instances statuses" << Log::Field("count", mInstances.Size());

    if (auto err = mInstances.GetStatuses(statuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::SubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    return mStatusNotifier.Subscribe(listener);
}

Error Launcher::UnsubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    return mStatusNotifier.Unsubscribe(listener);
}

Error Launcher::GetInstanceMonitoringParams(const InstanceIdent& instanceIdent, InstanceMonitoringParams& params) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get instance monitoring params" << Log::Field("instance", instanceIdent);

    const auto* const instanceData = mInstances.Find(instanceIdent);
    if (!instanceData) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance not found"));
    }

    if (!instanceData->mInfo.mMonitoringParams.HasValue()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotSupported);
    }

    params = instanceData->mInfo.mMonitoringParams.GetValue();

    return ErrorEnum::eNone;
}

Error Launcher::GetInstanceMonitoringData(
    const InstanceIdent& instanceIdent, monitoring::InstanceMonitoringData& monitoringData)
{
    LockGuard lock {mMutex};

    const auto* const instanceData = mInstances.Find(instanceIdent);
    if (!instanceData) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance not found"));
    }

    if (auto* runtime = mRuntimes.Find(instanceData->mStatus.mRuntimeID); runtime != nullptr) {
        if (auto err = runtime->GetInstanceMonitoringData(instanceIdent, monitoringData); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "runtime not found"));
}

Error Launcher::GetRuntimesInfos(Array<RuntimeInfo>& runtimes) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get runtimes infos";

    if (auto err = mRuntimes.GetRuntimesInfos(runtimes); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Launcher::OnOfflineCheck()
{
    LockGuard lock {mMutex};

    mOfflineCheckRequested = true;

    NotifyAll();
}

void Launcher::RunJobs()
{
    while (true) {
        UniqueLock lock {mMutex};

        if (auto err = mCondVar.Wait(lock,
                [this]() {
                    return !mIsRunning || mPendingUpdate || mRepairRequested || mOfflineCheckRequested
                        || !mRebootQueue.IsEmpty();
                });
            !err.IsNone()) {
            LOG_ERR() << "Can't wait for launcher job" << Log::Field(AOS_ERROR_WRAP(err));
        }

        if (!mIsRunning) {
            break;
        }

        if (mPendingUpdate) {
            auto request = Move(mPendingUpdate);

            mUpdateInProgress = true;

            (void)lock.Unlock();

            RunUpdate(request);
        } else if (mRepairRequested) {
            (void)lock.Unlock();

            if (RepairBrokenInstances()) {
                LockGuard sendLock {mMutex};

                mStatusNotifier.SendNodeStatuses(mInstances);
            }
        } else if (mOfflineCheckRequested) {
            mOfflineCheckRequested = false;

            (void)lock.Unlock();

            StopExpiredInstances();
        } else {
            const auto runtimeIDs = mRebootQueue;

            mRebootQueue.Clear();

            (void)lock.Unlock();

            mRuntimes.Reboot(runtimeIDs);
        }

        ArmOfflineHandler();
    }

    LockGuard lock {mMutex};

    mPendingUpdate.Reset();
    mUpdateInProgress = false;

    NotifyAll();
}

void Launcher::RunUpdate(UniquePtr<UpdateRequest>& request)
{
    // mFirstStart is changed only by the launcher thread.
    const auto moduleStart = mFirstStart;

    LaunchInstances(request->mStopInstances, request->mStartInstances, moduleStart);

    request.Reset();

    {
        LockGuard lock {mMutex};

        mUpdateInProgress = false;
        mFirstStart       = false;

        NotifyAll();
    }

    const auto repaired = RepairBrokenInstances();

    // On module start, the node instances statuses are sent only if the repair changes them.
    if (moduleStart && !repaired) {
        return;
    }

    LockGuard lock {mMutex};

    mStatusNotifier.SendNodeStatuses(mInstances);
}

void Launcher::LaunchInstances(
    Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances, bool moduleStart)
{
    LOG_INF() << "[profiling] Update instances begin" << Log::Field("stopCount", stopInstances.Size())
              << Log::Field("startCount", startInstances.Size());

    auto instances = MakeUnique<InstanceDataArray>(mAllocator);
    if (!instances) {
        LOG_ERR() << "Failed to allocate instances" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    auto removeItems = MakeUnique<StaticArray<ItemVersion, cMaxNumUpdateItems>>(mAllocator);
    if (!removeItems) {
        LOG_ERR() << "Failed to allocate remove update items" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    auto installItems = MakeUnique<StaticArray<InstallItem, cMaxNumUpdateItems>>(mAllocator);
    if (!installItems) {
        LOG_ERR() << "Failed to allocate install items" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    if (auto err = AppendInstancesWithModifiedParams(startInstances, stopInstances); !err.IsNone()) {
        LOG_ERR() << "Failed to append instances with modified params to stop list" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (!moduleStart) {
        mInstaller.GetUnusedItems(mInstances, stopInstances, startInstances, *removeItems);
    }

    FindInstances(stopInstances, *instances);

    mRunner.Stop(*instances);
    mRunner.Release(*instances);

    RemoveInstances(*instances);

    // On module start, the stored instances are already in the registry and their items are installed.
    if (!moduleStart) {
        LOG_INF() << "[profiling] Install items begin" << Log::Field("removeCount", removeItems->Size())
                  << Log::Field("installCount", startInstances.Size());

        mInstaller.Remove(*removeItems);

        auto installError = mInstaller.Install(startInstances, *installItems);

        LOG_INF() << "[profiling] Install items end";

        AddStartInstances(startInstances, *installItems, installError);
    }

    instances->Clear();

    FindInstances(startInstances, *instances);

    mRunner.Start(*instances);

    LOG_INF() << "[profiling] Update instances end";
}

void Launcher::AddStartInstances(
    const Array<InstanceInfo>& startInstances, const Array<InstallItem>& installItems, const Error& installError)
{
    LockGuard lock {mMutex};

    for (const auto& info : startInstances) {
        auto* instance = mInstances.Find(info);

        // The running instance doesn't use the item being installed, so it keeps running. The instance whose configs
        // failed to load on module start is prepared again to load them.
        const auto keepRunning = instance != nullptr && instance->mStatus.mState != InstanceStateEnum::eFailed
            && (instance->mInfo.mType != UpdateItemTypeEnum::eService || instance->mConfigsLoaded);

        if (instance == nullptr) {
            LOG_DBG() << "Add instance" << Log::Field("instance", info);

            Error err;

            if (Tie(instance, err) = mInstances.Add(info); !err.IsNone()) {
                LOG_ERR() << "Failed to add instance" << Log::Field("instance", info)
                          << Log::Field(AOS_ERROR_WRAP(err));

                continue;
            }
        } else if (!keepRunning) {
            // The failed instance is launched again: the error of the new attempt is set.
            if (instance->mStatus.mState == InstanceStateEnum::eFailed) {
                instance->mStatus.mState = InstanceStateEnum::eInactive;
                instance->mStatus.mError = ErrorEnum::eNone;
            }

            instance->mPrepared = false;
        }

        auto err  = installError;
        auto item = installItems.FindIf(
            [&info](const InstallItem& item) { return item.mID == info.mItemID && item.mVersion == info.mVersion; });

        if (err.IsNone() && item != installItems.end()) {
            err = item->mError;
        }

        if (err.IsNone()) {
            continue;
        }

        if (keepRunning) {
            LOG_WRN() << "Item install failed, keep running instance" << Log::Field("instance", info)
                      << Log::Field(err);

            continue;
        }

        instance->mStatus.mState = InstanceStateEnum::eFailed;
        instance->mStatus.mError = err;
    }
}

bool Launcher::RepairBrokenInstances()
{
    auto brokenInstances = MakeUnique<InstanceDataArray>(mAllocator);
    if (!brokenInstances) {
        LOG_ERR() << "Failed to allocate broken instances" << Log::Field(ErrorEnum::eNoMemory);

        return false;
    }

    auto repairItems = MakeUnique<StaticArray<InstallItem, cMaxNumUpdateItems>>(mAllocator);
    if (!repairItems) {
        LOG_ERR() << "Failed to allocate repair items" << Log::Field(ErrorEnum::eNoMemory);

        return false;
    }

    const auto findItem = [&repairItems](const InstanceInfo& info) {
        return repairItems->FindIf(
            [&info](const InstallItem& item) { return item.mID == info.mItemID && item.mVersion == info.mVersion; });
    };

    {
        LockGuard lock {mMutex};

        mRepairRequested = false;

        if (!mIsRunning) {
            return false;
        }

        for (auto& instance : mInstances) {
            if (instance.mStatus.mState != InstanceStateEnum::eFailed
                || !instance.mStatus.mError.Is(ErrorEnum::eIntegrity)) {
                continue;
            }

            const auto& info = instance.mInfo;

            if (auto err = brokenInstances->PushBack(&instance); !err.IsNone()) {
                LOG_ERR() << "Failed to add broken instance" << Log::Field("instance", info)
                          << Log::Field(AOS_ERROR_WRAP(err));

                continue;
            }

            if (findItem(info) != repairItems->end()) {
                continue;
            }

            if (auto err = repairItems->EmplaceBack(
                    InstallItem {{info.mItemID, info.mType, info.mVersion, info.mManifestDigest}, {}});
                !err.IsNone()) {
                LOG_ERR() << "Failed to add repair item" << Log::Field("itemID", info.mItemID)
                          << Log::Field("version", info.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }

    // The item which was already repaired since the last request for it isn't repaired again.
    mInstaller.Repair(*repairItems);

    if (repairItems->IsEmpty()) {
        return false;
    }

    LOG_INF() << "[profiling] Repair instances begin" << Log::Field("itemCount", repairItems->Size());

    {
        LockGuard lock {mMutex};

        // The instances of an item which can't be repaired stay failed with the repair error. The others are launched
        // again: an instance which fails again stays failed with the error of this attempt.
        brokenInstances->RemoveIf([&](InstanceData* instance) {
            auto item = findItem(instance->mInfo);
            if (item == repairItems->end()) {
                return true;
            }

            if (!item->mError.IsNone()) {
                instance->mStatus.mError = item->mError;

                return true;
            }

            instance->mStatus.mState = InstanceStateEnum::eInactive;
            instance->mStatus.mError = ErrorEnum::eNone;
            instance->mPrepared      = false;

            return false;
        });
    }

    mRunner.Start(*brokenInstances);

    LOG_INF() << "[profiling] Repair instances end";

    return true;
}

void Launcher::StopExpiredInstances()
{
    auto expiredInstances = MakeUnique<InstanceDataArray>(mAllocator);
    if (!expiredInstances) {
        LOG_ERR() << "Failed to allocate expired instances" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    {
        LockGuard lock {mMutex};

        for (auto& instance : mInstances) {
            if ((instance.mStatus.mState != InstanceStateEnum::eActive
                    && instance.mStatus.mState != InstanceStateEnum::eActivating)
                || !mOfflineHandler.IsExpired(instance.mOfflineTTL)) {
                continue;
            }

            LOG_INF() << "Offline TTL expired" << Log::Field("instance", instance.mInfo)
                      << Log::Field("offlineTTL", instance.mOfflineTTL);

            if (auto err = expiredInstances->PushBack(&instance); !err.IsNone()) {
                LOG_ERR() << "Failed to add expired instance" << Log::Field("instance", instance.mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }

    if (expiredInstances->IsEmpty()) {
        return;
    }

    mRunner.Stop(*expiredInstances);
}

void Launcher::ArmOfflineHandler()
{
    Optional<Duration> minOfflineTTL;

    {
        LockGuard lock {mMutex};

        minOfflineTTL = mInstances.GetMinOfflineTTL();
    }

    mOfflineHandler.Arm(minOfflineTTL);
}

void Launcher::LoadStoredInstances(const Array<InstanceInfo>& storedInstances)
{
    LOG_DBG() << "Load stored instances" << Log::Field("count", storedInstances.Size());

    auto instances = MakeUnique<InstanceDataArray>(mAllocator);
    if (!instances) {
        LOG_ERR() << "Failed to allocate instances" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    {
        LockGuard lock {mMutex};

        for (const auto& info : storedInstances) {
            auto [instance, err] = mInstances.Add(info);
            if (!err.IsNone()) {
                LOG_ERR() << "Failed to add instance" << Log::Field("instance", info)
                          << Log::Field(AOS_ERROR_WRAP(err));

                continue;
            }

            // The stored instance is prepared: its configs which failed to load are loaded on its start.
            instance->mPrepared = true;

            if (auto err = instances->PushBack(instance); !err.IsNone()) {
                LOG_ERR() << "Failed to add instance" << Log::Field("instance", info)
                          << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }

    mRunner.LoadConfigs(*instances);
}

void Launcher::StopAllInstances()
{
    auto instances = MakeUnique<InstanceDataArray>(mAllocator);
    if (!instances) {
        LOG_ERR() << "Failed to allocate instances" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    for (auto& instance : mInstances) {
        if (instance.mInfo.mType != UpdateItemTypeEnum::eService) {
            continue;
        }

        if (auto err = instances->PushBack(&instance); !err.IsNone()) {
            LOG_ERR() << "Failed to add instance" << Log::Field("instance", instance.mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    mRunner.Stop(*instances);
}

void Launcher::RemoveInstances(const Array<InstanceData*>& instances)
{
    // The idents are copied first: removing an instance moves the next ones.
    auto idents = MakeUnique<StaticArray<InstanceIdent, cMaxNumInstances>>(mAllocator);
    if (!idents) {
        LOG_ERR() << "Failed to allocate removed instances" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    for (const auto* instance : instances) {
        (void)idents->PushBack(static_cast<const InstanceIdent&>(instance->mInfo));
    }

    LockGuard lock {mMutex};

    for (const auto& ident : *idents) {
        LOG_DBG() << "Remove instance" << Log::Field("instance", ident);

        mInstances.Remove(ident);
    }
}

template <typename T>
void Launcher::FindInstances(const Array<T>& idents, Array<InstanceData*>& instances)
{
    for (const auto& ident : idents) {
        auto* instance = mInstances.Find(ident);
        if (instance == nullptr) {
            LOG_WRN() << "Instance not found" << Log::Field("instance", static_cast<const InstanceIdent&>(ident));

            continue;
        }

        if (instances.Contains(instance)) {
            continue;
        }

        if (auto err = instances.PushBack(instance); !err.IsNone()) {
            LOG_ERR() << "Failed to add instance" << Log::Field("instance", instance->mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }
}

Error Launcher::AppendInstancesWithModifiedParams(
    const Array<InstanceInfo>& startInstances, Array<InstanceIdent>& stopInstances)
{
    for (const auto& startInstance : startInstances) {
        const auto* const instanceData = mInstances.Find(startInstance);
        if (!instanceData) {
            continue;
        }

        if (instanceData->mInfo == startInstance) {
            continue;
        }

        if (stopInstances.Contains(startInstance)) {
            continue;
        }

        LOG_DBG() << "Instance parameters modified, adding to stop list" << Log::Field("instance", startInstance);

        if (auto err = stopInstances.EmplaceBack(startInstance);
            !err.IsNone()) { // NOSONAR cpp:S5912 - intentional InstanceIdent from InstanceInfo
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::HandleComponentStatus(const aos::InstanceStatus& status)
{
    if (status.mType != UpdateItemTypeEnum::eComponent) {
        return ErrorEnum::eNone;
    }

    if (status.mState == InstanceStateEnum::eInactive) {
        return mStorage->RemoveInstanceInfo(status);
    }

    if (!status.mPreinstalled) {
        return ErrorEnum::eNone;
    }

    if (mInstances.Find(status) != nullptr) {
        return ErrorEnum::eNone;
    }

    auto instanceInfo = MakeUnique<InstanceInfo>(mAllocator);
    if (!instanceInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    static_cast<InstanceIdent&>(*instanceInfo)
        = status; // NOSONAR cpp:S5912 - intentional copy of InstanceIdent base only
    instanceInfo->mRuntimeID      = status.mRuntimeID;
    instanceInfo->mType           = status.mType;
    instanceInfo->mVersion        = status.mVersion;
    instanceInfo->mManifestDigest = status.mManifestDigest;

    if (auto err = mStorage->UpdateInstanceInfo(*instanceInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Launcher::HandleBrokenInstance(const InstanceStatus& status)
{
    if (status.mState != InstanceStateEnum::eFailed || !status.mError.Is(ErrorEnum::eIntegrity)) {
        return;
    }

    auto* instance = mInstances.Find(status);
    if (instance == nullptr || instance->mInfo.mVersion != status.mVersion) {
        return;
    }

    LOG_WRN() << "Instance content is broken" << Log::Field("instance", instance->mInfo) << Log::Field(status.mError);

    instance->mStatus.mState = InstanceStateEnum::eFailed;
    instance->mStatus.mError = status.mError;
    mRepairRequested         = true;

    NotifyAll();
}

void Launcher::NotifyAll()
{
    if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify launcher" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

} // namespace aos::sm::launcher
