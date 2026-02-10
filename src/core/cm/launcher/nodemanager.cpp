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

    LOG_DBG() << "Start node manager" << Log::Field("nodes", nodes->Size());

    auto nodeInfo = MakeUnique<UnitNodeInfo>(&mAllocator);

    for (const auto& nodeID : *nodes) {
        if (auto err = mNodeInfoProvider->GetNodeInfo(nodeID, *nodeInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        LOG_DBG() << "Get node info" << Log::Field("nodeID", nodeID) << Log::Field("state", nodeInfo->mState);

        if (nodeInfo->mState != NodeStateEnum::eProvisioned) {
            continue;
        }

        // Add online provisioned node
        mNodes.EmplaceBack();

        mNodes.Back().Init(nodeInfo->mNodeID, *mNodeConfigProvider, *mRunner, &mNodeAllocator);
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

Error NodeManager::LoadSMDataForActiveInstances(
    const Array<SharedPtr<Instance>>& instances, ImageInfoProvider& imageInfoProvider)
{
    for (const auto& instance : instances) {
        const auto& nodeID         = instance->GetInfo().mNodeID;
        const auto& instanceID     = instance->GetInfo().mInstanceIdent;
        const auto& runtimeID      = instance->GetInfo().mRuntimeID;
        const auto& manifestDigest = instance->GetInfo().mManifestDigest;

        if (nodeID.IsEmpty()) {
            continue;
        }

        auto* node = FindNode(nodeID);
        if (node == nullptr) {
            LOG_ERR() << "Can't find node" << Log::Field("instanceID", instanceID) << Log::Field("nodeID", nodeID)
                      << Log::Field(AOS_ERROR_WRAP(ErrorEnum::eNotFound));
            continue;
        }

        auto imageDescriptor = MakeUnique<oci::IndexContentDescriptor>(&mAllocator);
        auto findDescErr     = FindImageDescriptor(
            instanceID.mItemID, instance->GetInfo().mVersion, manifestDigest, imageInfoProvider, *imageDescriptor);
        if (!findDescErr.IsNone()) {
            LOG_ERR() << "Can't find image descriptor" << Log::Field("instanceID", instanceID)
                      << Log::Field("manifestDigest", manifestDigest) << Log::Field(AOS_ERROR_WRAP(findDescErr));

            continue;
        }

        auto releaseConfigs = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { instance->ResetConfigs(); });
        if (auto err = instance->LoadConfigs(*imageDescriptor); !err.IsNone()) {
            LOG_ERR() << "Can't load instance configs" << Log::Field("instanceID", instanceID)
                      << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }

        if (auto err = instance->Schedule(*node, runtimeID); !err.IsNone()) {
            LOG_ERR() << "Can't load instance" << Log::Field("nodeID", nodeID) << Log::Field("instanceID", instanceID)
                      << Log::Field(AOS_ERROR_WRAP(err));

            continue;
        }
    }

    return ErrorEnum::eNone;
}

Error NodeManager::NotifyNodeStatusReceived(const String& nodeID)
{
    auto node = FindNode(nodeID);
    if (node == nullptr) {
        // Create new node for cases when status is received before node info.
        if (auto err = mNodes.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        mNodes.Back().Init(nodeID, *mNodeConfigProvider, *mRunner, &mNodeAllocator);

        node = FindNode(nodeID);
    }

    if (node == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "node not found"));
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

Error NodeManager::SendScheduledInstances(UniqueLock<Mutex>& lock, const Array<SharedPtr<Instance>>& scheduledInstances,
    const Array<InstanceStatus>& runningInstances)
{
    Error firstErr = ErrorEnum::eNone;

    for (auto& node : mNodes) {
        auto err = node.SendScheduledInstances(scheduledInstances, runningInstances);
        if (!err.IsNone()) {
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

Error NodeManager::ResendInstances(UniqueLock<Mutex>& lock, const Array<StaticString<cIDLen>>& updatedNodes,
    const Array<SharedPtr<Instance>>& activeInstances, const Array<InstanceStatus>& runningInstances)
{
    Error firstErr = ErrorEnum::eNone;

    mNodesExpectedToSendStatus.Clear();

    for (auto& node : mNodes) {
        if (!updatedNodes.Contains(node.GetInfo().mNodeID)) {
            continue;
        }

        auto [isRequestSent, sendErr] = node.ResendInstances(activeInstances, runningInstances);
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

        mNodes.Back().Init(info.mNodeID, *mNodeConfigProvider, *mRunner, &mNodeAllocator);
        mNodes.Back().UpdateInfo(info);

        return true;
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error NodeManager::FindImageDescriptor(const String& itemID, const String& version, const String& manifestDigest,
    ImageInfoProvider& imageInfoProvider, oci::IndexContentDescriptor& imageDescriptor)
{
    auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);

    if (auto err = imageInfoProvider.GetImageIndex(itemID, version, *imageIndex); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto ptr = imageIndex->mManifests.FindIf(
        [&](const oci::IndexContentDescriptor& descriptor) { return descriptor.mDigest == manifestDigest; });
    if (ptr == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    imageDescriptor = *ptr;

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
