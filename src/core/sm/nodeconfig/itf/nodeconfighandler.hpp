/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NODECONFIG_ITF_NODECONFIGHANDLER_HPP_
#define AOS_CORE_SM_NODECONFIG_ITF_NODECONFIGHANDLER_HPP_

#include <core/common/types/unitconfig.hpp>

namespace aos::sm::nodeconfig {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Node config handler interface.
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
     * @param config node config.
     * @return Error.
     */
    virtual Error CheckNodeConfig(const NodeConfig& config) = 0;

    /**
     * Updates node config.
     *
     * @param config node config.
     * @return Error.
     */
    virtual Error UpdateNodeConfig(const NodeConfig& config) = 0;

    /**
     * Returns node config status.
     *
     * @param[out] status node config status.
     * @return Error.
     */
    virtual Error GetNodeConfigStatus(NodeConfigStatus& status) = 0;
};

/** @}*/

} // namespace aos::sm::nodeconfig

#endif
