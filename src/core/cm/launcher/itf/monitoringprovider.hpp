/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_MONITORINGPROVIDER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_MONITORINGPROVIDER_HPP_

#include <core/common/monitoring/monitoring.hpp>
#include <core/common/types/types.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Provides monitoring data.
 */
class MonitoringProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~MonitoringProviderItf() = default;

    /**
     * Returns average monitoring data for node.
     *
     * @param nodeID node ID.
     * @param monitoring average monitoring data.
     * @return Error.
     */
    virtual Error GetAverageMonitoring(const String& nodeID, monitoring::NodeMonitoringData& monitoring) const = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
