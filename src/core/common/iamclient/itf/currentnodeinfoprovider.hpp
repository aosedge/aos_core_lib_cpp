/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IAMCLIENT_ITF_CURRENTNODEINFOPROVIDER_HPP_
#define AOS_CORE_COMMON_IAMCLIENT_ITF_CURRENTNODEINFOPROVIDER_HPP_

#include <core/common/types/common.hpp>

namespace aos::iamclient {

/**
 * Interface for receiving notification about changing current node information.
 */
class CurrentNodeInfoListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~CurrentNodeInfoListenerItf() = default;

    /**
     * Notifies about current node info changed.
     *
     * @param info current node information.
     */
    virtual void OnCurrentNodeInfoChanged(const NodeInfo& info) = 0;
};

/**
 * Current node information provider interface.
 */
class CurrentNodeInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~CurrentNodeInfoProviderItf() = default;

    /**
     * Returns current node info    .
     *
     * @param[out] nodeInfo current node information.
     * @return Error.
     */
    virtual Error GetCurrentNodeInfo(NodeInfo& nodeInfo) const = 0;

    /**
     * Subscribes current node info notifications.
     *
     * @param listener current node info listener.
     * @return Error.
     */
    virtual Error SubscribeListener(CurrentNodeInfoListenerItf& listener) = 0;

    /**
     * Unsubscribes from current node info notifications.
     *
     * @param listener current node info listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(CurrentNodeInfoListenerItf& listener) = 0;
};

} // namespace aos::iamclient

#endif
