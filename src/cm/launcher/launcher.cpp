/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aos/cm/launcher/launcher.hpp>

#include "log.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(const Config& config, storage::StorageItf& storage,
    nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider, nodemanager::NodeManagerItf& nodeManager,
    imageprovider::ImageProviderItf& imageProvider, resourcemanager::ResourceManagerItf& resourceManager,
    storagestate::StorageStateItf& storageState, networkmanager::NetworkManagerItf& networkManager)
{
    LOG_DBG() << "Init Launcher";

    LockGuard lock {mMutex};

    mConfig           = config;
    mStorage          = &storage;
    mNodeInfoProvider = &nodeInfoProvider;
    mNodeManager      = &nodeManager;
    mImageProvider    = &imageProvider;
    mResourceManager  = &resourceManager;
    mStorageState     = &storageState;
    mNetworkManager   = &networkManager;

    if (auto err = mInstanceManager.Init(config, storage, imageProvider, storageState); !err.IsNone()) {
        return err;
    }

    if (auto err = mBalancer.Init(*mNetworkManager, mInstanceManager, *mImageProvider, *mNodeManager); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LOG_DBG() << "Start Launcher";

    LockGuard lock {mMutex};

    if (auto err = InitNodes(false); !err.IsNone()) {
        return err;
    }

    auto sendRunStatus = [this](void*) {
        LockGuard lock {mMutex};

        SendRunStatus();
    };

    if (auto err = mConnectionTimer.Start(mConfig.mNodesConnectionTimeout, sendRunStatus); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    LOG_DBG() << "Stop Launcher";

    LockGuard lock {mMutex};

    if (auto err = mNodeManager->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceManager.Stop(); !err.IsNone()) {
        return err;
    }

    if (auto err = mConnectionTimer.Stop(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::RunInstances(const Array<RunServiceRequest>& requests, bool rebalancing)
{
    LOG_ERR() << "Run service instances" << Log::Field("rebalancing", rebalancing);

    LockGuard lock {mMutex};

    auto startInstances = MakeUnique<StaticArray<RunInstanceRequest, cMaxNumInstances>>(&mAllocator);
    auto stopInstances  = MakeUnique<StaticArray<storage::InstanceInfo, cMaxNumInstances>>(&mAllocator);

    if (auto err = UpdateNodes(rebalancing); !err.IsNone()) {
        LOG_ERR() << "Failed init nodes" << Log::Field(err);

        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceManager.UpdateInstanceCache(); !err.IsNone()) {
        LOG_ERR() << "Failed update instance data" << Log::Field(err);

        return AOS_ERROR_WRAP(err);
    }

    for (const auto& request : requests) {
        for (uint64_t instanceInd = 0; instanceInd < request.mNumInstances; instanceInd++) {
            auto runRequest = MakeUnique<RunInstanceRequest>(&mAllocator);

            runRequest->mInstanceID.mServiceID = request.mServiceID;
            runRequest->mInstanceID.mSubjectID = request.mSubjectID;
            runRequest->mInstanceID.mInstance  = instanceInd;
            runRequest->mPriority              = request.mPriority;
            runRequest->mLabels                = request.mLabels;

            if (auto err = startInstances->EmplaceBack(*runRequest); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    for (const auto& runInstance : mInstanceManager.GetRunningInstances()) {
        bool exist = requests.ExistIf([&runInstance](const RunServiceRequest& info) {
            return runInstance.mInstanceID.mServiceID == info.mServiceID
                && runInstance.mInstanceID.mSubjectID == info.mSubjectID
                && runInstance.mInstanceID.mInstance < info.mNumInstances;
        });

        if (!exist) {
            if (auto err = stopInstances->EmplaceBack(runInstance); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    // TODO: Handle service dependencies.

    if (auto err = mBalancer.StopInstances(*stopInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // Sort starting instances by priorities.
    startInstances->Sort([](const RunInstanceRequest& left, const RunInstanceRequest& right) {
        return left.mPriority > right.mPriority
            || (left.mPriority == right.mPriority && left.mInstanceID < right.mInstanceID);
    });

    if (auto err = mBalancer.StartInstances(*startInstances, rebalancing); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Launcher::SetListener(RunStatusListenerItf& listener)
{
    LockGuard lock {mMutex};

    mRunStatusListener = &listener;
}

void Launcher::ResetListener()
{
    LockGuard lock {mMutex};

    mRunStatusListener = nullptr;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Launcher::InitNodes(bool rebalancing)
{
    mNodes.Clear();

    auto nodeIDs = MakeUnique<StaticArray<StaticString<cNodeIDLen>, cNodeMaxNum>>(&mAllocator);

    if (auto err = mNodeInfoProvider->GetAllNodeIds(*nodeIDs); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& nodeID : *nodeIDs) {
        auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

        if (auto err = mNodeInfoProvider->GetNodeInfo(nodeID, *nodeInfo); !err.IsNone()) {
            LOG_ERR() << "Can't get node info" << Log::Field("nodeID", nodeID) << Log::Field(err);

            continue;
        }

        if (nodeInfo->mStatus == NodeStatusEnum::eUnprovisioned) {
            LOG_ERR() << "Skip not provisioned node" << Log::Field("nodeID", nodeID);

            continue;
        }

        if (auto err = mNodes.Emplace(nodeID); !err.IsNone()) {
            LOG_ERR() << "Can't create node handler" << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }

        auto& node        = mNodes.Find(nodeID)->mSecond;
        bool  isLocalNode = nodeInfo->mNodeID == mNodeInfoProvider->GetCurrentNodeID();

        if (auto err = node.Init(*nodeInfo, *mNodeManager, *mResourceManager, isLocalNode, rebalancing);
            !err.IsNone()) {
            LOG_ERR() << "Can't create node handler" << Log::Field("nodeID", nodeID) << Log::Field(AOS_ERROR_WRAP(err));

            mNodes.Remove(nodeID);
            continue;
        }
    }

    mBalancer.UpdateNodes(mNodes);

    return ErrorEnum::eNone;
}

Error Launcher::UpdateNodes(bool rebalancing)
{
    for (auto& node : mNodes) {
        if (auto err = node.mSecond.UpdateNodeData(*mNodeManager, *mResourceManager, rebalancing); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

void Launcher::OnStatusChanged(const nodemanager::NodeRunInstanceStatus& status)
{
    LOG_DBG() << "Receive run status from node" << Log::Field("nodeID", status.mNodeID);

    auto* node = mNodes.Find(status.mNodeID);
    if (node == mNodes.end()) {
        LOG_ERR() << "Received status for unknown node" << Log::Field("nodeID", status.mNodeID);

        return;
    }

    node->mSecond.SetRunStatus(status);

    // Wait until all nodes send run status.
    for (const auto& [_, node] : mNodes) {
        if (node.IsWaiting()) {
            return;
        }
    }

    LOG_INF() << "All SM statuses received";

    if (auto err = mConnectionTimer.Stop(); !err.IsNone()) {
        LOG_ERR() << "Stopping connection timer failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    SendRunStatus();

    // TODO: Handle service dependencies.
}

void Launcher::SendRunStatus()
{
    mRunStatus.Clear();

    const auto& [nodes, err] = NodeHandler::GetNodesByPriorities(mNodes);
    for (const auto& node : nodes) {
        if (node->IsWaiting()) {
            node->SetWaiting(false);
            for (const auto& instance : node->GetScheduledInstances()) {

                auto runRequest = MakeUnique<nodemanager::InstanceStatus>(&mAllocator);

                runRequest->mNodeID        = node->GetInfo().mNodeID;
                runRequest->mInstanceIdent = instance.mInstanceIdent;
                runRequest->mRunState      = InstanceRunStateEnum::eFailed;
                runRequest->mError         = AOS_ERROR_WRAP(Error(ErrorEnum::eTimeout, "wait run status timeout"));

                if (auto err = mRunStatus.EmplaceBack(*runRequest); !err.IsNone()) {
                    LOG_ERR() << "Failed to add run status" << Log::Field(AOS_ERROR_WRAP(err));

                    return;
                }
            }
        } else {
            auto err = mRunStatus.Insert(
                mRunStatus.end(), node->GetRunStatus().mInstances.begin(), node->GetRunStatus().mInstances.end());
            if (!err.IsNone()) {
                LOG_ERR() << "Failed to add run status" << Log::Field(AOS_ERROR_WRAP(err));

                return;
            }
        }
    }

    for (auto& runStatus : mRunStatus) {
        if (auto err = mInstanceManager.GetInstanceCheckSum(runStatus.mInstanceIdent, runStatus.mStateChecksum);
            !err.IsNone()) {
            if (!err.IsNone()) {
                LOG_ERR() << "Failed to get instance checksum" << Log::Field("instanceID", runStatus.mInstanceIdent)
                          << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }

    const auto& errorStatus = mInstanceManager.GetErrorStatuses();
    if (auto err = mRunStatus.Insert(mRunStatus.end(), errorStatus.begin(), errorStatus.end()); !err.IsNone()) {
        LOG_ERR() << "Failed to append error statuses" << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (mRunStatusListener) {
        mRunStatusListener->OnRunStatusChanged(mRunStatus);
    }
}

} // namespace aos::cm::launcher
