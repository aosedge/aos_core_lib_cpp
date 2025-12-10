/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_NODEMANAGER_ITF_NODEMANAGER_HPP_
#define AOS_CORE_NODEMANAGER_ITF_NODEMANAGER_HPP_

#include <core/common/iamclient/itf/nodeinfoprovider.hpp>
#include <core/common/types/common.hpp>

namespace aos::iam::nodemanager {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Node manager interface.
 */
class NodeManagerItf : public iamclient::NodeInfoProviderItf {
public:
    /**
     * Updates whole information for a node.
     *
     * @param info node info.
     * @return Error.
     */
    virtual Error SetNodeInfo(const NodeInfo& info) = 0;

    /**
     * Updates node state.
     *
     * @param nodeID node identifier.
     * @param state node state.
     * @return Error.
     */
    virtual Error SetNodeState(const String& nodeID, const NodeState& state) = 0;

    /**
     * Sets node connected state.
     *
     * @param nodeID node identifier.
     * @param isConnected connected state.
     * @return Error.
     */
    virtual Error SetNodeConnected(const String& nodeID, bool isConnected) = 0;

    /**
     * Destroys object instance.
     */
    virtual ~NodeManagerItf() = default;
};

/** @}*/

} // namespace aos::iam::nodemanager

#endif
