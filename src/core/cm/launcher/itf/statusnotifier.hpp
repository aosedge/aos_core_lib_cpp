/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_STATUSLISTENER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_STATUSLISTENER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Interface for receiving notification about instance statuses.
 */
class StatusListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~StatusListenerItf() = default;

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
class StatusNotifierItf {
    /**
     * Destructor.
     */
    virtual ~StatusNotifierItf() = default;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error SubscribeListener(StatusListenerItf& listener) = 0;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(StatusListenerItf& listener) = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
