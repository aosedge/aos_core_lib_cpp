/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_NODEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_NODEMANAGER_ITF_STORAGE_HPP_

#include <core/common/types/common.hpp>

namespace aos::iam::nodemanager {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Node info storage interface.
 */
class NodeInfoStorageItf {
public:
    /**
     * Updates whole information for a node.
     *
     * @param info node info.
     * @return Error.
     */
    virtual Error SetNodeInfo(const NodeInfo& info) = 0;

    /**
     * Returns node info.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node identifier.
     * @return Error.
     */
    virtual Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const = 0;

    /**
     * Returns ids for all the node in the manager.
     *
     * @param ids result node identifiers.
     * @return Error.
     */
    virtual Error GetAllNodeIDs(Array<StaticString<cIDLen>>& ids) const = 0;

    /**
     * Removes node info by its id.
     *
     * @param nodeID node identifier.
     * @return Error.
     */
    virtual Error RemoveNodeInfo(const String& nodeID) = 0;

    /**
     * Destroys object instance.
     */
    virtual ~NodeInfoStorageItf() = default;
};

/** @}*/

} // namespace aos::iam::nodemanager

#endif
