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

Error NodeManager::Init(NodeInfoStorageItf& storage)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Init node manager";

    mStorage = &storage;

    auto nodeIDs = MakeUnique<StaticArray<StaticString<cIDLen>, cNodeMaxNum>>(&mAllocator);

    auto err = storage.GetAllNodeIDs(*nodeIDs);
    if (!err.IsNone()) {
        return err;
    }

    for (const auto& nodeID : *nodeIDs) {
        auto nodeInfo = MakeUnique<NodeInfoObsolete>(&mAllocator);

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

Error NodeManager::SetNodeInfo(const NodeInfoObsolete& info)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Set node info: nodeID=" << info.mNodeID << ", state=" << info.mState;

    const auto* cachedInfo = GetNodeFromCache(info.mNodeID);

    if (cachedInfo != nullptr && *cachedInfo == info) {
        return ErrorEnum::eNone;
    }

    return UpdateNodeInfo(info);
}

Error NodeManager::SetNodeState(const String& nodeID, NodeStateObsolete state)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Set node state: nodeID=" << nodeID << ", state=" << state;

    auto        nodeInfo   = MakeUnique<NodeInfoObsolete>(&mAllocator);
    const auto* cachedInfo = GetNodeFromCache(nodeID);

    if (cachedInfo != nullptr) {
        if (cachedInfo->mState == state) {
            return ErrorEnum::eNone;
        }

        *nodeInfo = *cachedInfo;
    }

    nodeInfo->mNodeID = nodeID;
    nodeInfo->mState  = state;

    return UpdateNodeInfo(*nodeInfo);
}

Error NodeManager::GetNodeInfo(const String& nodeID, NodeInfoObsolete& nodeInfo) const
{
    LockGuard lock {mMutex};

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

    for (const auto& nodeInfo : mNodeInfoCache) {
        auto err = ids.PushBack(nodeInfo.mNodeID);
        if (!err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error NodeManager::RemoveNodeInfo(const String& nodeID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove node info: nodeID=" << nodeID;

    auto cachedInfo = GetNodeFromCache(nodeID);
    if (cachedInfo == nullptr) {
        // Cache contains all the entries, so if not found => just return error.
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto err = mStorage->RemoveNodeInfo(nodeID);
    if (!err.IsNone()) {
        return err;
    }

    mNodeInfoCache.Erase(cachedInfo);

    if (mNodeInfoListener) {
        mNodeInfoListener->OnNodeRemoved(nodeID);
    }

    return ErrorEnum::eNone;
}

Error NodeManager::SubscribeNodeInfoChange(NodeInfoListenerItf& listener)
{
    LockGuard lock {mMutex};

    mNodeInfoListener = &listener;

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

NodeInfoObsolete* NodeManager::GetNodeFromCache(const String& nodeID)
{
    for (auto& nodeInfo : mNodeInfoCache) {
        if (nodeInfo.mNodeID == nodeID) {
            return &nodeInfo;
        }
    }

    return nullptr;
}

const NodeInfoObsolete* NodeManager::GetNodeFromCache(const String& nodeID) const
{
    return const_cast<NodeManager*>(this)->GetNodeFromCache(nodeID);
}

Error NodeManager::UpdateCache(const NodeInfoObsolete& nodeInfo)
{
    bool isAdded = false;

    auto cachedInfo = GetNodeFromCache(nodeInfo.mNodeID);
    if (cachedInfo == nullptr) {
        Error err = ErrorEnum::eNone;

        Tie(cachedInfo, err) = AddNodeInfoToCache();
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        isAdded = true;
    }

    if (isAdded || *cachedInfo != nodeInfo) {
        *cachedInfo = nodeInfo;
        NotifyNodeInfoChange(nodeInfo);
    }

    return ErrorEnum::eNone;
}

Error NodeManager::UpdateNodeInfo(const NodeInfoObsolete& info)
{
    Error err = ErrorEnum::eNone;

    if (info.mState == NodeStateObsoleteEnum::eUnprovisioned) {
        err = mStorage->RemoveNodeInfo(info.mNodeID);
        if (err.Is(ErrorEnum::eNotFound)) {
            err = ErrorEnum::eNone;
        }
    } else {
        err = mStorage->SetNodeInfo(info);
    }

    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return UpdateCache(info);
}

RetWithError<NodeInfoObsolete*> NodeManager::AddNodeInfoToCache()
{
    auto err = mNodeInfoCache.EmplaceBack();
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    return {&mNodeInfoCache.Back(), ErrorEnum::eNone};
}

void NodeManager::NotifyNodeInfoChange(const NodeInfoObsolete& nodeInfo)
{
    if (mNodeInfoListener) {
        mNodeInfoListener->OnNodeInfoChange(nodeInfo);
    }
}

} // namespace aos::iam::nodemanager
