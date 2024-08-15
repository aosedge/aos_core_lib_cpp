/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_AVERAGE_HPP_
#define AOS_AVERAGE_HPP_

#include "aos/common/monitoring/monitoring.hpp"
#include "aos/common/tools/map.hpp"

namespace aos::monitoring {

/**
 * Average class.
 */
class Average {
public:
    /**
     * Initializes average.
     *
     * @param nodeDisks node disks info.
     * @param windowCount window count.
     * @return Error.
     */
    Error Init(const PartitionInfoStaticArray& nodeDisks, size_t windowCount);

    /**
     * Updates average data.
     *
     * @param data monitoring data.
     * @return Error.
     */
    Error Update(const NodeMonitoringData& data);

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
     * @param monitoringConfig monitoring config.
     * @return Error.
     */
    Error StartInstanceMonitoring(const InstanceMonitorParams& monitoringConfig);

    /**
     * Stops instance monitoring.
     *
     * @param instanceIdent instance identification.
     * @return Error.
     */
    Error StopInstanceMonitoring(const InstanceIdent& instanceIdent);

private:
    struct AverageData {
        bool           mIsInitialized = false;
        MonitoringData mMonitoringData;
    };

    Error UpdateMonitoringData(MonitoringData& data, const MonitoringData& newData, bool& isInitialized);
    Error GetMonitoringData(MonitoringData& data, const MonitoringData& averageData) const;

    size_t                                                  mWindowCount = 0;
    AverageData                                             mAverageNodeData {};
    StaticMap<InstanceIdent, AverageData, cMaxNumInstances> mAverageInstancesData {};
};

} // namespace aos::monitoring

#endif
