/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STATUSHANDLER_ITF_SENDER_HPP_
#define AOS_CORE_CM_STATUSHANDLER_ITF_SENDER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::cm::statushandler {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;

    /**
     * Sends a single instance status to node.
     *
     * @param nodeID destination node identifier.
     * @param status instance status.
     * @return Error.
     */
    virtual Error SendInstanceStatus(const String& nodeID, const InstanceStatus& status) = 0;

    /**
     * Sends node instances statuses to node.
     *
     * @param nodeID destination node identifier.
     * @param srcNodeID source node identifier.
     * @param statuses instances statuses.
     * @return Error.
     */
    virtual Error SendNodeInstancesStatuses(
        const String& nodeID, const String& srcNodeID, const Array<InstanceStatus>& statuses)
        = 0;

    /**
     * Sends other nodes instances statuses to node.
     *
     * @param nodeID destination node identifier.
     * @param statuses instances statuses.
     * @return Error.
     */
    virtual Error SendOtherNodesInstancesStatuses(const String& nodeID, const Array<InstanceStatus>& statuses) = 0;
};

/** @}*/

} // namespace aos::cm::statushandler

#endif
