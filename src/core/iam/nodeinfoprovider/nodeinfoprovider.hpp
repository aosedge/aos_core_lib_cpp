/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NODEINFOPROVIDER_HPP_
#define AOS_NODEINFOPROVIDER_HPP_

#include "aos/common/tools/error.hpp"
#include "aos/common/types.hpp"

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
 * Aos component cm.
 */
static constexpr auto cAosComponentCM = "cm";

/**
 * Aos component iam.
 */
static constexpr auto cAosComponentIAM = "iam";

/**
 * Aos component sm.
 */
static constexpr auto cAosComponentSM = "sm";

/**
 * Aos component um.
 */
static constexpr auto cAosComponentUM = "um";

/**
 * Checks if the node is the main node.
 *
 * @param nodeInfo node info.
 * @return bool.
 */
bool IsMainNode(const NodeInfo& nodeInfo);

/**
 * Node status observer interface.
 */
class NodeStatusObserverItf {
public:
    /**
     * On node status changed event.
     *
     * @param nodeID node id
     * @param status node status
     * @return Error
     */
    virtual Error OnNodeStatusChanged(const String& nodeID, const NodeStatus& status) = 0;

    /**
     * Destructor.
     */
    virtual ~NodeStatusObserverItf() = default;
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
     * Sets the node status.
     *
     * @param status node status
     * @return Error
     */
    virtual Error SetNodeStatus(const NodeStatus& status) = 0;

    /**
     * Subscribes on node status changed event.
     *
     * @param observer node status changed observer
     * @return Error
     */
    virtual Error SubscribeNodeStatusChanged(NodeStatusObserverItf& observer) = 0;

    /**
     * Unsubscribes from node status changed event.
     *
     * @param observer node status changed observer
     * @return Error
     */
    virtual Error UnsubscribeNodeStatusChanged(NodeStatusObserverItf& observer) = 0;

    /**
     * Destructor.
     */
    virtual ~NodeInfoProviderItf() = default;
};

} // namespace aos::iam::nodeinfoprovider

#endif
