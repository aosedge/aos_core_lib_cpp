/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_STATUSHANDLER_ITF_HANDLER_HPP_
#define AOS_CORE_SM_STATUSHANDLER_ITF_HANDLER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::sm::statushandler {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Handler interface.
 */
class HandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~HandlerItf() = default;

    /**
     * Sets instances statuses of other nodes.
     *
     * @param statuses instances statuses.
     * @return Error.
     */
    virtual Error SetOtherNodesInstancesStatuses(const Array<InstanceStatus>& statuses) = 0;
};

/** @}*/

} // namespace aos::sm::statushandler

#endif
