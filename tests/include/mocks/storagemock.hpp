/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/sm/networkmanager.hpp"

using namespace aos::sm::networkmanager;
using namespace testing;

class MockStorageItf : public StorageItf {
public:
    MOCK_METHOD(aos::Error, RemoveNetworkInfo, (const aos::String& networkID), (override));
    MOCK_METHOD(aos::Error, AddNetworkInfo, (const NetworkParameters& info), (override));
    MOCK_METHOD(aos::Error, GetNetworksInfo, (aos::Array<NetworkParameters> & networks), (const, override));
    MOCK_METHOD(aos::Error, SetTrafficMonitorData, (const aos::String& chain, const aos::Time& time, uint64_t value),
        (override));
    MOCK_METHOD(aos::Error, GetTrafficMonitorData, (const aos::String& chain, aos::Time& time, uint64_t& value),
        (const, override));
    MOCK_METHOD(aos::Error, RemoveTrafficMonitorData, (const aos::String& chain), (override));
};
