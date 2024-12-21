/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_MONITORING_STUB_HPP_
#define AOS_MONITORING_STUB_HPP_

#include "aos/common/monitoring/monitoring.hpp"

namespace aos::monitoring {
/**
 * Resource monitor stub.
 */
class ResourceMonitorStub : public ResourceMonitorItf {
public:
    /**
     * Starts instance monitoring.
     *
     * @param instanceID instance ID.
     * @param monitoringConfig monitoring config.
     * @return Error.
     */
    Error StartInstanceMonitoring(const String& instanceID, const InstanceMonitorParams& monitoringConfig) override
    {
        (void)instanceID;
        (void)monitoringConfig;

        return ErrorEnum::eNone;
    }

    /**
     * Stops instance monitoring.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    Error StopInstanceMonitoring(const String& instanceID) override
    {
        (void)instanceID;

        return ErrorEnum::eNone;
    }

    /**
     * Returns average monitoring data.
     *
     * @param[out] monitoringData monitoring data.
     * @return Error.
     */
    Error GetAverageMonitoringData(NodeMonitoringData& monitoringData) override
    {
        (void)monitoringData;

        return ErrorEnum::eNone;
    }
};

} // namespace aos::monitoring

#endif
