/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITF_INSTANCESTATUSRECEIVER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_INSTANCESTATUSRECEIVER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Receives instance status updates.
 */
class InstanceStatusReceiverItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceStatusReceiverItf() = default;

    /**
     * Receives instances statuses.
     *
     * @param statuses instances statuses.
     * @return Error.
     */
    virtual Error OnInstancesStatusesReceived(const Array<InstanceStatus>& statuses) = 0;

    /**
     * Notifies that runtime requires reboot.
     *
     * @param runtimeID runtime identifier.
     * @return Error.
     */
    virtual Error RebootRequired(const String& runtimeID) = 0;
};

} // namespace aos::sm::launcher

#endif
