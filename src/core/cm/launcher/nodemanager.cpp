/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nodemanager.hpp"

#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void NodeManager::Init(nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    unitconfig::NodeConfigProviderItf& nodeConfigProvider, InstanceRunnerItf& runner)
{
    mNodeInfoProvider   = &nodeInfoProvider;
    mNodeConfigProvider = &nodeConfigProvider;
    mRunner             = &runner;
}

Error NodeManager::Start()
{
    auto nodes = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumNodes>>(&mAllocator);

    if (auto err = mNodeInfoProvider->GetAllNodeIDs(*nodes); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& nodeID : *nodes) {
        auto nodeInfo = MakeUnique<UnitNodeInfo>(&mAllocator);

        if (auto err = mNodeInfoProvider->GetNodeInfo(nodeID, *nodeInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (nodeInfo->mState != NodeStateEnum::eProvisioned || !nodeInfo->mIsConnected) {
            continue;
        }

        // Add online provisioned node
        mNodes.EmplaceBack();

        mNodes.Back().Init(nodeInfo->mNodeID, *mNodeConfigProvider, *mRunner);
        mNodes.Back().UpdateInfo(*nodeInfo);
    }

    return ErrorEnum::eNone;
}

Error NodeManager::Stop()
{
    mNodes.Clear();

    // Unlock waiting run requests.
    mNodesExpectedToSendStatus.Clear();
    mStatusUpdateCondVar.NotifyAll();

    return ErrorEnum::eNone;
}

Error NodeManager::PrepareForBalancing(bool rebalancing)
{
    for (auto& node : mNodes) {
        node.PrepareForBalancing(rebalancing);
    }

    return ErrorEnum::eNone;
}

Error NodeManager::LoadSentInstances(const Array<SharedPtr<Instance>>& instances)
{
    for (auto& node : mNodes) {
        if (auto err = node.LoadSentInstances(instances); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NodeManager::UpdateRunnigInstances(const String& nodeID, const Array<InstanceStatus>& statuses)
{
    auto node = FindNode(nodeID);
    if (node == nullptr) {
        // Create new node for cases when status is received before node info.
        if (auto err = mNodes.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        mNodes.Back().Init(nodeID, *mNodeConfigProvider, *mRunner);

        node = FindNode(nodeID);
    }

    if (node == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "node not found"));
    }

    if (auto err = node->UpdateRunningInstances(statuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (node->GetInfo().mIsConnected && node->GetInfo().mState == NodeStateEnum::eProvisioned) {
        if (mNodesExpectedToSendStatus.Remove(nodeID) != 0) {
            mStatusUpdateCondVar.NotifyAll();
        }
    }

    return ErrorEnum::eNone;
}

Error NodeManager::GetConnectedNodes(Array<Node*>& nodes)
{
    nodes.Clear();

    for (auto& node : mNodes) {
        if (auto err = nodes.PushBack(&node); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    nodes.Sort([](const Node* left, const Node* right) {
        if (left->GetConfig().mPriority == right->GetConfig().mPriority) {
            return left->GetInfo().mNodeID < right->GetInfo().mNodeID;
        }

        return left->GetConfig().mPriority > right->GetConfig().mPriority;
    });

    return ErrorEnum::eNone;
}

Node* NodeManager::FindNode(const String& nodeID)
{
    auto it = mNodes.FindIf([&nodeID](const Node& node) { return node.GetInfo().mNodeID == nodeID; });

    return it != mNodes.end() ? it : nullptr;
}

Array<Node>& NodeManager::GetNodes()
{
    return mNodes;
}

bool NodeManager::IsScheduled(const InstanceIdent& id)
{
    return mNodes.ContainsIf([&id](const Node& info) { return info.IsScheduled(id); });
}

Error NodeManager::SendScheduledInstances(UniqueLock<Mutex>& lock)
{
    Error firstErr = ErrorEnum::eNone;

    for (auto& node : mNodes) {
        if (auto err = node.SendScheduledInstances(); !err.IsNone()) {
            LOG_ERR() << "Can't send instance update" << Log::Field("nodeID", node.GetInfo().mNodeID)
                      << Log::Field(err);

            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }
    }

    if (!firstErr.IsNone()) {
        return firstErr;
    }

    // Wait for node statuses
    mNodesExpectedToSendStatus.Clear();

    for (auto& node : mNodes) {
        if (auto err = mNodesExpectedToSendStatus.PushBack(node.GetInfo().mNodeID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    auto err
        = mStatusUpdateCondVar.Wait(lock, cStatusUpdateTimeout, [&]() { return mNodesExpectedToSendStatus.IsEmpty(); });
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NodeManager::ResendInstances(UniqueLock<Mutex>& lock, const Array<StaticString<cIDLen>>& updatedNodes)
{
    Error firstErr = ErrorEnum::eNone;

    mNodesExpectedToSendStatus.Clear();

    for (auto& node : mNodes) {
        if (!updatedNodes.Contains(node.GetInfo().mNodeID)) {
            continue;
        }

        auto [isRequestSent, sendErr] = node.ResendInstances();
        if (!sendErr.IsNone()) {
            LOG_ERR() << "Can't send instance update" << Log::Field("nodeID", node.GetInfo().mNodeID)
                      << Log::Field(sendErr);

            if (firstErr.IsNone()) {
                firstErr = sendErr;
            }
        }

        if (isRequestSent) {
            if (auto err = mNodesExpectedToSendStatus.PushBack(node.GetInfo().mNodeID); !err.IsNone()) {
                if (firstErr.IsNone()) {
                    firstErr = AOS_ERROR_WRAP(err);
                }
            }
        }
    }

    if (!firstErr.IsNone()) {
        return firstErr;
    }

    auto err
        = mStatusUpdateCondVar.Wait(lock, cStatusUpdateTimeout, [&]() { return mNodesExpectedToSendStatus.IsEmpty(); });
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool NodeManager::UpdateNodeInfo(const UnitNodeInfo& info)
{
    // Don't wait for instanse status for unprovisioned nodes(offline/online doesnt matter)
    if (info.mState != NodeStateEnum::eProvisioned) {
        if (mNodesExpectedToSendStatus.Remove(info.mNodeID) != 0) {
            mStatusUpdateCondVar.NotifyAll();
        }
    }

    auto* node = FindNode(info.mNodeID);
    if (node != nullptr) {
        if (info.mState != NodeStateEnum::eProvisioned) {
            mNodes.Erase(node);

            return true;
        }

        return node->UpdateInfo(info);
    } else {
        if (info.mState != NodeStateEnum::eProvisioned) {
            return false;
        }

        if (auto err = mNodes.EmplaceBack(); !err.IsNone()) {
            LOG_ERR() << "Can't add new node" << Log::Field(AOS_ERROR_WRAP(err));

            return false;
        }

        mNodes.Back().Init(info.mNodeID, *mNodeConfigProvider, *mRunner);
        mNodes.Back().UpdateInfo(info);

        return true;
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

} // namespace aos::cm::launcher
