/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_NODEMANAGER_NODEMANAGER_HPP_
#define AOS_CORE_NODEMANAGER_NODEMANAGER_HPP_

#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/types.hpp>
#include <core/iam/config.hpp>

namespace aos::iam::nodemanager {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * NodeInfo listener interface.
 */
class NodeInfoListenerItf {
public:
    /**
     * Node info change notification.
     *
     * @param info node info.
     */
    virtual void OnNodeInfoChange(const NodeInfo& info) = 0;

    /**
     * Node info removed notification.
     *
     * @param id id of the node been removed.
     */
    virtual void OnNodeRemoved(const String& id) = 0;

    /**
     * Destroys object instance.
     */
    virtual ~NodeInfoListenerItf() = default;
};

/**
 * Node manager interface.
 */
class NodeManagerItf {
public:
    /**
     * Updates whole information for a node.
     *
     * @param info node info.
     * @return Error.
     */
    virtual Error SetNodeInfo(const NodeInfo& info) = 0;

    /**
     * Updates node state.
     *
     * @param nodeID node identifier.
     * @param state node state.
     * @return Error.
     */
    virtual Error SetNodeState(const String& nodeID, NodeState state) = 0;

    /**
     * Returns node info.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node identifier.
     * @return Error.
     */
    virtual Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const = 0;

    /**
     * Returns ids for all the node in the manager.
     *
     * @param ids result node identifiers.
     * @return Error.
     */
    virtual Error GetAllNodeIds(Array<StaticString<cNodeIDLen>>& ids) const = 0;

    /**
     * Removes node info by its id.
     *
     * @param nodeID node identifier.
     * @return Error.
     */
    virtual Error RemoveNodeInfo(const String& nodeID) = 0;

    /**
     * Subscribes listener for node info updates.
     *
     * @param listener listener to subscribe.
     * @return Error.
     */
    virtual Error SubscribeNodeInfoChange(NodeInfoListenerItf& listener) = 0;

    /**
     * Destroys object instance.
     */
    virtual ~NodeManagerItf() = default;
};

/**
 * Node info storage interface.
 */
class NodeInfoStorageItf {
public:
    /**
     * Updates whole information for a node.
     *
     * @param info node info.
     * @return Error.
     */
    virtual Error SetNodeInfo(const NodeInfo& info) = 0;

    /**
     * Returns node info.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node identifier.
     * @return Error.
     */
    virtual Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const = 0;

    /**
     * Returns ids for all the node in the manager.
     *
     * @param ids result node identifiers.
     * @return Error.
     */
    virtual Error GetAllNodeIds(Array<StaticString<cNodeIDLen>>& ids) const = 0;

    /**
     * Removes node info by its id.
     *
     * @param nodeID node identifier.
     * @return Error.
     */
    virtual Error RemoveNodeInfo(const String& nodeID) = 0;

    /**
     * Destroys object instance.
     */
    virtual ~NodeInfoStorageItf() = default;
};

/**
 * Node manager.
 */
class NodeManager : public NodeManagerItf {
public:
    /**
     * Initializes node manager.
     *
     * @param storage node info storage.
     * @return Error.
     */
    Error Init(NodeInfoStorageItf& storage);

    /**
     * Updates whole information for a node.
     *
     * @param info node info.
     * @return Error.
     */
    Error SetNodeInfo(const NodeInfo& info) override;

    /**
     * Updates state for a node.
     *
     * @param nodeID node identifier.
     * @param state node state.
     * @return Error.
     */
    Error SetNodeState(const String& nodeID, NodeState state) override;

    /**
     * Returns node info.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node identifier.
     * @return Error.
     */
    Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const override;

    /**
     * Returns ids for all the node in the manager.
     *
     * @param ids result node identifiers.
     * @return Error.
     */
    Error GetAllNodeIds(Array<StaticString<cNodeIDLen>>& ids) const override;

    /**
     * Removes node info by its id.
     *
     * @param nodeID node identifier.
     * @return Error.
     */
    Error RemoveNodeInfo(const String& nodeID) override;

    /**
     * Subscribes listener for node info updates.
     *
     * @param listener listener to subscribe.
     * @return Error.
     */
    Error SubscribeNodeInfoChange(NodeInfoListenerItf& listener) override;

private:
    static constexpr auto cNodeMaxNum = AOS_CONFIG_NODEMANAGER_NODE_MAX_NUM;
    static constexpr auto cAllocatorSize
        = sizeof(StaticArray<StaticString<cNodeIDLen>, cNodeMaxNum>) + sizeof(NodeInfo);

    NodeInfo*       GetNodeFromCache(const String& nodeID);
    const NodeInfo* GetNodeFromCache(const String& nodeID) const;
    Error           UpdateNodeInfo(const NodeInfo& info);

    Error UpdateCache(const NodeInfo& nodeInfo);

    RetWithError<NodeInfo*> AddNodeInfoToCache();
    void                    NotifyNodeInfoChange(const NodeInfo& nodeInfo);

    NodeInfoStorageItf*                mStorage          = nullptr;
    NodeInfoListenerItf*               mNodeInfoListener = nullptr;
    StaticArray<NodeInfo, cNodeMaxNum> mNodeInfoCache;
    mutable Mutex                      mMutex;

    StaticAllocator<cAllocatorSize> mAllocator;
};

/** @}*/

} // namespace aos::iam::nodemanager

#endif
