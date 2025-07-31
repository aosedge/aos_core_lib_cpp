/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_MONITORINGMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_MONITORINGMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/monitoring/monitoring.hpp>

namespace aos::monitoring {

/**
 * Resource monitor mock.
 */
class ResourceMonitorMock : public ResourceMonitorItf {
public:
    MOCK_METHOD(Error, StartInstanceMonitoring,
        (const String& instanceID, const InstanceMonitorParams& monitoringConfig), (override));
    MOCK_METHOD(Error, UpdateInstanceRunState, (const String& instanceID, InstanceRunState runState), (override));
    MOCK_METHOD(Error, StopInstanceMonitoring, (const String& instanceID), (override));
    MOCK_METHOD(Error, GetAverageMonitoringData, (NodeMonitoringData & monitoringData), (override));
};

} // namespace aos::monitoring

#endif
