/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_ITF_NODEMONITORINGPROVIDER_HPP_
#define AOS_CORE_COMMON_MONITORING_ITF_NODEMONITORINGPROVIDER_HPP_

#include <core/common/types/monitoring.hpp>

namespace aos::monitoring {

/**
 * Node monitoring provider interface.
 */
class NodeMonitoringProviderItf {
public:
    /**
     * Destructor
     */
    virtual ~NodeMonitoringProviderItf() = default;

    /**
     * Returns node monitoring data.
     *
     * @param partitionInfos partition infos.
     * @param[out] monitoringData monitoring data.
     * @return Error.
     */
    virtual Error GetNodeMonitoringData(MonitoringData& monitoringData) = 0;
};

} // namespace aos::monitoring

#endif
