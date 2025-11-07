/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_NODEINFOPROVIDER_NODEINFOPROVIDER_HPP_
#define AOS_CORE_IAM_NODEINFOPROVIDER_NODEINFOPROVIDER_HPP_

#include <core/common/tools/error.hpp>
#include <core/common/types/common.hpp>

namespace aos::iam::nodeinfoprovider {

/**
 * Main node attribute.
 */
static constexpr auto cAttrMainNode = "MainNode";

/**
 * Aos components attribute.
 */
static constexpr auto cAttrAosComponents = "AosComponents";

/**
 * Node runners attribute.
 */
static constexpr auto cAttrNodeRunners = "NodeRunners";

/**
 * Checks if the node is the main node.
 *
 * @param nodeInfo node info.
 * @return bool.
 */
bool IsMainNode(const NodeInfo& nodeInfo);

/**
 * Checks if the node contains specified component.
 *
 * @param nodeInfo node info.
 * @param component searched component.
 * @return bool.
 */
bool ContainsComponent(const NodeInfo& nodeInfo, const CoreComponent& component);

/**
 * Node state observer interface.
 */
class NodeStateObserverItf {
public:
    /**
     * On node state changed event.
     *
     * @param nodeID node id.
     * @param state node state.
     * @return Error.
     */
    virtual Error OnNodeStateChanged(const String& nodeID, const NodeState& state) = 0;

    /**
     * Destructor.
     */
    virtual ~NodeStateObserverItf() = default;
};

/**
 * Node info provider interface.
 */
class NodeInfoProviderItf {
public:
    /**
     * Gets the node info object.
     *
     * @param[out] nodeInfo node info
     * @return Error
     */
    virtual Error GetNodeInfo(NodeInfo& nodeInfo) const = 0;

    /**
     * Sets the node state.
     *
     * @param state node state.
     * @return Error.
     */
    virtual Error SetNodeState(const NodeState& state) = 0;

    /**
     * Subscribes on node state changed event.
     *
     * @param observer node state changed observer
     * @return Error
     */
    virtual Error SubscribeNodeStateChanged(NodeStateObserverItf& observer) = 0;

    /**
     * Unsubscribes from node state changed event.
     *
     * @param observer node state changed observer
     * @return Error
     */
    virtual Error UnsubscribeNodeStateChanged(NodeStateObserverItf& observer) = 0;

    /**
     * Destructor.
     */
    virtual ~NodeInfoProviderItf() = default;
};

} // namespace aos::iam::nodeinfoprovider

#endif
