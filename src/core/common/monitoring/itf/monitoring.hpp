/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_ITF_MONITORING_HPP_
#define AOS_CORE_COMMON_MONITORING_ITF_MONITORING_HPP_

#include "monitoringdata.hpp"

namespace aos::monitoring {

/**
 * Monitoring interface.
 */
class MonitoringItf {
public:
    /**
     * Destructor.
     */
    virtual ~MonitoringItf() = default;

    /**
     * Returns average monitoring data.
     *
     * @param[out] monitoringData monitoring data.
     * @return Error.
     */
    virtual Error GetAverageMonitoringData(NodeMonitoringData& monitoringData) = 0;
};

} // namespace aos::monitoring

#endif
