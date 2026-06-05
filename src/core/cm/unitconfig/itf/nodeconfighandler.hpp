/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UNITCONFIG_ITF_NODECONFIGHANDLER_HPP_
#define AOS_CORE_CM_UNITCONFIG_ITF_NODECONFIGHANDLER_HPP_

#include <core/common/types/unitconfig.hpp>

namespace aos::cm::unitconfig {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Provides node configuration.
 */
class NodeConfigHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigHandlerItf() = default;

    /**
     * Checks node config.
     *
     * @param nodeID node ID.
     * @param config node config.
     * @return Error.
     */
    virtual Error CheckNodeConfig(const String& nodeID, const NodeConfig& config) = 0;

    /**
     * Updates node config.
     *
     * @param nodeID node ID.
     * @param config node config.
     * @return Error.
     */
    virtual Error UpdateNodeConfig(const String& nodeID, const NodeConfig& config) = 0;

    /**
     * Returns node config status.
     *
     * @param nodeID node ID.
     * @param[out] status node config status.
     * @return Error.
     */
    virtual Error GetNodeConfigStatus(const String& nodeID, NodeConfigStatus& status) = 0;
};

/** @}*/

} // namespace aos::cm::unitconfig

#endif
