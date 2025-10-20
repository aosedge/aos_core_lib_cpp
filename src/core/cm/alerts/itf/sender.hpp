/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_ALERTS_ITF_SENDER_HPP_
#define AOS_CORE_CM_ALERTS_ITF_SENDER_HPP_

#include <core/common/types/alerts.hpp>

namespace aos::cm::updatemanager {

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
     * Sends alerts.
     *
     * @param alerts alerts.
     * @return Error.
     */
    virtual Error SendAlerts(const Alerts& alerts) = 0;
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
