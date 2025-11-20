/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_AVERAGE_HPP_
#define AOS_CORE_COMMON_MONITORING_AVERAGE_HPP_

#include <core/common/tools/map.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/types/monitoring.hpp>

#include "itf/instanceparamsprovider.hpp"
#include "itf/monitoringdata.hpp"

namespace aos::monitoring {

/**
 * Average class.
 */
class Average {
public:
    /**
     * Initializes average.
     *
     * @param windowCount window count.
     * @return Error.
     */
    Error Init(size_t windowCount);

    /**
     * Updates average data.
     *
     * @param data monitoring data.
     * @return Error.
     */
    Error Update(const NodeMonitoringData& nodeMonitoring);

    /**
     * Returns average data.
     *
     * @param[out] data monitoring data.
     * @return Error.
     */
    Error GetData(NodeMonitoringData& data) const;

    /**
     * Starts instance monitoring.
     *
     * @param instanceIdent instance identification.
     * @return Error.
     */
    Error StartInstanceMonitoring(const InstanceIdent& instanceIdent);

    /**
     * Stops instance monitoring.
     *
     * @param instanceIdent instance identification.
     * @return Error.
     */
    Error StopInstanceMonitoring(const InstanceIdent& instanceIdent);

private:
    struct AverageData {
        bool               mIsInitialized {};
        MonitoringData     mMonitoringData;
        PartitionInfoArray mMonitoredPartitions;
    };

    static constexpr auto cAllocatorSize = sizeof(AverageData);

    Error UpdateMonitoringData(MonitoringData& data, const MonitoringData& newData, bool& isInitialized);
    Error GetMonitoringData(MonitoringData& data, const MonitoringData& averageData) const;

    size_t                                                  mWindowCount {};
    AverageData                                             mAverageNodeData {};
    StaticMap<InstanceIdent, AverageData, cMaxNumInstances> mAverageInstancesData {};

    StaticAllocator<cAllocatorSize> mAllocator;
};

} // namespace aos::monitoring

#endif
