/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_ALERTS_ITF_PROVIDER_HPP_
#define AOS_CORE_CM_ALERTS_ITF_PROVIDER_HPP_

#include <core/common/types/alerts.hpp>

namespace aos::cm::alerts {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Alerts listener interface.
 */
class AlertsListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~AlertsListenerItf() = default;

    /**
     * Receives alert.
     *
     * @param alert alert.
     * @return Error.
     */
    virtual Error OnAlertReceived(const AlertVariant& alert) = 0;
};

/**
 * Alert provider interface.
 */
class AlertsProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~AlertsProviderItf() = default;

    /**
     * Subscribes alerts listener to specified tags.
     *
     * @param tags alert tags to subscribe to.
     * @param listener alerts listener to subscribe.
     * @return Error.
     */
    virtual Error SubscribeListener(const Array<AlertTag>& tags, AlertsListenerItf& listener) = 0;

    /**
     * Unsubscribes alerts listener.
     *
     * @param listener alerts listener to unsubscribe.
     * @return Error.
     */
    virtual Error UnsubscribeListener(AlertsListenerItf& listener) = 0;
};

} // namespace aos::cm::alerts

#endif
