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
#include <core/iam/config.hpp>

#include "itf/nodemanager.hpp"
#include "itf/storage.hpp"

namespace aos::iam::nodemanager {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

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
     * @param isConnected node connected flag.
     * @return Error.
     */
    Error SetNodeState(const String& nodeID, const NodeState& state, bool isConnected) override;

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
    Error GetAllNodeIDs(Array<StaticString<cIDLen>>& ids) const override;

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
    static constexpr auto cNodeMaxNum    = AOS_CONFIG_NODEMANAGER_NODE_MAX_NUM;
    static constexpr auto cAllocatorSize = sizeof(StaticArray<StaticString<cIDLen>, cNodeMaxNum>) + sizeof(NodeInfo);

    NodeInfo*               GetNodeFromCache(const String& nodeID);
    const NodeInfo*         GetNodeFromCache(const String& nodeID) const;
    Error                   UpdateNodeInfo(const NodeInfo& info);
    Error                   UpdateCache(const NodeInfo& nodeInfo);
    RetWithError<NodeInfo*> AddNodeInfoToCache();
    void                    NotifyNodeInfoChange(const NodeInfo& nodeInfo);

    NodeInfoStorageItf*                mStorage {};
    NodeInfoListenerItf*               mNodeInfoListener {};
    StaticArray<NodeInfo, cNodeMaxNum> mNodeInfoCache;
    mutable Mutex                      mMutex;

    StaticAllocator<cAllocatorSize> mAllocator;
};

/** @}*/

} // namespace aos::iam::nodemanager

#endif
