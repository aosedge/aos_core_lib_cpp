/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IAMCLIENT_ITF_NODEINFOPROVIDER_HPP_
#define AOS_CORE_CM_IAMCLIENT_ITF_NODEINFOPROVIDER_HPP_

#include <core/common/types/obsolete.hpp>

namespace aos::cm::iamclient {
/**
 * Interface for receiving notification about changing node information.
 */
class NodeInfoListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeInfoListenerItf() = default;

    /**
     * Notifies about node info changed.
     *
     * @param info node information.
     */
    virtual void OnNodeInfoChanged(const NodeInfo& info) = 0;
};

/**
 * Node infor provider interface.
 */
class NodeInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeInfoProviderItf() = default;

    /**
     * Returns ids for all the nodes of the unit.
     *
     * @param[out] ids result node identifiers.
     * @return Error.
     */
    virtual Error GetAllNodeIds(Array<StaticString<cNodeIDLen>>& ids) const = 0;

    /**
     * Returns info for specified node.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node information.
     * @return Error.
     */
    virtual Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const = 0;

    /**
     * Subscribes node info notifications.
     *
     * @param listener node info listener.
     * @return Error.
     */
    virtual Error SubscribeListener(NodeInfoListenerItf& listener) = 0;

    /**
     * Unsubscribes from node info notifications.
     *
     * @param listener node info listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(NodeInfoListenerItf& listener) = 0;
};

} // namespace aos::cm::iamclient

#endif
