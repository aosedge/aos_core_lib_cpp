/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_LAUNCHER_ITF_INSTANCESTATUSSENDER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_INSTANCESTATUSSENDER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Instance status sender interface.
 */
class InstanceStatusSenderItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceStatusSenderItf() = default;

    /**
     * Sends node instances statuses.
     *
     * @param statuses instances statuses.
     */
    virtual void SendNodeInstancesStatuses(const Array<aos::InstanceStatus>& statuses) = 0;

    /**
     * Sends update instances statuses.
     *
     * @param statuses instances statuses.
     */
    virtual void SendUpdateInstancesStatuses(const Array<aos::InstanceStatus>& statuses) = 0;
};

} // namespace aos::sm::launcher

#endif
