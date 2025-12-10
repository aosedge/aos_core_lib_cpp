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
    Error Init(StorageItf& storage);

    /**
     * Updates whole information for a node.
     *
     * @param info node info.
     * @return Error.
     */
    Error SetNodeInfo(const NodeInfo& info) override;

    /**
     * Updates node state.
     *
     * @param nodeID node identifier.
     * @param state node state.
     * @return Error.
     */
    Error SetNodeState(const String& nodeID, const NodeState& state) override;

    /**
     * Sets node connected state.
     *
     * @param nodeID node identifier.
     * @param isConnected connected state.
     * @return Error.
     */
    Error SetNodeConnected(const String& nodeID, bool isConnected) override;

    /**
     * Returns ids for all the nodes of the unit.
     *
     * @param[out] ids result node identifiers.
     * @return Error.
     */
    Error GetAllNodeIDs(Array<StaticString<cIDLen>>& ids) const override;

    /**
     * Returns info for specified node.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node information.
     * @return Error.
     */
    Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const override;

    /**
     * Subscribes node info notifications.
     *
     * @param listener node info listener.
     * @return Error.
     */
    Error SubscribeListener(iamclient::NodeInfoListenerItf& listener) override;

    /**
     * Unsubscribes from node info notifications.
     *
     * @param listener node info listener.
     * @return Error.
     */
    Error UnsubscribeListener(iamclient::NodeInfoListenerItf& listener) override;

private:
    static constexpr auto cAllocatorSize
        = sizeof(StaticArray<StaticString<cIDLen>, cMaxNumNodes>) + 2 * sizeof(NodeInfo);
    static constexpr auto cMaxNumListeners = 1;

    NodeInfo*               GetNodeFromCache(const String& nodeID);
    const NodeInfo*         GetNodeFromCache(const String& nodeID) const;
    Error                   UpdateStorage(const NodeInfo& info);
    Error                   UpdateCache(const NodeInfo& nodeInfo);
    RetWithError<NodeInfo*> AddNodeInfoToCache(const NodeInfo& info);
    void                    NotifyNodeInfoChange(const NodeInfo& nodeInfo);

    StorageItf*                                                    mStorage {};
    StaticArray<iamclient::NodeInfoListenerItf*, cMaxNumListeners> mListeners;
    StaticArray<NodeInfo, cMaxNumNodes>                            mNodeInfoCache;
    mutable Mutex                                                  mMutex;

    StaticAllocator<cAllocatorSize> mAllocator;
};

/** @}*/

} // namespace aos::iam::nodemanager

#endif
