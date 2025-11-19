/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_NODECONFIG_ITF_NODECONFIGPROVIDER_HPP_
#define AOS_CORE_COMMON_NODECONFIG_ITF_NODECONFIGPROVIDER_HPP_

#include <core/common/types/unitconfig.hpp>

namespace aos::nodeconfig {

/**
 * Node config listener interface.
 */
class NodeConfigListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigListenerItf() = default;

    /**
     * Notifies about node config change.
     *
     * @return Error.
     */
    virtual Error OnNodeConfigChanged(const NodeConfig& nodeConfig) = 0;
};

/**
 * Provides node configuration.
 */
class NodeConfigProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigProviderItf() = default;

    /**
     * Returns current node config.
     *
     * @param[out] nodeConfig param to store node config.
     * @return Error.
     */
    virtual Error GetNodeConfig(NodeConfig& nodeConfig) = 0;

    /**
     * Subscribes listener for node config changes.
     *
     * @param listener listener to subscribe.
     * @return Error.
     */
    virtual Error SubscribeListener(NodeConfigListenerItf& listener) = 0;

    /**
     * Unsubscribes listener from node config changes.
     *
     * @param listener listener to unsubscribe.
     * @return Error.
     */
    virtual Error UnsubscribeListener(NodeConfigListenerItf& listener) = 0;
};

} // namespace aos::nodeconfig

#endif
