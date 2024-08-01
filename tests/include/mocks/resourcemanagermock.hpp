/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_JSON_PROVIDER_MOCK_HPP_
#define AOS_JSON_PROVIDER_MOCK_HPP_

#include "aos/sm/resourcemanager.hpp"
#include <gmock/gmock.h>

namespace aos {
namespace sm {
namespace resourcemanager {

/**
 * JSON provider mock.
 */
class JSONProviderMock : public JSONProviderItf {
public:
    MOCK_METHOD(Error, DumpNodeConfig, (const NodeConfig&, String&), (const override));
    MOCK_METHOD(Error, ParseNodeConfig, (const String&, NodeConfig&), (const override));
};

/**
 * Host device manager mock.
 */
class HostDeviceManagerMock : public HostDeviceManagerItf {
public:
    MOCK_METHOD(Error, AllocateDevice, (const DeviceInfo&, const String&), (override));
    MOCK_METHOD(Error, RemoveInstanceFromDevice, (const String&, const String&), (override));
    MOCK_METHOD(Error, RemoveInstanceFromAllDevices, (const String&), (override));
    MOCK_METHOD(Error, GetDeviceInstances, (const String&, Array<StaticString<cInstanceIDLen>>&), (const override));
    MOCK_METHOD(bool, DeviceExists, (const String&), (const override));
};

/**
 * Host group manager mock.
 */
class HostGroupManagerMock : public HostGroupManagerItf {
public:
    MOCK_METHOD(bool, GroupExists, (const String&), (const override));
};

} // namespace resourcemanager
} // namespace sm
} // namespace aos

#endif
