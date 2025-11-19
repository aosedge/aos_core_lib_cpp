/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "launcher.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(const Config& config, StorageItf& storage, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    InstanceRunnerItf& runner, ImageInfoProviderItf& imageInfoProvider,
    unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
    networkmanager::NetworkManagerItf& networkManager, MonitoringProviderItf& monitorProvider)
{
    LOG_DBG() << "Init Launcher";

    mConfig             = config;
    mStorage            = &storage;
    mNodeInfoProvider   = &nodeInfoProvider;
    mRunner             = &runner;
    mImageInfoProvider  = &imageInfoProvider;
    mNodeConfigProvider = &nodeConfigProvider;
    mStorageState       = &storageState;
    mNetworkManager     = &networkManager;
    mMonitorProvider    = &monitorProvider;

    mInstanceManager.Init(config, storage, imageInfoProvider, storageState);
    mNodeManager.Init(*mNodeInfoProvider, *mNodeConfigProvider, *mStorageState, *mRunner);
    mBalancer.Init(mInstanceManager, *mImageInfoProvider, mNodeManager, *mMonitorProvider, *mRunner, *mNetworkManager);

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LOG_DBG() << "Start Launcher";

    if (auto err = mInstanceManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.UpdateNodeInstances(mInstanceManager.GetActiveInstances()); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners();

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    LOG_DBG() << "Stop Launcher";

    if (auto err = mInstanceManager.Stop(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.Stop(); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error Launcher::RunInstances(const Array<RunInstanceRequest>& requests)
{
    LOG_DBG() << "Run instances";

    LockGuard lock {mMutex};

    if (auto err = mRunRequests.Assign(requests); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    UpdateData(false);

    if (auto err = mBalancer.RunInstances(mRunRequests, false); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners();

    return ErrorEnum::eNone;
}

Error Launcher::Rebalance()
{
    LOG_DBG() << "Rebalance instances";

    LockGuard lock {mMutex};

    UpdateData(true);

    if (auto err = mBalancer.RunInstances(mRunRequests, true); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners();

    return ErrorEnum::eNone;
}

Error Launcher::GetInstancesStatuses(Array<InstanceStatus>& statuses)
{
    LockGuard lock {mMutex};

    statuses.Clear();

    for (const auto& instance : mInstanceManager.GetActiveInstances()) {
        if (auto err = statuses.PushBack(instance->GetStatus()); !err.IsNone()) {
            LOG_ERR() << "Failed to push new instance status" << Log::Field(AOS_ERROR_WRAP(err));

            break;
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::SubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mMutex};

    if (auto err = mInstanceStatusListeners.PushBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::UnsubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mMutex};

    auto count = mInstanceStatusListeners.Remove(&listener);

    return count == 0 ? AOS_ERROR_WRAP(ErrorEnum::eNotFound) : ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Launcher::NotifyInstanceStatusListeners()
{
    auto statuses = MakeUnique<StaticArray<InstanceStatus, cMaxNumInstances>>(&mAllocator);

    for (const auto& instance : mInstanceManager.GetActiveInstances()) {
        if (auto err = statuses->PushBack(instance->GetStatus()); !err.IsNone()) {
            LOG_ERR() << "Failed to push new instance status" << Log::Field(AOS_ERROR_WRAP(err));

            break;
        }
    }

    for (auto& listener : mInstanceStatusListeners) {
        listener->OnInstancesStatusesChanged(*statuses);
    }
}

void Launcher::UpdateData(bool rebalancing)
{
    for (auto& node : mNodeManager.GetNodes()) {
        const auto& nodeID = node.GetInfo().mNodeID;

        auto nodeMonitoring = MakeUnique<monitoring::NodeMonitoringData>(&mAllocator);

        if (auto err = mMonitorProvider->GetAverageMonitoring(nodeID, *nodeMonitoring); !err.IsNone()) {
            mInstanceManager.UpdateMonitoringData(nodeMonitoring->mInstances);
        }

        node.UpdateAvailableResources(*nodeMonitoring, rebalancing);
        node.UpdateConfig();
    }
}

Error Launcher::OnInstanceStatusReceived(const InstanceStatus& status)
{
    if (auto err = mInstanceManager.UpdateStatus(status); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners();

    return ErrorEnum::eNone;
}

Error Launcher::OnNodeInstancesStatusesReceived(const String& nodeID, const Array<InstanceStatus>& statuses)
{
    (void)nodeID;

    Error firstErr;
    for (const auto& status : statuses) {
        if (auto err = mInstanceManager.UpdateStatus(status); !err.IsNone()) {
            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }
    }

    if (auto err = mNodeManager.UpdateNodeInstances(mInstanceManager.GetActiveInstances()); !err.IsNone()) {
        if (firstErr.IsNone()) {
            firstErr = err;
        }
    }

    if (!firstErr.IsNone()) {
        return firstErr;
    }

    NotifyInstanceStatusListeners();

    return ErrorEnum::eNone;
}

Error Launcher::OnEnvVarsStatusesReceived(const String& nodeID, const Array<EnvVarsInstanceStatus>& statuses)
{
    (void)nodeID;
    (void)statuses;

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
