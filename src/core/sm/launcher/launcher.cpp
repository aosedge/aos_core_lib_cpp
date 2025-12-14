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

Error Launcher::Init(const Array<RuntimeItf*>& runtimes, SenderItf& sender, StorageItf& storage)
{
    LOG_INF() << "Initializing launcher";

    for (auto* runtime : runtimes) {
        if (auto err = mRuntimes.Set(runtime, ""); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    mStorage = &storage;
    mSender  = &sender;

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    UniqueLock lock {mMutex};

    LOG_INF() << "Start launcher";

    for (auto& it : mRuntimes) {
        if (auto err = it.mFirst->Start(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

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

    auto storedInstances = MakeUnique<InstanceInfoArray>(&mAllocator);

    if (auto err = mStorage->GetAllInstancesInfos(*storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mIsRunning = true;

    if (auto err = mRebootThread.Run([this](void*) { RunRebootThread(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    lock.Unlock();

    if (auto err = UpdateInstances({}, *storedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    {
        UniqueLock lock {mMutex};

        LOG_INF() << "Stop launcher";

        mCondVar.Wait(lock, [this]() { return !mLaunchInProgress; });

        for (auto& it : mRuntimes) {
            if (auto err = it.mFirst->Stop(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        mIsRunning = false;

        mCondVar.NotifyAll();
    }

    mThread.Join();
    mRebootThread.Join();

    return ErrorEnum::eNone;
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
    LockGuard lock {mMutex};

    LOG_DBG() << "Instances statuses received" << Log::Field("count", statuses.Size());
    for (const auto& status : statuses) {
        LOG_DBG() << "Instance status received" << Log::Field("ident", static_cast<const InstanceIdent&>(status))
                  << Log::Field("runtimeID", status.mRuntimeID) << Log::Field("state", status.mState)
                  << Log::Field("error", status.mError);
    }

    for (auto* subscriber : mSubscribers) {
        subscriber->OnInstancesStatusesChanged(statuses);
    }

    return mSender->SendUpdateInstancesStatuses(statuses);
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

    LOG_DBG() << "Get instances statuses";

    for (const auto& instance : mInstances) {
        if (auto err = statuses.EmplaceBack(instance.mStatus); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::SubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mMutex};

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
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe instance status listener";

    return mSubscribers.Remove(&listener) == 0 ? AOS_ERROR_WRAP(ErrorEnum::eNotFound) : ErrorEnum::eNone;
}

Error Launcher::GetInstanceMonitoringParams(
    const InstanceIdent& instanceIdent, Optional<InstanceMonitoringParams>& params) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get instance monitoring params" << Log::Field("ident", instanceIdent);

    if (auto instanceData = FindInstanceData(instanceIdent); instanceData) {
        params = instanceData->mInfo.mMonitoringParams;

        return ErrorEnum::eNone;
    }

    return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "Instance not found"));
}

Error Launcher::GetInstanceMonitoringData(
    const InstanceIdent& instanceIdent, monitoring::InstanceMonitoringData& monitoringData)
{
    LockGuard lock {mMutex};

    const auto* const instanceData = FindInstanceData(instanceIdent);
    if (instanceData == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "Instance not found"));
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

void Launcher::UpdateInstancesImpl(const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances)
{
    LOG_INF() << "Update instances" << Log::Field("stopCount", stopInstances.Size())
              << Log::Field("startCount", startInstances.Size());

    auto statuses = MakeUnique<InstanceStatusArray>(&mAllocator);

    StopInstances(stopInstances, *statuses);
    ClearCachedInstances();
    StartInstances(startInstances);

    for (const auto& instance : mInstances) {
        if (auto err = statuses->EmplaceBack(instance.mStatus); !err.IsNone()) {
            LOG_ERR() << "Failed to add instance status to statuses array"
                      << Log::Field("ident", static_cast<const InstanceIdent&>(instance.mStatus))
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (!statuses->IsEmpty()) {
        if (auto err = mSender->SendNodeInstancesStatuses(*statuses); !err.IsNone()) {
            LOG_ERR() << "Failed to send node instances statuses" << Log::Field(err);
        }
    }
}

void Launcher::StopInstances(const Array<InstanceIdent>& stopInstances, Array<InstanceStatus>& statuses)
{
    if (stopInstances.IsEmpty()) {
        LOG_DBG() << "No instances to stop";

        return;
    }

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool to stop instances" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    for (const auto& instance : stopInstances) {
        LOG_DBG() << "Stop instance" << Log::Field("ident", instance);

        if (auto err = statuses.EmplaceBack(); !err.IsNone()) {
            LOG_ERR() << "Stop instance failed" << Log::Field("ident", instance) << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }

        static_cast<InstanceIdent&>(statuses.Back()) = instance;

        auto itInstance = FindInstanceData(instance);
        if (itInstance == mInstances.end()) {
            statuses.Back().mState = InstanceStateEnum::eFailed;
            statuses.Back().mError = AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "Instance not found"));

            continue;
        }

        statuses.Back() = itInstance->mStatus;

        auto runtime = FindInstanceRuntime(itInstance->mStatus.mRuntimeID);
        if (runtime == nullptr) {
            statuses.Back().mState = InstanceStateEnum::eFailed;
            statuses.Back().mError = AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "Runtime not found"));

            continue;
        }

        if (auto errAddTask = mLaunchPool.AddTask([this, runtime, &instance, status = &statuses.Back()](void*) {
                if (auto err = runtime->StopInstance(instance, *status); !err.IsNone()) {
                    LOG_ERR() << "Failed to stop instance" << Log::Field("ident", instance)
                              << Log::Field(AOS_ERROR_WRAP(err));

                    if (status->mState != InstanceStateEnum::eFailed) {
                        status->mState = InstanceStateEnum::eFailed;
                        status->mError = AOS_ERROR_WRAP(err);
                    }
                }
            });
            !errAddTask.IsNone()) {
            LOG_ERR() << "Stop instance failed" << Log::Field("ident", instance)
                      << Log::Field(AOS_ERROR_WRAP(errAddTask));

            continue;
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (auto err = mLaunchPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

void Launcher::StartInstances(const Array<InstanceInfo>& startInstances)
{
    if (startInstances.IsEmpty()) {
        LOG_DBG() << "No instances to start";

        return;
    }

    for (const auto& instance : startInstances) {
        if (auto err = mInstances.EmplaceBack(); !err.IsNone()) {
            LOG_ERR() << "Start instance failed" << Log::Field("instance", static_cast<const InstanceIdent&>(instance))
                      << Log::Field(AOS_ERROR_WRAP(err));
        }

        mInstances.Back().mInfo                                = instance;
        static_cast<InstanceIdent&>(mInstances.Back().mStatus) = static_cast<const InstanceIdent&>(instance);
        mInstances.Back().mStatus.mRuntimeID                   = instance.mRuntimeID;
        mInstances.Back().mStatus.mState                       = InstanceStateEnum::eInactive;
    }

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Start instances failed" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    for (auto& instance : mInstances) {
        auto runtime = FindInstanceRuntime(instance.mInfo.mRuntimeID);
        if (runtime == nullptr) {
            instance.mStatus.mState = InstanceStateEnum::eFailed;
            instance.mStatus.mError = AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "Runtime not found"));

            continue;
        }

        if (auto err = mLaunchPool.AddTask([this, runtime, &instance](void*) { StartInstance(*runtime, instance); });
            !err.IsNone()) {
            LOG_ERR() << "Start instance failed"
                      << Log::Field("instance", static_cast<const InstanceIdent&>(instance.mInfo))
                      << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (auto err = mLaunchPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

void Launcher::StartInstance(RuntimeItf& runtime, InstanceData& instance)
{
    LOG_DBG() << "Start instance" << Log::Field("instance", static_cast<const InstanceIdent&>(instance.mInfo));

    instance.mStatus.mState = InstanceStateEnum::eActivating;

    if (auto err = runtime.StartInstance(instance.mInfo, instance.mStatus); !err.IsNone()) {
        LOG_ERR() << "Start instance failed"
                  << Log::Field("instance", static_cast<const InstanceIdent&>(instance.mInfo)) << Log::Field(err);

        if (instance.mStatus.mState != InstanceStateEnum::eFailed) {
            instance.mStatus.mState = InstanceStateEnum::eFailed;
            instance.mStatus.mError = AOS_ERROR_WRAP(err);
        }

        return;
    }

    if (auto err = mStorage->AddInstanceInfo(instance.mInfo); !err.IsNone() && !err.Is(ErrorEnum::eAlreadyExist)) {
        LOG_ERR() << "Start instance failed"
                  << Log::Field("instance", static_cast<const InstanceIdent&>(instance.mInfo)) << Log::Field(err);

        if (instance.mStatus.mState != InstanceStateEnum::eFailed) {
            instance.mStatus.mState = InstanceStateEnum::eFailed;
            instance.mStatus.mError = AOS_ERROR_WRAP(err);
        }
    }
}

void Launcher::ClearCachedInstances()
{
    LockGuard lock {mMutex};

    while (!mInstances.IsEmpty()) {
        if (auto err = mStorage->RemoveInstanceInfo(static_cast<const InstanceIdent&>(mInstances.Back().mInfo));
            !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
            LOG_ERR() << "Remove instance info failed"
                      << Log::Field("ident", static_cast<const InstanceIdent&>(mInstances.Back().mInfo))
                      << Log::Field(AOS_ERROR_WRAP(err));
        }

        mInstances.PopBack();
    }
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
    if (instanceData == nullptr) {
        return nullptr;
    }

    return FindInstanceRuntime(instanceData->mInfo.mRuntimeID);
}

RuntimeItf* Launcher::FindInstanceRuntime(const InstanceIdent& instanceIdent) const
{
    return const_cast<Launcher*>(this)->FindInstanceRuntime(instanceIdent);
}

} // namespace aos::sm::launcher
