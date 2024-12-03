/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/sm/networkmanager.hpp"

using namespace aos::sm::networkmanager;
using namespace testing;

class MockTrafficMonitor : public TrafficMonitorItf {
public:
    MOCK_METHOD(aos::Error, Start, (), (override));
    MOCK_METHOD(aos::Error, Close, (), (override));
    MOCK_METHOD(void, SetPeriod, (int period), (override));
    MOCK_METHOD(aos::Error, StartInstanceMonitoring,
        (const aos::String& instanceID, const aos::String& IPAddress, uint64_t downloadLimit, uint64_t uploadLimit),
        (override));
    MOCK_METHOD(aos::Error, StopInstanceMonitoring, (const aos::String& instanceID), (override));
    MOCK_METHOD(aos::Error, GetSystemData, (uint64_t & inputTraffic, uint64_t& outputTraffic), (const, override));
    MOCK_METHOD(aos::Error, GetInstanceTraffic,
        (const aos::String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic), (const, override));
};
