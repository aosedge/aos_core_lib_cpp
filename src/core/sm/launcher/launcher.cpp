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

Error Launcher::Init(const Array<RuntimeItf*>& runtimes, imagemanager::ImageManagerItf& imageManager, SenderItf& sender,
    StorageItf& storage, oci::OCISpecItf& ociSpec, imagemanager::ItemInfoProviderItf& itemInfoProvider,
    cloudconnection::CloudConnectionItf& cloudConnection, networkmanager::NetworkManagerItf& networkManager,
    InstanceIDProviderItf& instanceIDProvider, resourcemanager::ResourceInfoProviderItf& resourceInfoProvider)
{
    LOG_DBG() << "Init launcher";

    for (auto* runtime : runtimes) {
        if (auto err = mRuntimes.Set(runtime, ""); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    mImageManager         = &imageManager;
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

    mIsRunning = true;

    if (auto err = mRebootThread.Run([this](void*) { RunRebootThread(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto storedInstances = MakeUnique<InstanceInfoArray>(&mAllocator);

    if (auto err = mStorage->GetAllInstancesInfos(*storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    lock.Unlock();

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

    {
        UniqueLock lock {mMutex};

        LOG_DBG() << "Stop launcher";

        mCondVar.Wait(lock, [this]() { return !mLaunchInProgress; });

        lock.Unlock();

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
            if (err = it.mFirst->Stop(); !err.IsNone() && stopErr.IsNone()) {
                stopErr = AOS_ERROR_WRAP(err);
            }
        }

        lock.Lock();

        mIsRunning = false;

        mCondVar.NotifyAll();
    }

    mThread.Join();
    mRebootThread.Join();
    mOfflineTTLHandler.Stop(Timer::StopMode::WaitForCallbacks);

    return stopErr;
}

Error Launcher::UpdateInstances(const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances)
{
    if (auto err = StartLaunch(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // Wait in case previous request is not yet finished
    mThread.Join();

    auto stop  = MakeShared<StaticArray<InstanceIdent, cMaxNumInstances>>(&mAllocator, stopInstances);
    auto start = MakeShared<InstanceInfoArray>(&mAllocator, startInstances);

    if (auto err = mThread.Run([this, stop, start](void*) {
            UpdateInstancesImpl(*stop, *start);
            FinishLaunch();
        });
        !err.IsNone()) {
        FinishLaunch();

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

    mCondVar.NotifyAll();

    return ErrorEnum::eNone;
}

Error Launcher::GetInstancesStatuses(Array<InstanceStatus>& statuses)
{
    UniqueLock lock {mMutex};

    mCondVar.Wait(lock, [this]() { return !mLaunchInProgress; });

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

void Launcher::RunRebootThread()
{
    while (true) {
        StaticArray<RuntimeItf*, cMaxNumNodeRuntimes> runtimesToReboot;

        {
            UniqueLock lock {mMutex};

            mCondVar.Wait(lock, [this]() { return !mIsRunning || (!mLaunchInProgress && !mRebootQueue.IsEmpty()); });

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

    mCondVar.Wait(lock, [this]() { return !mIsRunning || !mLaunchInProgress; });

    if (!mIsRunning || !mOfflineTime.HasValue()) {
        return;
    }

    mLaunchInProgress = true;

    StopExpiredInstances(lock);

    mLaunchInProgress = false;
    mCondVar.NotifyAll();

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

    lock.Unlock();

    if (auto err = mOfflineTTLPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Offline TTL thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    lock.Lock();
}

void Launcher::SendNodeInstancesStatuses()
{
    LOG_INF() << "Send node instances statuses" << Log::Field("count", mInstances.Size());

    auto statuses = MakeUnique<InstanceStatusArray>(&mAllocator);

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

    auto instanceInfo = MakeUnique<InstanceInfo>(&mAllocator);

    static_cast<InstanceIdent&>(*instanceInfo) = status;
    instanceInfo->mRuntimeID                   = status.mRuntimeID;
    instanceInfo->mType                        = status.mType;
    instanceInfo->mVersion                     = status.mVersion;
    instanceInfo->mManifestDigest              = status.mManifestDigest;

    if (auto err = mStorage->UpdateInstanceInfo(*instanceInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::LoadInstanceData(InstanceData& instanceData)
{
    auto itemConfig  = MakeUnique<oci::ItemConfig>(&mAllocator);
    auto imageConfig = MakeUnique<oci::ImageConfig>(&mAllocator);

    if (auto err = GetInstanceConfigs(instanceData.mInfo, *itemConfig, *imageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    instanceData.mOfflineTTL = itemConfig->mOfflineTTL;

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

        if (err = mLaunchPool.AddTask([this, instanceData](void*) {
                if (auto err = LoadInstanceData(*instanceData); !err.IsNone()) {
                    LOG_ERR() << "Failed to load instance data" << Log::Field("instance", instanceData->mInfo)
                              << Log::Field(AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
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

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    if (auto err = AppendInstancesWithModifiedParams(startInstances, stopInstances); !err.IsNone()) {
        LOG_ERR() << "Failed to append instances with modified params to stop list" << Log::Field(AOS_ERROR_WRAP(err));
    }

    auto removeItems = MakeUnique<StaticArray<UpdateItemInfo, cMaxNumUpdateItems>>(&mAllocator);

    if (!mFirstStart) {
        GetRemoveUpdateItems(stopInstances, startInstances, *removeItems);
    }

    StopInstances(stopInstances);
    StopNetworks(stopInstances);
    RemoveInstances(stopInstances);

    if (!mFirstStart) {
        RemoveUpdateItems(*removeItems);
        InstallUpdateItems(startInstances);
        PrepareInstances(startInstances);
    }

    StartNetworks(startInstances);
    StartInstances(startInstances);

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

Error Launcher::StopInstance(aos::sm::launcher::RuntimeItf* runtime, InstanceData& instanceData)
{
    LOG_INF() << "Stop instance" << Log::Field("instance", instanceData.mInfo)
              << Log::Field("runtimeID", instanceData.mInfo.mRuntimeID);

    if (auto err = runtime->StopInstance(instanceData.mInfo, instanceData.mStatus); !err.IsNone()) {
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

    for (auto& instance : mInstances) {
        if (instance.mInfo.mType != UpdateItemTypeEnum::eService) {
            continue;
        }

        if (auto err = AddStopNetworkTask(instance); !err.IsNone()) {
            LOG_ERR() << "Failed to stop network" << Log::Field("instance", instance.mInfo) << Log::Field(err);

            SetInstanceState(instance, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
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

    auto itemConfig  = MakeUnique<oci::ItemConfig>(&mAllocator);
    auto imageConfig = MakeUnique<oci::ImageConfig>(&mAllocator);

    if (auto err = GetInstanceConfigs(instanceData.mInfo, *itemConfig, *imageConfig); !err.IsNone()) {
        return err;
    }

    instanceData.mOfflineTTL = itemConfig->mOfflineTTL;

    if (auto err = CreateNetwork(instanceData, *itemConfig, *imageConfig); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

void Launcher::PrepareInstances(const Array<InstanceInfo>& startInstances)
{
    for (const auto& instance : startInstances) {
        auto instanceData = FindInstanceData(instance);
        if (instanceData) {
            LOG_WRN() << "Instance data already exists" << Log::Field("instance", instance);

            SetInstanceState(*instanceData, InstanceStateEnum::eInactive);

            continue;
        }

        Error err;

        Tie(instanceData, err) = AddInstanceData(instance);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add instance data" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }

        if (err = mLaunchPool.AddTask([this, instanceData](void*) {
                if (auto err = PrepareInstance(*instanceData); !err.IsNone()) {
                    LOG_ERR() << "Failed to start instance" << Log::Field("instance", instanceData->mInfo)
                              << Log::Field(AOS_ERROR_WRAP(err));

                    SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
            LOG_ERR() << "Failed to prepare instance" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(err));

            SetInstanceState(*instanceData, InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Error Launcher::AddStartNetworkTask(InstanceData& instanceData)
{
    if (auto err = mLaunchPool.AddTask([this, &instanceData](void*) {
            if (auto err = mNetworkManager->StartInstanceNetwork(instanceData.mInstanceID, instanceData.mInfo.mOwnerID);
                !err.IsNone()) {
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

    LOG_INF() << "[profiling] Start networks end";
}

Error Launcher::AddStopNetworkTask(InstanceData& instanceData)
{
    if (auto err = mLaunchPool.AddTask([this, &instanceData](void*) {
            if (auto err = mNetworkManager->StopInstanceNetwork(instanceData.mInstanceID, instanceData.mInfo.mOwnerID);
                !err.IsNone()) {
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

    for (const auto& instance : stopInstances) {
        auto instanceData = FindInstanceData(instance);
        if (!instanceData) {
            LOG_ERR() << "Failed to stop network" << Log::Field("instance", instance)
                      << Log::Field(AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance data not found")));

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

    LOG_INF() << "[profiling] Stop networks end";
}

void Launcher::StartInstances(const Array<InstanceInfo>& startInstances)
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
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Start instances end";
}

Error Launcher::AddStartInstanceTask(InstanceData& instanceData)
{
    auto runtime = FindInstanceRuntime(instanceData.mInfo.mRuntimeID);
    if (runtime == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "runtime not found"));
    }

    if (auto err = mLaunchPool.AddTask([this, runtime, &instanceData](void*) {
            SetInstanceState(instanceData, InstanceStateEnum::eActivating);

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

Error Launcher::StartInstance(aos::sm::launcher::RuntimeItf* runtime, InstanceData& instanceData)
{
    LOG_INF() << "Start instance" << Log::Field("instance", instanceData.mInfo)
              << Log::Field("runtimeID", instanceData.mInfo.mRuntimeID)
              << Log::Field("manifestDigest", instanceData.mInfo.mManifestDigest);

    if (auto err = runtime->StartInstance(instanceData.mInfo, instanceData.mStatus); !err.IsNone()) {
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

        if (auto err = stopInstances.EmplaceBack(startInstance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::StartLaunch()
{
    LockGuard lock {mMutex};

    if (mLaunchInProgress) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    mLaunchInProgress = true;

    return ErrorEnum::eNone;
}

void Launcher::FinishLaunch()
{
    LockGuard lock {mMutex};

    mLaunchInProgress = false;
    mCondVar.NotifyAll();
}

Launcher::InstanceData* Launcher::FindInstanceData(const InstanceIdent& instanceIdent)
{
    auto it = mInstances.FindIf([&instanceIdent](const auto& instance) {
        return static_cast<const InstanceIdent&>(instance.mInfo) == instanceIdent;
    });

    if (it != mInstances.end()) {
        return it;
    }

    return nullptr;
}

Launcher::InstanceData* Launcher::FindInstanceData(const InstanceIdent& instanceIdent) const
{
    return const_cast<Launcher*>(this)->FindInstanceData(instanceIdent);
}

RuntimeItf* Launcher::FindInstanceRuntime(const String& runtimeID)
{
    auto it = mRuntimes.FindIf([&runtimeID](const auto& it) { return it.mSecond == runtimeID; });
    if (it != mRuntimes.end()) {
        return it->mFirst;
    }

    return nullptr;
}

RuntimeItf* Launcher::FindInstanceRuntime(const String& runtimeID) const
{
    return const_cast<Launcher*>(this)->FindInstanceRuntime(runtimeID);
}

RuntimeItf* Launcher::FindInstanceRuntime(const InstanceIdent& instanceIdent)
{
    const auto* const instanceData = FindInstanceData(instanceIdent);
    if (!instanceData) {
        return nullptr;
    }

    return FindInstanceRuntime(instanceData->mInfo.mRuntimeID);
}

RuntimeItf* Launcher::FindInstanceRuntime(const InstanceIdent& instanceIdent) const
{
    return const_cast<Launcher*>(this)->FindInstanceRuntime(instanceIdent);
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
            removeItems.EmplaceBack(UpdateItemInfo {instanceData->mInfo.mItemID, instanceData->mInfo.mVersion});
        }
    }
}

void Launcher::RemoveUpdateItems(const Array<UpdateItemInfo>& removeItems)
{
    for (const auto& updateItem : removeItems) {
        if (auto err = mLaunchPool.AddTask([this, &updateItem](void*) {
                if (auto err = mImageManager->RemoveUpdateItem(updateItem.mItemID, updateItem.mVersion);
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

void Launcher::InstallUpdateItems(const Array<InstanceInfo>& startInstances)
{
    auto currentItems = MakeUnique<StaticArray<imagemanager::UpdateItemStatus, cMaxNumUpdateItems>>(&mAllocator);
    auto installItems = MakeUnique<StaticArray<imagemanager::UpdateItemInfo, cMaxNumUpdateItems>>(&mAllocator);

    if (auto err = mImageManager->GetAllInstalledItems(*currentItems); !err.IsNone()) {
        LOG_ERR() << "Get update items statuses failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    for (const auto& startInstance : startInstances) {
        if (auto it = currentItems->FindIf([&startInstance](const auto& item) {
                return item.mID == startInstance.mItemID && item.mVersion == startInstance.mVersion;
            });
            it != currentItems->end() && it->mState == ItemStateEnum::eInstalled) {
            continue;
        }

        if (auto it = installItems->FindIf([&startInstance](const auto& item) {
                return item.mID == startInstance.mItemID && item.mVersion == startInstance.mVersion;
            });
            it == installItems->end()) {
            installItems->EmplaceBack(imagemanager::UpdateItemInfo {
                startInstance.mItemID, startInstance.mType, startInstance.mVersion, startInstance.mManifestDigest});
        }
    }

    for (const auto& installItem : *installItems) {
        if (auto err = mLaunchPool.AddTask([this, installItem](void*) {
                if (auto err = mImageManager->InstallUpdateItem(installItem); !err.IsNone()) {
                    LOG_ERR() << "Install update item failed" << Log::Field("itemID", installItem.mID)
                              << Log::Field("version", installItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
            LOG_ERR() << "Install update item failed" << Log::Field("itemID", installItem.mID)
                      << Log::Field("version", installItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

RetWithError<Launcher::InstanceData*> Launcher::AddInstanceData(const InstanceInfo& instanceInfo)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Add instance data" << Log::Field("instance", instanceInfo);

    if (auto err = mInstances.EmplaceBack(); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    auto itInstance = &mInstances.Back();

    itInstance->mInfo                                = instanceInfo;
    static_cast<InstanceIdent&>(itInstance->mStatus) = instanceInfo;
    itInstance->mStatus.mVersion                     = instanceInfo.mVersion;
    itInstance->mStatus.mRuntimeID                   = instanceInfo.mRuntimeID;
    itInstance->mStatus.mState                       = InstanceStateEnum::eInactive;

    if (auto err = mInstanceIDProvider->GetInstanceID(instanceInfo, itInstance->mInstanceID); !err.IsNone()) {
        mInstances.Erase(itInstance);

        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    return itInstance;
}

Error Launcher::ReleaseInstance(const InstanceData& instanceData)
{
    LOG_DBG() << "Remove instance" << Log::Field("instance", instanceData.mInfo);

    if (auto err = mStorage->RemoveInstanceInfo(instanceData.mInfo); !err.IsNone()) {
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

        if (instanceData->mStatus.mState != InstanceStateEnum::eInactive) {
            LOG_ERR() << "Instance is not inactive, skip removing" << Log::Field("instance", instanceIdent)
                      << Log::Field("state", instanceData->mStatus.mState);

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

        mInstances.RemoveIf([this, &instanceIdent](const auto& instance) {
            if (static_cast<const InstanceIdent&>(instance.mInfo) != instanceIdent) {
                return false;
            }

            if (instance.mStatus.mState != InstanceStateEnum::eInactive) {
                LOG_ERR() << "Instance is not inactive, skip removing" << Log::Field("instance", instanceIdent)
                          << Log::Field("state", instance.mStatus.mState);

                return false;
            }

            return true;
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

Error Launcher::GetInstanceConfigs(
    const InstanceInfo& instance, oci::ItemConfig& itemConfig, oci::ImageConfig& imageConfig)
{
    LOG_DBG() << "Get instance configs" << Log::Field("instance", instance);

    auto path = MakeUnique<StaticString<cFilePathLen>>(&mAllocator);

    if (auto err = mItemInfoProvider->GetBlobPath(instance.mManifestDigest, *path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto manifest = MakeUnique<oci::ImageManifest>(&mAllocator);

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

    auto resourceInfo = MakeUnique<resourcemanager::ResourceInfo>(&mAllocator);

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
    auto networkConfig = MakeUnique<networkmanager::InstanceNetworkConfig>(&mAllocator);

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
