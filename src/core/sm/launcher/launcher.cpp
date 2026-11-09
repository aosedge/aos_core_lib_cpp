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

    mAllocator = &allocator;

    for (auto* runtime : runtimes) {
        if (auto err = mRuntimes.Set(runtime, ""); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    mImageManager         = &imageManager;
    mItemStatusProvider   = &itemStatusProvider;
    mStorage              = &storage;
    mSender               = &sender;
    mOCISpec              = &ociSpec;
    mItemInfoProvider     = &itemInfoProvider;
    mCloudConnection      = &cloudConnection;
    mNetworkManager       = &networkManager;
    mInstanceIDProvider   = &instanceIDProvider;
    mResourceInfoProvider = &resourceInfoProvider;

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LOG_DBG() << "Start launcher";

    for (auto& it : mRuntimes) {
        if (auto err = it.mFirst->Start(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    UniqueLock lock {mMutex};

    for (auto& it : mRuntimes) {
        RuntimeInfo runtimeInfo;

        if (auto err = it.mFirst->GetRuntimeInfo(runtimeInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = it.mSecond.Assign(runtimeInfo.mRuntimeID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        LOG_INF() << "Registered runtime" << Log::Field("runtimeID", runtimeInfo.mRuntimeID)
                  << Log::Field("type", runtimeInfo.mRuntimeType);
    }

    mIsRunning        = true;
    mInstallsCanceled = false;

    if (auto err = mRebootThread.Run([this](void*) { RunRebootThread(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto storedInstances = MakeUnique<InstanceInfoArray>(mAllocator);
    if (!storedInstances) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetAllInstancesInfos(*storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = InitInstances(*storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    (void)lock.Unlock();

    LoadInstancesData(*storedInstances);

    if (auto err = UpdateInstances({}, *storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mCloudConnection->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!mCloudConnection->IsConnected()) {
        OnDisconnect();
    }

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    Error stopErr;

    if (auto err = mCloudConnection->UnsubscribeListener(*this); !err.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    // Don't wait for the items being installed by the launch in progress.
    CancelInstalls();

    {
        UniqueLock lock {mMutex};

        LOG_DBG() << "Stop launcher";

        if (auto err = mCondVar.Wait(lock, [this]() { return !mLaunchInProgress; }); !err.IsNone()) {
            LOG_ERR() << "Can't wait for launch finish" << Log::Field(AOS_ERROR_WRAP(err));
        }

        (void)lock.Unlock();

        auto err = mLaunchPool.Run();

        if (err.IsNone()) {
            StopAllInstances();
            StopAllNetworks();

            err = mLaunchPool.Shutdown();
        }

        if (!err.IsNone() && stopErr.IsNone()) {
            stopErr = AOS_ERROR_WRAP(err);
        }

        for (auto& it : mRuntimes) {
            err = it.mFirst->Stop();
            if (!err.IsNone() && stopErr.IsNone()) {
                stopErr = AOS_ERROR_WRAP(err);
            }
        }

        (void)lock.Lock();

        mIsRunning = false;

        if (auto notifyErr = mCondVar.NotifyAll(); !notifyErr.IsNone()) {
            LOG_ERR() << "Can't notify launcher" << Log::Field(AOS_ERROR_WRAP(notifyErr));
        }
    }

    if (auto err = mThread.Join(); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    if (auto err = mRebootThread.Join(); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    if (auto err = mOfflineTTLHandler.Stop(Timer::StopMode::WaitForCallbacks); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    return stopErr;
}

Error Launcher::UpdateInstances(const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances)
{
    auto err = StartLaunch();
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto finishLaunch = DeferRelease(this, [&err](Launcher* self) {
        if (!err.IsNone()) {
            self->FinishLaunch();
        }
    });

    // Wait in case previous request is not yet finished
    if (auto joinErr = mThread.Join(); !joinErr.IsNone()) {
        LOG_ERR() << "Can't join launcher thread" << Log::Field(AOS_ERROR_WRAP(joinErr));
    }

    auto stop = MakeShared<StaticArray<InstanceIdent, cMaxNumInstances>>(mAllocator, stopInstances);
    if (!stop) {
        err = AOS_ERROR_WRAP(ErrorEnum::eNoMemory);

        return err;
    }

    auto start = MakeShared<InstanceInfoArray>(mAllocator, startInstances);
    if (!start) {
        err = AOS_ERROR_WRAP(ErrorEnum::eNoMemory);

        return err;
    }

    err = mThread.Run([this, stop, start](void*) {
        // mFirstStart is changed only by this thread.
        const auto moduleStart = mFirstStart;

        UpdateInstancesImpl(*stop, *start);

        if (moduleStart) {
            RepairModuleStartInstances();
        }

        FinishLaunch();
    });
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

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
        }

        if (!mFirstStart) {
            if (auto sendErr = mSender->SendUpdateInstancesStatuses(statuses); err.IsNone() && !sendErr.IsNone()) {
                err = AOS_ERROR_WRAP(sendErr);
            }
        }
    }

    {
        LockGuard lock {mSubscribersMutex};

        for (auto* subscriber : mSubscribers) {
            subscriber->OnInstancesStatusesChanged(statuses);
        }
    }

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

    if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify launcher" << Log::Field(AOS_ERROR_WRAP(err));
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetInstancesStatuses(Array<InstanceStatus>& statuses)
{
    UniqueLock lock {mMutex};

    // The module start repair may wait for the CM, which is connected by the modules that get the statuses: don't wait
    // for it. The statuses changed by the repair are sent when it is finished.
    if (auto err = mCondVar.Wait(lock, [this]() { return !mLaunchInProgress || mModuleStartRepair; }); !err.IsNone()) {
        LOG_ERR() << "Can't wait for launch finish" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_DBG() << "Get instances statuses" << Log::Field("count", mInstances.Size());

    statuses.Clear();

    for (const auto& instance : mInstances) {
        if (auto err = statuses.EmplaceBack(instance.mStatus); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::SubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mSubscribersMutex};

    LOG_DBG() << "Subscribe instance status listener";

    if (mSubscribers.Contains(&listener)) {
        return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
    }

    if (auto err = mSubscribers.EmplaceBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::UnsubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mSubscribersMutex};

    LOG_DBG() << "Unsubscribe instance status listener";

    return mSubscribers.Remove(&listener) == 0 ? AOS_ERROR_WRAP(ErrorEnum::eNotFound) : ErrorEnum::eNone;
}

Error Launcher::GetInstanceMonitoringParams(const InstanceIdent& instanceIdent, InstanceMonitoringParams& params) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get instance monitoring params" << Log::Field("instance", instanceIdent);

    const auto* const instanceData = FindInstanceData(instanceIdent);
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

    const auto* const instanceData = FindInstanceData(instanceIdent);
    if (!instanceData) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance not found"));
    }

    if (auto* runtime = FindInstanceRuntime(instanceData->mStatus.mRuntimeID); runtime != nullptr) {
        if (auto err = runtime->GetInstanceMonitoringData(instanceIdent, monitoringData); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        monitoringData.mRuntimeID     = instanceData->mStatus.mRuntimeID;
        monitoringData.mInstanceIdent = instanceIdent;

        return ErrorEnum::eNone;
    }

    return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "Runtime not found"));
}

Error Launcher::GetRuntimesInfos(Array<RuntimeInfo>& runtimes) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get runtimes infos";

    for (const auto& runtime : mRuntimes) {
        if (auto err = runtimes.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = runtime.mFirst->GetRuntimeInfo(runtimes.Back()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Launcher::OnConnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Cloud connected event";

    mOfflineTime.Reset();
}

void Launcher::OnDisconnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Cloud disconnected event";

    mOfflineTime = Time::Now();

    StartTTLTimer();
}

Error Launcher::InitInstances(const Array<InstanceInfo>& instancesInfo)
{
    LOG_DBG() << "Init instances" << Log::Field("numInstances", instancesInfo.Size());

    for (auto& it : mRuntimes) {
        auto runtimeInstances = MakeUnique<InstanceInfoArray>(mAllocator);
        if (!runtimeInstances) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        for (const auto& instanceInfo : instancesInfo) {
            if (instanceInfo.mRuntimeID != it.mSecond) {
                continue;
            }

            if (auto err = runtimeInstances->PushBack(instanceInfo); !err.IsNone()) {
                LOG_ERR() << "Failed to add instance to runtime init list" << Log::Field("instance", instanceInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));

                break;
            }
        }

        if (auto err = it.mFirst->InitInstances(*runtimeInstances); !err.IsNone()) {
            LOG_ERR() << "Failed to init instances" << Log::Field("runtimeID", it.mSecond)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    return ErrorEnum::eNone;
}

void Launcher::RunRebootThread()
{
    while (true) {
        StaticArray<RuntimeItf*, cMaxNumNodeRuntimes> runtimesToReboot;

        {
            UniqueLock lock {mMutex};

            if (auto err = mCondVar.Wait(
                    lock, [this]() { return !mIsRunning || (!mLaunchInProgress && !mRebootQueue.IsEmpty()); });
                !err.IsNone()) {
                LOG_ERR() << "Can't wait for reboot request" << Log::Field(AOS_ERROR_WRAP(err));
            }

            if (!mIsRunning) {
                return;
            }

            for (const auto& runtimeID : mRebootQueue) {
                auto* runtime = mRuntimes.FindIf([&runtimeID](const auto& pair) { return pair.mSecond == runtimeID; });
                if (runtime == mRuntimes.end()) {
                    LOG_ERR() << "Runtime for reboot not found" << Log::Field("runtimeID", runtimeID);

                    continue;
                }

                if (auto err = runtimesToReboot.EmplaceBack(runtime->mFirst); !err.IsNone()) {
                    LOG_ERR() << "Failed to add runtime to reboot list" << Log::Field(AOS_ERROR_WRAP(err));
                }
            }

            mRebootQueue.Clear();
        }

        for (auto* runtime : runtimesToReboot) {
            if (auto err = runtime->Reboot(); !err.IsNone()) {
                LOG_ERR() << "Reboot runtime failed" << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }
}

void Launcher::HandleOfflineTTLs()
{
    UniqueLock lock {mMutex};

    LOG_DBG() << "Start offline TTL handler";

    if (auto err = mCondVar.Wait(lock, [this]() { return !mIsRunning || !mLaunchInProgress; }); !err.IsNone()) {
        LOG_ERR() << "Can't wait for launch finish" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (!mIsRunning || !mOfflineTime.HasValue()) {
        return;
    }

    mLaunchInProgress = true;

    StopExpiredInstances(lock);

    mLaunchInProgress = false;

    if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify launcher" << Log::Field(AOS_ERROR_WRAP(err));
    }

    StartTTLTimer();
}

Optional<Duration> Launcher::GetMinOfflineTTL() const
{
    Optional<Duration> earliest;

    for (const auto& instance : mInstances) {
        if (!instance.mOfflineTTL
            || (instance.mStatus.mState != InstanceStateEnum::eActive
                && instance.mStatus.mState != InstanceStateEnum::eActivating)) {
            continue;
        }

        if (!earliest.HasValue()) {
            earliest = instance.mOfflineTTL;

            continue;
        }

        earliest = Min(*earliest, instance.mOfflineTTL);
    }

    return earliest;
}

void Launcher::StartTTLTimer()
{
    if (!mOfflineTime.HasValue()) {
        return;
    }

    auto earliestDeadline = GetMinOfflineTTL();
    if (!earliestDeadline.HasValue()) {
        return;
    }

    const auto deadline  = mOfflineTime->Add(*earliestDeadline);
    const auto now       = Time::Now();
    const auto remaining = deadline > now ? deadline.Sub(now) : Time::cMilliseconds;

    if (auto err = mOfflineTTLHandler.Start(remaining, [this](void*) { HandleOfflineTTLs(); }); !err.IsNone()) {
        LOG_ERR() << "Can't start offline TTL handler" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

void Launcher::StopExpiredInstances(UniqueLock<Mutex>& lock)
{
    if (auto err = mOfflineTTLPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start offline TTL thread pool" << Log::Field(AOS_ERROR_WRAP(err));
        return;
    }

    for (auto& instance : mInstances) {
        if ((instance.mStatus.mState != InstanceStateEnum::eActive
                && instance.mStatus.mState != InstanceStateEnum::eActivating)
            || !instance.mOfflineTTL || mOfflineTime->Add(instance.mOfflineTTL) > Time::Now()) {
            continue;
        }

        auto* runtime = FindInstanceRuntime(instance.mStatus.mRuntimeID);
        if (!runtime) {
            LOG_ERR() << "Runtime not found for expired TTL instance" << Log::Field("instance", instance.mInfo);
            continue;
        }

        if (auto err = mOfflineTTLPool.AddTask([runtime, &instance](void*) {
                if (auto err = runtime->StopInstance(instance.mInfo, instance.mStatus); !err.IsNone()) {
                    LOG_ERR() << "Failed to stop instance after offline TTL expired"
                              << Log::Field("instance", instance.mInfo)
                              << Log::Field("offlineTTL", instance.mOfflineTTL) << Log::Field(AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
            LOG_ERR() << "Failed to schedule stop for expired TTL instance" << Log::Field("instance", instance.mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    (void)lock.Unlock();

    if (auto err = mOfflineTTLPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Offline TTL thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    (void)lock.Lock();
}

void Launcher::SendNodeInstancesStatuses()
{
    LOG_INF() << "Send node instances statuses" << Log::Field("count", mInstances.Size());

    auto statuses = MakeUnique<InstanceStatusArray>(mAllocator);
    if (!statuses) {
        LOG_ERR() << "Failed to allocate instance statuses" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    for (const auto& instance : mInstances) {
        LOG_INF() << "Node instance status" << Log::Field("instance", instance.mInfo)
                  << Log::Field("runtimeID", instance.mInfo.mRuntimeID) << Log::Field("state", instance.mStatus.mState)
                  << Log::Field(instance.mStatus.mError);

        if (auto err = statuses->EmplaceBack(instance.mStatus); !err.IsNone()) {
            LOG_ERR() << "Failed to add instance status to list" << Log::Field("instance", instance.mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mSender->SendNodeInstancesStatuses(*statuses); !err.IsNone()) {
        LOG_ERR() << "Failed to send node instances statuses" << Log::Field(err);
    }
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

    if (mInstances.ContainsIf([&status](const auto& instance) {
            return static_cast<const InstanceIdent&>(instance.mStatus) == static_cast<const InstanceIdent&>(status);
        })) {
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

Error Launcher::LoadInstanceData(InstanceData& instanceData)
{
    auto itemConfig = MakeUnique<oci::ItemConfig>(mAllocator);
    if (!itemConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto imageConfig = MakeUnique<oci::ImageConfig>(mAllocator);
    if (!imageConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    // If the configs aren't loaded on module start, the start of the instance loads them again: the broken content
    // fails the instance with eIntegrity and is repaired. Otherwise the next UpdateInstances prepares the instance
    // again.
    if (auto err = LoadInstanceConfigs(instanceData.mInfo, *itemConfig, *imageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    instanceData.mOfflineTTL    = itemConfig->mOfflineTTL;
    instanceData.mConfigsLoaded = true;

    return ErrorEnum::eNone;
}

void Launcher::LoadInstancesData(const Array<InstanceInfo>& storedInstances)
{
    LOG_DBG() << "Load instances data" << Log::Field("count", storedInstances.Size());

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    for (const auto& instanceInfo : storedInstances) {
        auto [instanceData, err] = AddInstanceData(instanceInfo);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add instance data" << Log::Field("instance", instanceInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }

        if (instanceData->mInfo.mType != UpdateItemTypeEnum::eService) {
            continue;
        }

        err = mLaunchPool.AddTask([this, instanceData](void*) {
            if (auto err = LoadInstanceData(*instanceData); !err.IsNone()) {
                LOG_ERR() << "Failed to load instance data" << Log::Field("instance", instanceData->mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to load instance data" << Log::Field("instance", instanceInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

void Launcher::UpdateInstancesImpl(Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances)
{
    LOG_INF() << "[profiling] Update instances begin" << Log::Field("stopCount", stopInstances.Size())
              << Log::Field("startCount", startInstances.Size());

    auto sendStatus = DeferRelease(&mInstances, [this](Array<InstanceData>*) {
        LockGuard lock {mMutex};

        if (!mFirstStart) {
            SendNodeInstancesStatuses();

            return;
        }

        mFirstStart = false;
    });

    // The buffers are allocated before the thread pool is started, so an allocation failure doesn't leave it running.
    auto removeItems = MakeUnique<StaticArray<UpdateItemInfo, cMaxNumUpdateItems>>(mAllocator);
    if (!removeItems) {
        LOG_ERR() << "Failed to allocate remove update items" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    auto installItems = MakeUnique<StaticArray<InstallItem, cMaxNumUpdateItems>>(mAllocator);
    if (!installItems) {
        LOG_ERR() << "Failed to allocate install items" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    // An item is repaired at most once per request, so the repaired items are shared by the prepare and start stages.
    auto repairedItems = MakeUnique<StaticArray<InstallItem, cMaxNumUpdateItems>>(mAllocator);
    if (!repairedItems) {
        LOG_ERR() << "Failed to allocate repaired items" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    auto processedInstances = MakeUnique<StaticArray<InstanceIdent, cMaxNumInstances>>(mAllocator);
    if (!processedInstances) {
        LOG_ERR() << "Failed to allocate processed instances" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    if (auto err = AppendInstancesWithModifiedParams(startInstances, stopInstances); !err.IsNone()) {
        LOG_ERR() << "Failed to append instances with modified params to stop list" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (!mFirstStart) {
        GetRemoveUpdateItems(stopInstances, startInstances, *removeItems);
    }

    StopInstances(stopInstances);
    StopNetworks(stopInstances);
    RemoveInstances(stopInstances);

    if (!mFirstStart) {
        LOG_INF() << "[profiling] Install items begin" << Log::Field("removeCount", removeItems->Size())
                  << Log::Field("installCount", startInstances.Size());

        RemoveUpdateItems(*removeItems);
        auto installError = InstallUpdateItems(startInstances, *installItems);

        LOG_INF() << "[profiling] Install items end";

        PrepareInstances(startInstances, *installItems, installError, *processedInstances);
        RepairInstances(*processedInstances, *repairedItems, &Launcher::AddPrepareInstanceTask);
    }

    StartNetworks(startInstances);

    processedInstances->Clear();

    StartInstances(startInstances, *processedInstances);

    // On module start, the items are repaired after the launch by RepairModuleStartInstances: the repair may wait for
    // the CM, which needs the modules started after the launcher.
    if (!mFirstStart) {
        RepairInstances(*processedInstances, *repairedItems, &Launcher::AddStartInstanceTask);
    }

    if (auto err = mLaunchPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Update instances end";
}

void Launcher::StopInstances(const Array<InstanceIdent>& stopInstances)
{
    LOG_INF() << "[profiling] Stop instances begin" << Log::Field("count", stopInstances.Size());

    for (const auto& instance : stopInstances) {
        auto instanceData = FindInstanceData(instance);
        if (!instanceData) {
            LOG_ERR() << "Failed to stop instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance data not found")));

            continue;
        }

        if (auto err = AddStopInstanceTask(*instanceData); !err.IsNone()) {
            LOG_ERR() << "Failed to stop instance" << Log::Field("instance", instance) << Log::Field(err);

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Stop instances end";
}

Error Launcher::AddStopInstanceTask(InstanceData& instanceData)
{
    auto runtime = FindInstanceRuntime(instanceData.mStatus.mRuntimeID);
    if (runtime == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "runtime not found"));
    }

    if (auto err = mLaunchPool.AddTask([this, runtime, &instanceData](void*) {
            if (auto err = StopInstance(runtime, instanceData); !err.IsNone()) {
                LOG_ERR() << "Failed to stop instance" << Log::Field("instance", instanceData.mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));

                SetInstanceState(instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));

                return;
            }

            SetInstanceState(instanceData, InstanceStateEnum::eInactive);
        });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::StopInstance(aos::sm::launcher::RuntimeItf* runtime, InstanceData& instanceData) const
{
    LOG_INF() << "Stop instance" << Log::Field("instance", instanceData.mInfo)
              << Log::Field("runtimeID", instanceData.mInfo.mRuntimeID);

    if (auto err = runtime->StopInstance(instanceData.mInfo, instanceData.mStatus);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Launcher::StopAllInstances()
{
    LOG_INF() << "[profiling] Stop all instances begin" << Log::Field("count", mInstances.Size());

    for (auto& instance : mInstances) {
        if (instance.mInfo.mType != UpdateItemTypeEnum::eService) {
            continue;
        }

        if (auto err = AddStopInstanceTask(instance); !err.IsNone()) {
            LOG_ERR() << "Failed to stop instance" << Log::Field("instance", instance.mInfo) << Log::Field(err);

            SetInstanceState(instance, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Stop all instances end";
}

void Launcher::StopAllNetworks()
{
    LOG_INF() << "[profiling] Stop all networks begin" << Log::Field("count", mInstances.Size());

    auto errBegin = mNetworkManager->BeginBatch();
    if (!errBegin.IsNone()) {
        LOG_ERR() << "Failed to begin network batch" << Log::Field(AOS_ERROR_WRAP(errBegin));
    }

    for (auto& instance : mInstances) {
        if (instance.mInfo.mType != UpdateItemTypeEnum::eService) {
            continue;
        }

        if (auto err = AddStopNetworkTask(instance); !err.IsNone()) {
            LOG_ERR() << "Failed to stop network" << Log::Field("instance", instance.mInfo) << Log::Field(err);

            SetInstanceState(instance, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (errBegin.IsNone()) {
        auto failedIDs = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumInstances>>(mAllocator);
        if (!failedIDs) {
            LOG_ERR() << "Failed to allocate failed network IDs" << Log::Field(ErrorEnum::eNoMemory);

            return;
        }

        if (auto err = mNetworkManager->FlushBatch(*failedIDs); !err.IsNone()) {
            LOG_ERR() << "Can't flush network batch" << Log::Field(AOS_ERROR_WRAP(err));
        }

        if (!failedIDs->IsEmpty()) {
            LOG_WRN() << "Network stop batch partially failed" << Log::Field("count", failedIDs->Size());
        }
    }

    LOG_INF() << "[profiling] Stop all networks end";
}

Error Launcher::PrepareInstance(InstanceData& instanceData)
{
    LOG_DBG() << "Prepare instance" << Log::Field("instance", instanceData.mInfo);

    if (auto err = mStorage->UpdateInstanceInfo(instanceData.mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (instanceData.mInfo.mType != UpdateItemTypeEnum::eService) {
        return ErrorEnum::eNone;
    }

    auto itemConfig = MakeUnique<oci::ItemConfig>(mAllocator);
    if (!itemConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto imageConfig = MakeUnique<oci::ImageConfig>(mAllocator);
    if (!imageConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    // The broken content is repaired by RepairInstances once per item after all instances are prepared.
    if (auto err = LoadInstanceConfigs(instanceData.mInfo, *itemConfig, *imageConfig); !err.IsNone()) {
        return err;
    }

    instanceData.mOfflineTTL    = itemConfig->mOfflineTTL;
    instanceData.mConfigsLoaded = true;

    if (auto err = CreateNetwork(instanceData, *itemConfig, *imageConfig); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

void Launcher::PrepareInstances(const Array<InstanceInfo>& startInstances, const Array<InstallItem>& installItems,
    const Error& installError, Array<InstanceIdent>& preparedInstances)
{
    LOG_INF() << "[profiling] Prepare instances begin" << Log::Field("count", startInstances.Size());

    for (const auto& instance : startInstances) {
        auto instanceData = FindInstanceData(instance);
        // The running instance whose configs failed to load on module start is prepared again to load them.
        auto existingInstance = instanceData && instanceData->mStatus.mState != InstanceStateEnum::eFailed
            && (instanceData->mInfo.mType != UpdateItemTypeEnum::eService || instanceData->mConfigsLoaded);
        Error err;

        if (!instanceData) {
            Tie(instanceData, err) = AddInstanceData(instance);
            if (!err.IsNone()) {
                LOG_ERR() << "Failed to add instance data" << Log::Field("instance", instance)
                          << Log::Field(AOS_ERROR_WRAP(err));

                continue;
            }
        } else {
            SetInstanceState(*instanceData, InstanceStateEnum::eInactive);
        }

        auto installItem = installItems.FindIf([&instance](const auto& item) {
            return item.mID == instance.mItemID && item.mVersion == instance.mVersion;
        });
        err              = installError;
        if (err.IsNone() && installItem != installItems.end()) {
            err = installItem->mError;
        }

        // The instance which is already running doesn't use the item being installed, so it keeps running.
        if (existingInstance) {
            if (!err.IsNone()) {
                LOG_WRN() << "Item install failed, keep running instance" << Log::Field("instance", instance)
                          << Log::Field(err);
            }

            continue;
        }

        if (!err.IsNone()) {
            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, err);

            continue;
        }

        if (err = AddPrepareInstanceTask(*instanceData); !err.IsNone()) {
            LOG_ERR() << "Failed to prepare instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(err));

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));

            continue;
        }

        if (err = preparedInstances.PushBack(static_cast<const InstanceIdent&>(instance)); !err.IsNone()) {
            LOG_ERR() << "Failed to add prepared instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Prepare instances end";
}

Error Launcher::AddPrepareInstanceTask(InstanceData& instanceData)
{
    if (auto err = mLaunchPool.AddTask([this, &instanceData](void*) {
            // The instance which failed the previous prepare is prepared again after its item is repaired.
            SetInstanceState(instanceData, InstanceStateEnum::eInactive);

            if (auto err = PrepareInstance(instanceData); !err.IsNone()) {
                LOG_ERR() << "Failed to prepare instance" << Log::Field("instance", instanceData.mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));

                SetInstanceState(instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
            }
        });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::AddStartNetworkTask(InstanceData& instanceData)
{
    if (auto err = mLaunchPool.AddTask([this, &instanceData](void*) {
            if (auto err = mNetworkManager->StartInstanceNetwork(instanceData.mInstanceID, instanceData.mInfo.mOwnerID);
                !err.IsNone() && !err.Is(ErrorEnum::eAlreadyExist)) {
                LOG_ERR() << "Failed to start network" << Log::Field("instance", instanceData.mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));

                SetInstanceState(instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
            }
        });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Launcher::StartNetworks(const Array<InstanceInfo>& startInstances)
{
    LOG_INF() << "[profiling] Start networks begin" << Log::Field("count", startInstances.Size());

    auto errBegin = mNetworkManager->BeginBatch();
    if (!errBegin.IsNone()) {
        LOG_ERR() << "Failed to begin network batch" << Log::Field(AOS_ERROR_WRAP(errBegin));
    }

    for (const auto& instance : startInstances) {
        auto instanceData = FindInstanceData(instance);
        if (!instanceData) {
            LOG_ERR() << "Failed to start network" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance data not found")));

            continue;
        }

        if (instanceData->mInfo.mType != UpdateItemTypeEnum::eService) {
            continue;
        }

        if (instanceData->mStatus.mState != InstanceStateEnum::eInactive) {
            LOG_ERR() << "Failed to start network" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(Error(ErrorEnum::eWrongState, "instance not inactive")));

            continue;
        }

        if (auto err = AddStartNetworkTask(*instanceData); !err.IsNone()) {
            LOG_ERR() << "Failed to start network" << Log::Field("instance", instanceData->mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (errBegin.IsNone()) {
        auto failedIDs = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumInstances>>(mAllocator);
        if (!failedIDs) {
            LOG_ERR() << "Failed to allocate failed network IDs" << Log::Field(ErrorEnum::eNoMemory);

            return;
        }

        if (auto err = mNetworkManager->FlushBatch(*failedIDs); !err.IsNone()) {
            LOG_ERR() << "Can't flush network batch" << Log::Field(AOS_ERROR_WRAP(err));
        }

        for (const auto& failedID : *failedIDs) {
            auto instanceData = FindInstanceDataByID(failedID);
            if (!instanceData) {
                continue;
            }

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed,
                AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "network batch apply failed")));

            if (auto err
                = mNetworkManager->StopInstanceNetwork(instanceData->mInstanceID, instanceData->mInfo.mOwnerID);
                !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
                LOG_ERR() << "Failed to stop network" << Log::Field("instance", instanceData->mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }

    LOG_INF() << "[profiling] Start networks end";
}

Error Launcher::AddStopNetworkTask(InstanceData& instanceData)
{
    if (auto err = mLaunchPool.AddTask([this, &instanceData](void*) {
            if (auto err = mNetworkManager->StopInstanceNetwork(instanceData.mInstanceID, instanceData.mInfo.mOwnerID);
                !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
                LOG_ERR() << "Failed to stop network" << Log::Field("instance", instanceData.mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));

                SetInstanceState(instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
            }
        });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Launcher::StopNetworks(const Array<InstanceIdent>& stopInstances)
{
    LOG_INF() << "[profiling] Stop networks begin" << Log::Field("count", stopInstances.Size());

    auto errBegin = mNetworkManager->BeginBatch();
    if (!errBegin.IsNone()) {
        LOG_ERR() << "Failed to begin network batch" << Log::Field(AOS_ERROR_WRAP(errBegin));
    }

    for (const auto& instance : stopInstances) {
        auto instanceData = FindInstanceData(instance);
        if (!instanceData) {
            LOG_DBG() << "Instance already removed" << Log::Field("instance", instance);

            continue;
        }

        if (instanceData->mInfo.mType != UpdateItemTypeEnum::eService) {
            continue;
        }

        if (auto err = AddStopNetworkTask(*instanceData); !err.IsNone()) {
            LOG_ERR() << "Failed to stop network" << Log::Field("instance", instanceData->mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (errBegin.IsNone()) {
        auto failedIDs = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumInstances>>(mAllocator);
        if (!failedIDs) {
            LOG_ERR() << "Failed to allocate failed network IDs" << Log::Field(ErrorEnum::eNoMemory);

            return;
        }

        if (auto err = mNetworkManager->FlushBatch(*failedIDs); !err.IsNone()) {
            LOG_ERR() << "Can't flush network batch" << Log::Field(AOS_ERROR_WRAP(err));
        }

        if (!failedIDs->IsEmpty()) {
            LOG_WRN() << "Network stop batch partially failed" << Log::Field("count", failedIDs->Size());
        }
    }

    LOG_INF() << "[profiling] Stop networks end";
}

void Launcher::StartInstances(const Array<InstanceInfo>& startInstances, Array<InstanceIdent>& startedInstances)
{
    LOG_INF() << "[profiling] Start instances begin" << Log::Field("count", startInstances.Size());

    for (const auto& instance : startInstances) {
        auto instanceData = FindInstanceData(instance);
        if (!instanceData) {
            LOG_ERR() << "Failed to start instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance data not found")));

            continue;
        }

        if (instanceData->mStatus.mState != InstanceStateEnum::eInactive) {
            LOG_ERR() << "Failed to start instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(Error(ErrorEnum::eWrongState, "instance not inactive")));

            continue;
        }

        if (auto err = AddStartInstanceTask(*instanceData); !err.IsNone()) {
            LOG_ERR() << "Failed to start instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(err));

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));

            continue;
        }

        if (auto err = startedInstances.PushBack(static_cast<const InstanceIdent&>(instance)); !err.IsNone()) {
            LOG_ERR() << "Failed to add started instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Start instances end";
}

void Launcher::RepairInstances(
    const Array<InstanceIdent>& instances, Array<InstallItem>& repairedItems, AddInstanceTaskFunc addRetryTask)
{
    const auto isBroken = [](const InstanceData& instanceData) {
        return instanceData.mStatus.mState == InstanceStateEnum::eFailed
            && instanceData.mStatus.mError.Is(ErrorEnum::eIntegrity);
    };

    const auto findItem = [&repairedItems](const InstanceInfo& instance) {
        return repairedItems.FindIf([&instance](const InstallItem& item) {
            return item.mID == instance.mItemID && item.mVersion == instance.mVersion;
        });
    };

    // The items repaired by the previous stage of the request are not repaired again.
    const auto firstNewItem = repairedItems.Size();

    {
        LockGuard lock {mMutex};

        for (const auto& instance : instances) {
            auto instanceData = FindInstanceData(instance);
            if (!instanceData || !isBroken(*instanceData) || findItem(instanceData->mInfo) != repairedItems.end()) {
                continue;
            }

            const auto& info = instanceData->mInfo;

            if (auto err = repairedItems.EmplaceBack(
                    InstallItem {{info.mItemID, info.mType, info.mVersion, info.mManifestDigest}, {}});
                !err.IsNone()) {
                LOG_ERR() << "Failed to add repair item" << Log::Field("itemID", info.mItemID)
                          << Log::Field("version", info.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }

    if (repairedItems.Size() == firstNewItem) {
        return;
    }

    LOG_INF() << "[profiling] Repair instances begin" << Log::Field("itemCount", repairedItems.Size() - firstNewItem);

    // Each item is repaired once per request, the instances which still fail stay failed.
    for (auto item = repairedItems.begin() + firstNewItem; item != repairedItems.end(); ++item) {
        if (auto err = mLaunchPool.AddTask([this, item](void*) {
                LOG_WRN() << "Repair update item" << Log::Field("itemID", item->mID)
                          << Log::Field("version", item->mVersion);

                item->mError = InstallUpdateItem(*item);
                if (!item->mError.IsNone()) {
                    LOG_ERR() << "Repair update item failed" << Log::Field("itemID", item->mID)
                              << Log::Field("version", item->mVersion) << Log::Field(AOS_ERROR_WRAP(item->mError));
                }
            });
            !err.IsNone()) {
            item->mError = AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    for (const auto& instance : instances) {
        InstanceData* instanceData = nullptr;

        {
            LockGuard lock {mMutex};

            instanceData = FindInstanceData(instance);
            if (!instanceData || !isBroken(*instanceData)) {
                continue;
            }

            auto item = findItem(instanceData->mInfo);
            if (item == repairedItems.end() || item < repairedItems.begin() + firstNewItem) {
                continue;
            }

            // The instance of an item which can't be repaired fails with the repair error.
            if (!item->mError.IsNone()) {
                instanceData->mStatus.mError = item->mError;

                continue;
            }
        }

        if (auto err = (this->*addRetryTask)(*instanceData); !err.IsNone()) {
            LOG_ERR() << "Failed to retry instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(err));

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Repair instances end";
}

void Launcher::RepairModuleStartInstances()
{
    auto brokenInstances = MakeUnique<StaticArray<InstanceIdent, cMaxNumInstances>>(mAllocator);
    if (!brokenInstances) {
        LOG_ERR() << "Failed to allocate broken instances" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    auto repairedItems = MakeUnique<StaticArray<InstallItem, cMaxNumUpdateItems>>(mAllocator);
    if (!repairedItems) {
        LOG_ERR() << "Failed to allocate repaired items" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    {
        LockGuard lock {mMutex};

        for (const auto& instance : mInstances) {
            if (instance.mStatus.mState != InstanceStateEnum::eFailed
                || !instance.mStatus.mError.Is(ErrorEnum::eIntegrity)) {
                continue;
            }

            if (auto err = brokenInstances->PushBack(static_cast<const InstanceIdent&>(instance.mInfo));
                !err.IsNone()) {
                LOG_ERR() << "Failed to add broken instance" << Log::Field("instance", instance.mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));
            }
        }

        if (brokenInstances->IsEmpty()) {
            return;
        }

        mModuleStartRepair = true;

        if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
            LOG_ERR() << "Can't notify launcher" << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    LOG_INF() << "[profiling] Module start repair begin" << Log::Field("count", brokenInstances->Size());

    if (auto err = mLaunchPool.Run(); err.IsNone()) {
        RepairInstances(*brokenInstances, *repairedItems, &Launcher::AddStartInstanceTask);

        if (err = mLaunchPool.Shutdown(); !err.IsNone()) {
            LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
        }
    } else {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Module start repair end";

    // mModuleStartRepair is reset by FinishLaunch, so a request waiting for the repair starts after the launch is
    // finished.
    LockGuard lock {mMutex};

    SendNodeInstancesStatuses();
}

Error Launcher::AddStartInstanceTask(InstanceData& instanceData)
{
    auto runtime = FindInstanceRuntime(instanceData.mInfo.mRuntimeID);
    if (runtime == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "runtime not found"));
    }

    if (auto err = mLaunchPool.AddTask([this, runtime, &instanceData](void*) {
            SetInstanceState(instanceData, InstanceStateEnum::eActivating);

            // The configs of the instance restored on module start aren't loaded if their read failed. The broken
            // content fails the instance with eIntegrity, so the item is repaired and the instance is started again.
            if (instanceData.mInfo.mType == UpdateItemTypeEnum::eService && !instanceData.mConfigsLoaded) {
                if (auto err = LoadInstanceData(instanceData); err.Is(ErrorEnum::eIntegrity)) {
                    LOG_ERR() << "Failed to start instance" << Log::Field("instance", instanceData.mInfo)
                              << Log::Field(AOS_ERROR_WRAP(err));

                    SetInstanceState(instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));

                    return;
                }
            }

            if (auto err = StartInstance(runtime, instanceData); !err.IsNone()) {
                LOG_ERR() << "Failed to start instance" << Log::Field("instance", instanceData.mInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));

                SetInstanceState(instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));

                return;
            }

            SetInstanceState(instanceData, InstanceStateEnum::eActive);
        });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::StartInstance(aos::sm::launcher::RuntimeItf* runtime, InstanceData& instanceData) const
{
    LOG_INF() << "Start instance" << Log::Field("instance", instanceData.mInfo)
              << Log::Field("runtimeID", instanceData.mInfo.mRuntimeID)
              << Log::Field("manifestDigest", instanceData.mInfo.mManifestDigest);

    if (auto err = runtime->StartInstance(instanceData.mInfo, instanceData.mStatus);
        !err.IsNone() && !err.Is(ErrorEnum::eAlreadyExist)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::AppendInstancesWithModifiedParams(
    const Array<InstanceInfo>& startInstances, Array<InstanceIdent>& stopInstances)
{
    for (const auto& startInstance : startInstances) {
        const auto* const instanceData = FindInstanceData(startInstance);
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

Error Launcher::InstallUpdateItem(const imagemanager::UpdateItemInfo& item)
{
    {
        LockGuard lock {mMutex};

        if (mInstallsCanceled) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eCanceled, "launcher is stopping"));
        }

        mNumInstalls++;
    }

    auto err = mImageManager->InstallItem(item);

    LockGuard lock {mMutex};

    mNumInstalls--;

    if (auto notifyErr = mCondVar.NotifyAll(); !notifyErr.IsNone()) {
        LOG_ERR() << "Can't notify installs" << Log::Field(AOS_ERROR_WRAP(notifyErr));
    }

    return err;
}

void Launcher::CancelInstalls()
{
    {
        LockGuard lock {mMutex};

        mInstallsCanceled = true;
    }

    // An install which passed the check above may reach the image manager after Cancel resets its context, so cancel
    // again until all of them return.
    while (true) {
        if (auto err = mImageManager->Cancel(); !err.IsNone()) {
            LOG_ERR() << "Can't cancel installs" << Log::Field(AOS_ERROR_WRAP(err));
        }

        UniqueLock lock {mMutex};

        if (auto err = mCondVar.Wait(lock, cCancelInstallsPeriod, [this]() { return mNumInstalls == 0; });
            err.IsNone() || !err.Is(ErrorEnum::eTimeout)) {
            return;
        }
    }
}

Error Launcher::StartLaunch()
{
    UniqueLock lock {mMutex};

    // The request received while the module start repair waits for the CM is processed after the repair.
    if (auto err = mCondVar.Wait(lock, [this]() { return !mModuleStartRepair; }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mLaunchInProgress) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    mLaunchInProgress = true;

    return ErrorEnum::eNone;
}

void Launcher::FinishLaunch()
{
    LockGuard lock {mMutex};

    mLaunchInProgress  = false;
    mModuleStartRepair = false;

    if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify launcher" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Launcher::InstanceData* Launcher::FindInstanceData(const InstanceIdent& instanceIdent)
{
    if (auto it = mInstances.FindIf([&instanceIdent](const auto& instance) {
            return static_cast<const InstanceIdent&>(instance.mInfo) == instanceIdent;
        });
        it != mInstances.end()) {
        return it;
    }

    return nullptr;
}

const Launcher::InstanceData* Launcher::FindInstanceData(const InstanceIdent& instanceIdent) const
{
    if (auto it = mInstances.FindIf([&instanceIdent](const auto& instance) {
            return static_cast<const InstanceIdent&>(instance.mInfo) == instanceIdent;
        });
        it != mInstances.end()) {
        return it;
    }

    return nullptr;
}

Launcher::InstanceData* Launcher::FindInstanceDataByID(const String& instanceID)
{
    if (auto it = mInstances.FindIf([&instanceID](const auto& instance) { return instance.mInstanceID == instanceID; });
        it != mInstances.end()) {
        return it;
    }

    return nullptr;
}

RuntimeItf* Launcher::FindInstanceRuntime(const String& runtimeID)
{
    if (auto it = mRuntimes.FindIf([&runtimeID](const auto& it) { return it.mSecond == runtimeID; });
        it != mRuntimes.end()) {
        return it->mFirst;
    }

    return nullptr;
}

const RuntimeItf* Launcher::FindInstanceRuntime(const String& runtimeID) const
{
    if (auto it = mRuntimes.FindIf([&runtimeID](const auto& it) { return it.mSecond == runtimeID; });
        it != mRuntimes.end()) {
        return it->mFirst;
    }

    return nullptr;
}

RuntimeItf* Launcher::FindInstanceRuntime(const InstanceIdent& instanceIdent)
{
    const auto* const instanceData = FindInstanceData(instanceIdent);
    if (!instanceData) {
        return nullptr;
    }

    return FindInstanceRuntime(instanceData->mInfo.mRuntimeID);
}

const RuntimeItf* Launcher::FindInstanceRuntime(const InstanceIdent& instanceIdent) const
{
    const auto* const instanceData = FindInstanceData(instanceIdent);
    if (!instanceData) {
        return nullptr;
    }

    return FindInstanceRuntime(instanceData->mInfo.mRuntimeID);
}

void Launcher::GetRemoveUpdateItems(const Array<InstanceIdent>& stopInstances,
    const Array<InstanceInfo>& startInstances, Array<UpdateItemInfo>& removeItems)
{
    for (const auto& instanceIdent : stopInstances) {
        const auto* const instanceData = FindInstanceData(instanceIdent);
        if (!instanceData) {
            LOG_ERR() << "Instance not found, skip removing update item" << Log::Field("instance", instanceIdent);

            continue;
        }

        if (auto it = startInstances.FindIf([&instanceData](const auto& item) {
                return item.mItemID == instanceData->mInfo.mItemID && item.mVersion == instanceData->mInfo.mVersion;
            });
            it != startInstances.end()) {
            continue;
        }

        if (auto it = removeItems.FindIf([&instanceData](const auto& item) {
                return item.mItemID == instanceData->mInfo.mItemID && item.mVersion == instanceData->mInfo.mVersion;
            });
            it == removeItems.end()) {
            (void)removeItems.EmplaceBack(UpdateItemInfo {instanceData->mInfo.mItemID, instanceData->mInfo.mVersion});
        }
    }
}

void Launcher::RemoveUpdateItems(const Array<UpdateItemInfo>& removeItems)
{
    for (const auto& updateItem : removeItems) {
        if (auto err = mLaunchPool.AddTask([this, &updateItem](void*) {
                if (auto err = mImageManager->RemoveItem(updateItem.mItemID, updateItem.mVersion);
                    !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
                    LOG_ERR() << "Remove update item failed" << Log::Field("itemID", updateItem.mItemID)
                              << Log::Field("version", updateItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
            LOG_ERR() << "Remove update item failed" << Log::Field("itemID", updateItem.mItemID)
                      << Log::Field("version", updateItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Error Launcher::InstallUpdateItems(const Array<InstanceInfo>& startInstances, Array<InstallItem>& installItems)
{
    // The image manager reports all stored item versions, including the ones kept in the cache.
    auto currentItems = MakeUnique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems * 2>>(mAllocator);
    if (!currentItems) {
        LOG_ERR() << "Failed to allocate current items" << Log::Field(ErrorEnum::eNoMemory);

        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mItemStatusProvider->GetItemsStatuses(*currentItems); !err.IsNone()) {
        LOG_ERR() << "Get update items statuses failed" << Log::Field(AOS_ERROR_WRAP(err));

        currentItems->Clear();
    }

    for (const auto& startInstance : startInstances) {
        if (auto it = currentItems->FindIf([&startInstance](const auto& item) {
                return item.mItemID == startInstance.mItemID && item.mVersion == startInstance.mVersion;
            });
            it != currentItems->end() && it->mState == ItemStateEnum::eInstalled) {
            continue;
        }

        if (auto it = installItems.FindIf([&startInstance](const auto& item) {
                return item.mID == startInstance.mItemID && item.mVersion == startInstance.mVersion;
            });
            it == installItems.end()) {
            if (auto err = installItems.EmplaceBack(InstallItem {
                    {startInstance.mItemID, startInstance.mType, startInstance.mVersion, startInstance.mManifestDigest},
                    {}});
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    for (auto& installItem : installItems) {
        if (auto err = mLaunchPool.AddTask([this, &installItem](void*) {
                if (auto err = InstallUpdateItem(installItem); !err.IsNone()) {
                    installItem.mError = AOS_ERROR_WRAP(err);

                    LOG_ERR() << "Install update item failed" << Log::Field("itemID", installItem.mID)
                              << Log::Field("version", installItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
            installItem.mError = AOS_ERROR_WRAP(err);

            LOG_ERR() << "Install update item failed" << Log::Field("itemID", installItem.mID)
                      << Log::Field("version", installItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<Launcher::InstanceData*> Launcher::AddInstanceData(const InstanceInfo& instanceInfo)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Add instance data" << Log::Field("instance", instanceInfo);

    if (auto err = mInstances.EmplaceBack(); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    auto itInstance = &mInstances.Back();

    itInstance->mInfo = instanceInfo;
    static_cast<InstanceIdent&>(itInstance->mStatus)
        = instanceInfo; // NOSONAR cpp:S5912 - intentional copy of InstanceIdent base only
    itInstance->mStatus.mVersion   = instanceInfo.mVersion;
    itInstance->mStatus.mRuntimeID = instanceInfo.mRuntimeID;
    itInstance->mStatus.mState     = InstanceStateEnum::eInactive;

    if (auto err = mInstanceIDProvider->GetInstanceID(instanceInfo, itInstance->mInstanceID); !err.IsNone()) {
        (void)mInstances.Erase(itInstance);

        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    return itInstance;
}

Error Launcher::ReleaseInstance(const InstanceData& instanceData)
{
    LOG_DBG() << "Remove instance" << Log::Field("instance", instanceData.mInfo);

    if (auto err = mStorage->RemoveInstanceInfo(instanceData.mInfo); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNetworkManager->ReleaseInstanceNetwork(instanceData.mInstanceID, instanceData.mInfo.mOwnerID);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Launcher::RemoveInstances(const Array<InstanceIdent>& instances)
{
    for (const auto& instanceIdent : instances) {
        auto instanceData = FindInstanceData(instanceIdent);
        if (!instanceData) {
            LOG_ERR() << "Instance data not found, skip removing" << Log::Field("instance", instanceIdent);

            continue;
        }

        if (auto err = mLaunchPool.AddTask([this, instanceData](void*) {
                if (auto err = ReleaseInstance(*instanceData); !err.IsNone()) {
                    LOG_ERR() << "Failed to remove instance" << Log::Field("instance", instanceData->mInfo)
                              << Log::Field(AOS_ERROR_WRAP(err));

                    SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
            LOG_ERR() << "Failed to remove instance" << Log::Field("instance", instanceData->mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LockGuard lock {mMutex};

    for (const auto& instanceIdent : instances) {
        LOG_DBG() << "Remove instance data" << Log::Field("instance", instanceIdent);

        (void)mInstances.RemoveIf([this, &instanceIdent](const auto& instance) {
            return static_cast<const InstanceIdent&>(instance.mInfo) == instanceIdent;
        });
    }
}

void Launcher::SetInstanceState(InstanceData& instance, const InstanceState& state, const Error& error)
{
    LockGuard lock {mMutex};

    if (instance.mStatus.mState == state) {
        return;
    }

    instance.mStatus.mState = state;
    instance.mStatus.mError = error;
}

Error Launcher::LoadInstanceConfigs(
    const InstanceInfo& instance, oci::ItemConfig& itemConfig, oci::ImageConfig& imageConfig)
{

    auto path = MakeUnique<StaticString<cFilePathLen>>(mAllocator);
    if (!path) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mItemInfoProvider->GetBlobPath(instance.mManifestDigest, *path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto manifest = MakeUnique<oci::ImageManifest>(mAllocator);
    if (!manifest) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mOCISpec->LoadImageManifest(*path, *manifest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!manifest->mItemConfig.HasValue()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "item config not found in manifest"));
    }

    if (auto err = mItemInfoProvider->GetBlobPath(manifest->mItemConfig->mDigest, *path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadItemConfig(*path, itemConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mItemInfoProvider->GetBlobPath(manifest->mConfig.mDigest, *path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadImageConfig(*path, imageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetInstanceNetworkConfig(const InstanceInfo& instance, const oci::ItemConfig& itemConfig,
    const oci::ImageConfig& imageConfig, networkmanager::InstanceNetworkConfig& networkConfig)
{
    networkConfig.mInstanceIdent = static_cast<const InstanceIdent&>(instance);

    auto resourceInfo = MakeUnique<resourcemanager::ResourceInfo>(mAllocator);
    if (!resourceInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    for (const auto& resource : itemConfig.mResources) {

        if (auto err = mResourceInfoProvider->GetResourceInfo(resource.mName, *resourceInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = networkConfig.mHosts.Insert(
                networkConfig.mHosts.end(), resourceInfo->mHosts.begin(), resourceInfo->mHosts.end());
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (itemConfig.mHostname.HasValue()) {
        networkConfig.mHostname = *itemConfig.mHostname;
    }

    if (itemConfig.mQuotas.mDownloadSpeed.HasValue()) {
        networkConfig.mIngressKbit = *itemConfig.mQuotas.mDownloadSpeed;
    }

    if (itemConfig.mQuotas.mUploadSpeed.HasValue()) {
        networkConfig.mEgressKbit = *itemConfig.mQuotas.mUploadSpeed;
    }

    if (itemConfig.mQuotas.mDownloadLimit.HasValue()) {
        networkConfig.mDownloadLimit = *itemConfig.mQuotas.mDownloadLimit;
    }

    if (itemConfig.mQuotas.mUploadLimit.HasValue()) {
        networkConfig.mUploadLimit = *itemConfig.mQuotas.mUploadLimit;
    }

    if (auto err = networkConfig.mExposedPorts.Assign(imageConfig.mConfig.mExposedPorts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = networkConfig.mAllowedConnections.Assign(itemConfig.mAllowedConnections); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::CreateNetwork(
    const InstanceData& instanceData, const oci::ItemConfig& itemConfig, const oci::ImageConfig& imageConfig)
{
    auto networkConfig = MakeUnique<networkmanager::InstanceNetworkConfig>(mAllocator);
    if (!networkConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = GetInstanceNetworkConfig(instanceData.mInfo, itemConfig, imageConfig, *networkConfig);
        !err.IsNone()) {
        return err;
    }

    if (auto err
        = mNetworkManager->CreateInstanceNetwork(instanceData.mInstanceID, instanceData.mInfo.mOwnerID, *networkConfig);
        !err.IsNone() && !err.Is(ErrorEnum::eAlreadyExist)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
