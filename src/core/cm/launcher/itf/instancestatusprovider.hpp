/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_INSTANCESTATUSPROVIER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_INSTANCESTATUSPROVIER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Interface for receiving notification about instance statuses.
 */
class InstanceStatusListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceStatusListenerItf() = default;

    /**
     * Notifies about instances statuses change.
     *
     * @param statuses instances statuses.
     */
    virtual void OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses) = 0;
};

/**
 * Interface for notifying about instance statuses.
 */
class InstanceStatusProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceStatusProviderItf() = default;

    /**
     * Returns current statuses of running instances.
     *
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    virtual Error GetInstancesStatuses(Array<InstanceStatus>& statuses) = 0;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error SubscribeListener(InstanceStatusListenerItf& listener) = 0;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(InstanceStatusListenerItf& listener) = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
