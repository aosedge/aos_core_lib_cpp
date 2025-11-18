/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_NODEINFOPROVIDER_ITF_NODEINFOPROVIDER_HPP_
#define AOS_CORE_IAM_NODEINFOPROVIDER_ITF_NODEINFOPROVIDER_HPP_

#include <core/common/tools/error.hpp>
#include <core/common/types/common.hpp>

namespace aos::iam::nodeinfoprovider {

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
     * @param provisioned node provisioned flag.
     * @return Error.
     */
    virtual Error SetNodeState(const NodeState& state, bool provisioned) = 0;

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
