/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_MONITORING_ITF_RECEIVER_HPP_
#define AOS_CORE_CM_MONITORING_ITF_RECEIVER_HPP_

#include <core/common/monitoring/monitoring.hpp>

namespace aos::cm::monitoring {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Receiver interface.
 */
class ReceiverItf {
public:
    /**
     * Destructor.
     */
    virtual ~ReceiverItf() = default;

    /**
     * Receives monitoring data.
     *
     * @param monitoring monitoring data.
     * @return Error.
     */
    virtual Error OnMonitoringReceived(const aos::monitoring::NodeMonitoringData& monitoring) = 0;
};

} // namespace aos::cm::monitoring

#endif
