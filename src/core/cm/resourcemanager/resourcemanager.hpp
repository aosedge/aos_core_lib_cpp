/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_RESOURCEMANAGER_HPP_
#define AOS_CM_RESOURCEMANAGER_HPP_

#include <aos/common/types.hpp>

namespace aos::cm::resourcemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Resource manager interface.
 */
class ResourceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ResourceManagerItf() = default;

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

} // namespace aos::cm::resourcemanager

#endif
