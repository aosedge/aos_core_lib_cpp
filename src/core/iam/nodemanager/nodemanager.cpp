/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "nodemanager.hpp"

namespace aos::iam::nodemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NodeManager::Init(StorageItf& storage)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Init node manager";

    mStorage = &storage;

    auto nodeIDs = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumNodes>>(&mAllocator);

    auto err = storage.GetAllNodeIDs(*nodeIDs);
    if (!err.IsNone()) {
        return err;
    }

    for (const auto& nodeID : *nodeIDs) {
        auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

        err = storage.GetNodeInfo(nodeID, *nodeInfo);
        if (!err.IsNone()) {
            return err;
        }

        err = mNodeInfoCache.PushBack(*nodeInfo);
        if (!err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error NodeManager::SetNodeInfo(const NodeInfo& info)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Set node info" << Log::Field("nodeID", info.mNodeID) << Log::Field("state", info.mState)
              << Log::Field("connected", info.mIsConnected);

    if (auto err = UpdateStorage(info); !err.IsNone()) {
        return err;
    }

    if (auto err = UpdateCache(info); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error NodeManager::SetNodeState(const String& nodeID, const NodeState& state)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Set node state" << Log::Field("nodeID", nodeID) << Log::Field("state", state);

    const auto* cachedInfo = GetNodeFromCache(nodeID);
    if (!cachedInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

    *nodeInfo        = *cachedInfo;
    nodeInfo->mState = state;

    if (auto err = UpdateStorage(*nodeInfo); !err.IsNone()) {
        return err;
    }

    if (auto err = UpdateCache(*nodeInfo); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error NodeManager::SetNodeConnected(const String& nodeID, bool isConnected)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Set node connected" << Log::Field("nodeID", nodeID) << Log::Field("connected", isConnected);

    const auto* cachedInfo = GetNodeFromCache(nodeID);
    if (!cachedInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

    *nodeInfo              = *cachedInfo;
    nodeInfo->mIsConnected = isConnected;

    if (auto err = UpdateCache(*nodeInfo); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error NodeManager::GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get node info" << Log::Field("nodeID", nodeID);

    auto cachedInfo = GetNodeFromCache(nodeID);

    if (cachedInfo == nullptr) {
        return ErrorEnum::eNotFound;
    }

    nodeInfo = *cachedInfo;

    return ErrorEnum::eNone;
}

Error NodeManager::GetAllNodeIDs(Array<StaticString<cIDLen>>& ids) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get all node IDs";

    for (const auto& nodeInfo : mNodeInfoCache) {
        auto err = ids.PushBack(nodeInfo.mNodeID);
        if (!err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error NodeManager::SubscribeListener(iamclient::NodeInfoListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe node info listener";

    if (auto err = mListeners.PushBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NodeManager::UnsubscribeListener(iamclient::NodeInfoListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe node info listener";

    auto count = mListeners.Remove(&listener);

    return count == 0 ? AOS_ERROR_WRAP(ErrorEnum::eNotFound) : ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

NodeInfo* NodeManager::GetNodeFromCache(const String& nodeID)
{
    auto it = mNodeInfoCache.FindIf([&nodeID](const NodeInfo& nodeInfo) { return nodeInfo.mNodeID == nodeID; });
    if (it != mNodeInfoCache.end()) {
        return it;
    }

    return nullptr;
}

const NodeInfo* NodeManager::GetNodeFromCache(const String& nodeID) const
{
    return const_cast<NodeManager*>(this)->GetNodeFromCache(nodeID);
}

Error NodeManager::UpdateCache(const NodeInfo& nodeInfo)
{
    auto cachedInfo = GetNodeFromCache(nodeInfo.mNodeID);
    if (cachedInfo && *cachedInfo == nodeInfo) {
        return ErrorEnum::eNone;
    }

    if (cachedInfo == nullptr) {
        Error err = ErrorEnum::eNone;

        Tie(cachedInfo, err) = AddNodeInfoToCache(nodeInfo);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        *cachedInfo = nodeInfo;
    }

    NotifyNodeInfoChange(nodeInfo);

    return ErrorEnum::eNone;
}

Error NodeManager::UpdateStorage(const NodeInfo& info)
{
    auto        storageInfo = MakeUnique<NodeInfo>(&mAllocator);
    const auto* cachedInfo  = GetNodeFromCache(info.mNodeID);

    *storageInfo = info;

    if (cachedInfo) {
        // compare without mIsConnected field
        storageInfo->mIsConnected = cachedInfo->mIsConnected;

        if (*storageInfo == *cachedInfo) {
            return ErrorEnum::eNone;
        }
    }

    // Do not store connection state
    storageInfo->mIsConnected = false;

    if (info.mState == NodeStateEnum::eUnprovisioned) {
        if (auto err = mStorage->RemoveNodeInfo(info.mNodeID); !err.Is(ErrorEnum::eNotFound) && !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    if (auto err = mStorage->SetNodeInfo(*storageInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<NodeInfo*> NodeManager::AddNodeInfoToCache(const NodeInfo& info)
{
    if (auto err = mNodeInfoCache.EmplaceBack(info); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    return &mNodeInfoCache.Back();
}

void NodeManager::NotifyNodeInfoChange(const NodeInfo& nodeInfo)
{
    for (auto& listener : mListeners) {
        listener->OnNodeInfoChanged(nodeInfo);
    }
}

} // namespace aos::iam::nodemanager
