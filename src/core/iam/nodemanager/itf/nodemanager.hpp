/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_NODEMANAGER_ITF_NODEMANAGER_HPP_
#define AOS_CORE_NODEMANAGER_ITF_NODEMANAGER_HPP_

#include <core/common/types/common.hpp>

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
     * @param provisioned node provisioned flag.
     * @return Error.
     */
    virtual Error SetNodeState(const String& nodeID, const NodeState& state, bool provisioned) = 0;

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
    virtual Error GetAllNodeIDs(Array<StaticString<cIDLen>>& ids) const = 0;

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

/** @}*/

} // namespace aos::iam::nodemanager

#endif
