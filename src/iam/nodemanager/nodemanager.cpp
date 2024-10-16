/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/nodemanager.hpp"
#include "log.hpp"

namespace aos::iam::nodemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NodeManager::Init(NodeInfoStorageItf& storage)
{
    LockGuard lock {mMutex};

    mStorage = &storage;

    StaticArray<StaticString<cNodeIDLen>, cNodeMaxNum> nodeIds;

    auto err = storage.GetAllNodeIds(nodeIds);
    if (!err.IsNone()) {
        return err;
    }

    for (const auto& nodeId : nodeIds) {
        NodeInfo nodeInfo;

        err = storage.GetNodeInfo(nodeId, nodeInfo);
        if (!err.IsNone()) {
            return err;
        }

        err = mNodeInfoCache.PushBack(nodeInfo);
        if (!err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error NodeManager::SetNodeInfo(const NodeInfo& info)
{
    LockGuard lock {mMutex};

    Error err = ErrorEnum::eNone;

    if (info.mStatus == NodeStatusEnum::eUnprovisioned) {
        err = mStorage->RemoveNodeInfo(info.mNodeID);
        if (err.Is(ErrorEnum::eNotFound)) {
            err = ErrorEnum::eNone;
        }
    } else {
        err = mStorage->SetNodeInfo(info);
    }

    if (!err.IsNone()) {
        return err;
    }

    return UpdateCache(info);
}

Error NodeManager::SetNodeStatus(const String& nodeId, NodeStatus status)
{
    NodeInfo nodeInfo;

    GetNodeInfo(nodeId, nodeInfo);

    nodeInfo.mNodeID = nodeId;
    nodeInfo.mStatus = status;

    return SetNodeInfo(nodeInfo);
}

Error NodeManager::GetNodeInfo(const String& nodeId, NodeInfo& nodeInfo) const
{
    LockGuard lock {mMutex};

    auto cachedInfo = GetNodeFromCache(nodeId);

    if (cachedInfo == nullptr) {
        return ErrorEnum::eNotFound;
    }

    nodeInfo = *cachedInfo;

    return ErrorEnum::eNone;
}

Error NodeManager::GetAllNodeIds(Array<StaticString<cNodeIDLen>>& ids) const
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

Error NodeManager::RemoveNodeInfo(const String& nodeId)
{
    LockGuard lock {mMutex};

    auto cachedInfo = GetNodeFromCache(nodeId);
    if (cachedInfo == nullptr) {
        // Cache contains all the entries, so if not found => just return error.
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto err = mStorage->RemoveNodeInfo(nodeId);
    if (!err.IsNone()) {
        return err;
    }

    err = mNodeInfoCache.Remove(cachedInfo).mError;
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mNodeInfoListener) {
        mNodeInfoListener->OnNodeRemoved(nodeId);
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

NodeInfo* NodeManager::GetNodeFromCache(const String& nodeId)
{
    for (auto& nodeInfo : mNodeInfoCache) {
        if (nodeInfo.mNodeID == nodeId) {
            return &nodeInfo;
        }
    }

    return nullptr;
}

const NodeInfo* NodeManager::GetNodeFromCache(const String& nodeId) const
{
    return const_cast<NodeManager*>(this)->GetNodeFromCache(nodeId);
}

Error NodeManager::UpdateCache(const NodeInfo& nodeInfo)
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

RetWithError<NodeInfo*> NodeManager::AddNodeInfoToCache()
{
    auto err = mNodeInfoCache.EmplaceBack();
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    return {&mNodeInfoCache.Back().mValue, ErrorEnum::eNone};
}

void NodeManager::NotifyNodeInfoChange(const NodeInfo& nodeInfo)
{
    if (mNodeInfoListener) {
        mNodeInfoListener->OnNodeInfoChange(nodeInfo);
    }
}

} // namespace aos::iam::nodemanager
