/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_ITF_SENDER_HPP_
#define AOS_CORE_COMMON_MONITORING_ITF_SENDER_HPP_

#include "monitoringdata.hpp"

namespace aos::monitoring {

/**
 * Monitor sender interface.
 */
class SenderItf {
public:
    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;

    /**
     * Sends monitoring data.
     *
     * @param monitoringData monitoring data.
     * @return Error.
     */
    virtual Error SendMonitoringData(const NodeMonitoringData& monitoringData) = 0;
};

} // namespace aos::monitoring

#endif
