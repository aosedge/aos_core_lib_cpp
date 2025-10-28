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
    unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
    InstanceRunnerItf& runner)
{
    mNodeInfoProvider    = &nodeInfoProvider;
    mNodeConfigProvider  = &nodeConfigProvider;
    mStorageStateManager = &storageState;
    mRunner              = &runner;
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

        if (!nodeInfo->mProvisioned || nodeInfo->mState != NodeStateEnum::eOnline) {
            continue;
        }

        // add online provisioned node
        mNodes.EmplaceBack();

        if (auto err = mNodes.Back().Init(*nodeInfo, *mNodeConfigProvider, *mRunner); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = mNodeInfoProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mStorageStateManager->IsSamePartition()) {
        mAvailableState = mAvailableStorage = MakeShared<size_t>(&mAllocator, 0);
    } else {
        mAvailableState   = MakeShared<size_t>(&mAllocator, 0);
        mAvailableStorage = MakeShared<size_t>(&mAllocator, 0);
    }

    const auto& [stateSize, stateErr] = mStorageStateManager->GetTotalStateSize();
    if (!stateErr.IsNone()) {
        return AOS_ERROR_WRAP(stateErr);
    }

    *mAvailableState = stateSize;

    const auto& [storageSize, storageErr] = mStorageStateManager->GetTotalStorageSize();
    if (!storageErr.IsNone()) {
        return AOS_ERROR_WRAP(storageErr);
    }

    *mAvailableStorage = storageSize;

    return ErrorEnum::eNone;
}

Error NodeManager::Stop()
{
    if (auto err = mNodeInfoProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mNodes.Clear();

    mAvailableState.Reset();
    mAvailableStorage.Reset();

    return ErrorEnum::eNone;
}

Error NodeManager::UpdateNodeInstances(const Array<SharedPtr<Instance>>& instances)
{
    for (auto& node : mNodes) {
        if (auto err = node.SetRunningInstances(instances); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NodeManager::GetNodesByPriorities(Array<Node*>& nodes)
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
    return mNodes.FindIf([&nodeID](const Node& node) { return node.GetInfo().mNodeID == nodeID; });
}

Array<Node>& NodeManager::GetNodes()
{
    return mNodes;
}

Error NodeManager::SetupStateStorage(
    const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig, gid_t gid, aos::InstanceInfo& info)
{
    auto reqState   = GetReqStateSize(nodeConfig, serviceConfig);
    auto reqStorage = GetReqStorageSize(nodeConfig, serviceConfig);

    storagestate::SetupParams params;

    params.mUID = info.mUID;
    params.mGID = gid;

    if (serviceConfig.mQuotas.mStateLimit.HasValue()) {
        params.mStateQuota = *serviceConfig.mQuotas.mStateLimit;
    }

    if (serviceConfig.mQuotas.mStorageLimit.HasValue()) {
        params.mStorageQuota = *serviceConfig.mQuotas.mStorageLimit;
    }

    if (reqStorage > *mAvailableStorage && !serviceConfig.mSkipResourceLimits) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "not enough storage space"));
    }

    *mAvailableStorage -= reqStorage;
    auto releaseStorage = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { *mAvailableStorage += reqStorage; });

    if (reqState > *mAvailableState && !serviceConfig.mSkipResourceLimits) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "not enough state space"));
    }

    *mAvailableState -= reqState;
    auto releaseState = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { *mAvailableState += reqState; });

    if (auto err = mStorageStateManager->Setup(info, params, info.mStoragePath, info.mStatePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    releaseStorage.Release();
    releaseState.Release();

    LOG_DBG() << "Available storage and state" << Log::Field("state", *mAvailableState)
              << Log::Field("storage", *mAvailableStorage);

    return ErrorEnum::eNone;
}

bool NodeManager::IsRunning(const InstanceIdent& id)
{
    return mNodes.ContainsIf([&id](const Node& info) { return info.IsRunning(id); });
}

bool NodeManager::IsScheduled(const InstanceIdent& id)
{
    return mNodes.ContainsIf([&id](const Node& info) { return info.IsScheduled(id); });
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void NodeManager::OnNodeInfoChanged(const UnitNodeInfo& info)
{
    auto* node = FindNode(info.mNodeID);
    if (node != nullptr) {
        if (!info.mProvisioned || info.mState != NodeStateEnum::eOnline) {
            mNodes.Erase(node);

            return;
        }

        node->UpdateInfo(info);
    } else {
        if (!info.mProvisioned || info.mState != NodeStateEnum::eOnline) {
            return;
        }

        if (auto err = mNodes.EmplaceBack(); !err.IsNone()) {
            LOG_ERR() << "Can't add new node" << Log::Field(AOS_ERROR_WRAP(err));

            return;
        }

        if (auto err = mNodes.Back().Init(info, *mNodeConfigProvider, *mRunner); !err.IsNone()) {
            LOG_ERR() << "Node initialization failed" << Log::Field(AOS_ERROR_WRAP(err));
        }
    }
}

size_t NodeManager::GetReqStateSize(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) const
{
    size_t requestedState = 0;
    auto   quota          = serviceConfig.mQuotas.mStateLimit;

    if (serviceConfig.mRequestedResources.HasValue() && serviceConfig.mRequestedResources->mState.HasValue()) {
        requestedState = ClampResource(*serviceConfig.mRequestedResources->mState, quota);
    } else {
        requestedState = GetReqStateFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    return requestedState;
}

size_t NodeManager::GetReqStorageSize(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) const
{
    size_t requestedStorage = 0;
    auto   quota            = serviceConfig.mQuotas.mStorageLimit;

    if (serviceConfig.mRequestedResources.HasValue() && serviceConfig.mRequestedResources->mStorage.HasValue()) {
        requestedStorage = ClampResource(*serviceConfig.mRequestedResources->mStorage, quota);
    } else {
        requestedStorage = GetReqStorageFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    return requestedStorage;
}

size_t NodeManager::ClampResource(size_t value, const Optional<size_t>& quota) const
{
    if (quota.HasValue() && value > quota.GetValue()) {
        return quota.GetValue();
    }

    return value;
}

size_t NodeManager::GetReqStateFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios) const
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mState.HasValue()) {
        ratio = nodeRatios->mState.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(*quota * ratio + 0.5);
    }

    return 0;
}

size_t NodeManager::GetReqStorageFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios) const
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mStorage.HasValue()) {
        ratio = nodeRatios->mStorage.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(*quota * ratio + 0.5);
    }

    return 0;
}

} // namespace aos::cm::launcher
