/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UNITCONFIG_ITF_NODECONFIGPROVIDER_HPP_
#define AOS_CORE_CM_UNITCONFIG_ITF_NODECONFIGPROVIDER_HPP_

#include <core/common/types/unitconfig.hpp>

namespace aos::cm::unitconfig {

/** @addtogroup cm Communication Manager
 *  @{
 */

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
     * Returns node config by either node id or type.
     *
     * @param nodeID node identifier.
     * @param nodeType node type.
     * @param[out] nodeConfig param to store node config.
     * @return Error.
     */
    virtual Error GetNodeConfig(const String& nodeID, const String& nodeType, NodeConfig& nodeConfig) = 0;
};

/** @}*/

} // namespace aos::cm::unitconfig

#endif
