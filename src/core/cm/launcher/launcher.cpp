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
    InstanceRunnerItf& runner, imagemanager::ItemInfoProviderItf& itemInfoProvider,
    imagemanager::BlobInfoProviderItf& blobInfoProvider, oci::OCISpecItf& ociSpec,
    unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
    networkmanager::NetworkManagerItf& networkManager, MonitoringProviderItf& monitorProvider, IDValidator gidValidator,
    IDValidator uidValidator)
{
    LOG_DBG() << "Init Launcher";

    mConfig             = config;
    mStorage            = &storage;
    mNodeInfoProvider   = &nodeInfoProvider;
    mRunner             = &runner;
    mNodeConfigProvider = &nodeConfigProvider;
    mStorageState       = &storageState;
    mNetworkManager     = &networkManager;
    mMonitorProvider    = &monitorProvider;

    auto err = mInstanceManager.Init(
        config, storage, storageState, itemInfoProvider, blobInfoProvider, ociSpec, gidValidator, uidValidator);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mNodeManager.Init(*mNodeInfoProvider, *mNodeConfigProvider, *mStorageState, *mRunner);
    mBalancer.Init(mInstanceManager, itemInfoProvider, blobInfoProvider, ociSpec, mNodeManager, *mMonitorProvider,
        *mRunner, *mNetworkManager);

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LOG_DBG() << "Start Launcher";

    LockGuard lock {mMutex};

    if (auto err = mInstanceManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.UpdateNodeInstances("", mInstanceManager.GetActiveInstances()); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners(mInstanceStatuses);

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    LOG_DBG() << "Stop Launcher";

    LockGuard lock {mMutex};

    if (auto err = mInstanceManager.Stop(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.Stop(); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error Launcher::RunInstances(const Array<RunInstanceRequest>& requests, Array<InstanceStatus>& statuses)
{
    LOG_DBG() << "Run instances";

    UniqueLock lock {mMutex};

    statuses.Clear();

    if (auto err = mRunRequests.Assign(requests); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    UpdateData(false);

    if (auto err = mBalancer.RunInstances(mRunRequests, lock, false); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners(statuses);

    return ErrorEnum::eNone;
}

Error Launcher::Rebalance()
{
    LOG_DBG() << "Rebalance instances";

    UniqueLock lock {mMutex};

    UpdateData(true);

    if (auto err = mBalancer.RunInstances(mRunRequests, lock, true); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners(mInstanceStatuses);

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

void Launcher::NotifyInstanceStatusListeners(Array<InstanceStatus>& statuses)
{
    statuses.Clear();

    for (const auto& instance : mInstanceManager.GetActiveInstances()) {
        if (auto err = statuses.PushBack(instance->GetStatus()); !err.IsNone()) {
            LOG_ERR() << "Failed to push new instance status" << Log::Field(AOS_ERROR_WRAP(err));

            break;
        }
    }

    for (auto& listener : mInstanceStatusListeners) {
        listener->OnInstancesStatusesChanged(statuses);
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
    LockGuard lock {mMutex};

    if (auto err = mInstanceManager.UpdateStatus(status); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners(mInstanceStatuses);

    return ErrorEnum::eNone;
}

Error Launcher::OnNodeInstancesStatusesReceived(const String& nodeID, const Array<InstanceStatus>& statuses)
{
    LockGuard lock {mMutex};

    Error firstErr;

    for (const auto& status : statuses) {
        if (auto err = mInstanceManager.UpdateStatus(status); !err.IsNone()) {
            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }
    }

    if (auto err = mNodeManager.UpdateNodeInstances(nodeID, mInstanceManager.GetActiveInstances()); !err.IsNone()) {
        if (firstErr.IsNone()) {
            firstErr = err;
        }
    }

    if (!firstErr.IsNone()) {
        return firstErr;
    }

    NotifyInstanceStatusListeners(mInstanceStatuses);

    return ErrorEnum::eNone;
}

Error Launcher::OnEnvVarsStatusesReceived(const String& nodeID, const Array<EnvVarsInstanceStatus>& statuses)
{
    (void)nodeID;
    (void)statuses;

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
