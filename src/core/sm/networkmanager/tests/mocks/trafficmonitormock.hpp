/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_TRAFFICMONITORMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_TRAFFICMONITORMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/trafficmonitor.hpp>

namespace aos::sm::networkmanager {

class TrafficMonitorMock : public TrafficMonitorItf {
public:
    MOCK_METHOD(Error, Start, (), (override));
    MOCK_METHOD(Error, Stop, (), (override));
    MOCK_METHOD(void, SetPeriod, (TrafficPeriod), (override));
    MOCK_METHOD(Error, StartInstanceMonitoring, (const String&, const String&, uint64_t, uint64_t), (override));
    MOCK_METHOD(Error, StopInstanceMonitoring, (const String&), (override));
    MOCK_METHOD(Error, GetSystemData, (uint64_t&, uint64_t&), (const, override));
    MOCK_METHOD(Error, GetInstanceTraffic, (const String&, uint64_t&, uint64_t&), (const, override));
};

} // namespace aos::sm::networkmanager

#endif
