/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_NODEINFOPROVIDER_HPP_
#define AOS_CM_NODEINFOPROVIDER_HPP_

#include <aos/common/types.hpp>

namespace aos::cm::nodeinfoprovider {

/** @addtogroup CM NodeInfoProvider
 *  @{
 */

/**
 * Node information provider interface.
 */
class NodeInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeInfoProviderItf() = default;

    /**
     * Retrieves current node identifier.
     *
     * @return StaticString<cNodeIDLen>.
     */
    virtual StaticString<cNodeIDLen> GetCurrentNodeID() const = 0;

    /**
     * Returns info for specified node.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node information.
     * @return Error.
     */
    virtual Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const = 0;

    /**
     * Returns ids for all the nodes of the unit.
     *
     * @param[out] ids result node identifiers.
     * @return Error.
     */
    virtual Error GetAllNodeIds(Array<StaticString<cNodeIDLen>>& ids) const = 0;
};

/** @}*/

} // namespace aos::cm::nodeinfoprovider

#endif
