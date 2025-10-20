/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_MONITORING_ITF_SENDER_HPP_
#define AOS_CORE_CM_MONITORING_ITF_SENDER_HPP_

#include <core/common/types/monitoring.hpp>

namespace aos::cm::monitoring {

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
     * Sends monitoring.
     *
     * @param monitoring monitoring.
     * @return Error.
     */
    virtual Error SendMonitoring(const Monitoring& monitoring) = 0;
};

/** @}*/

} // namespace aos::cm::monitoring

#endif
