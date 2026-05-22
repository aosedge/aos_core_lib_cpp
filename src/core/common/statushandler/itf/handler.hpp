/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_STATUSHANDLER_ITF_HANDLER_HPP_
#define AOS_CORE_COMMON_STATUSHANDLER_ITF_HANDLER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::statushandler {

/**
 * Interface for handling instance statuses.
 */
class HandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~HandlerItf() = default;

    /**
     * Sets status for a single instance.
     *
     * @param status instance status.
     * @return Error.
     */
    virtual Error SetInstanceStatus(const InstanceStatus& status) = 0;

    /**
     * Sets node instances statuses.
     *
     * @param nodeID node identifier.
     * @param statuses instances statuses.
     * @return Error.
     */
    virtual Error SetNodeInstancesStatuses(const String& nodeID, const Array<InstanceStatus>& statuses) = 0;

    /**
     * Clears orphan statuses.
     *
     * @return Error.
     */
    virtual Error ClearOrphanStatuses() = 0;
};

} // namespace aos::statushandler

#endif
