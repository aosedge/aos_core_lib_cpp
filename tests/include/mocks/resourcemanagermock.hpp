/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_JSON_PROVIDER_MOCK_HPP_
#define AOS_JSON_PROVIDER_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/sm/resourcemanager.hpp"

namespace aos::sm::resourcemanager {

/**
 * JSON provider mock.
 */
class JSONProviderMock : public JSONProviderItf {
public:
    MOCK_METHOD(Error, NodeConfigToJSON, (const NodeConfig&, String&), (const override));
    MOCK_METHOD(Error, NodeConfigFromJSON, (const String&, NodeConfig&), (const override));
};

/**
 * Host device manager mock.
 */
class HostDeviceManagerMock : public HostDeviceManagerItf {
public:
    MOCK_METHOD(Error, CheckDevice, (const String&), (const override));
    MOCK_METHOD(Error, CheckGroup, (const String&), (const override));
};

/**
 * Node config receiver mock.
 */
class NodeConfigReceiverMock : public NodeConfigReceiverItf {
public:
    MOCK_METHOD(Error, ReceiveNodeConfig, (const NodeConfig&), (override));
};

/**
 * Resource manager mock.
 */

class ResourceManagerMock : public ResourceManagerItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cVersionLen>>, GetNodeConfigVersion, (), (const override));
    MOCK_METHOD(Error, GetNodeConfig, (aos::NodeConfig&), (const override));
    MOCK_METHOD(Error, GetDeviceInfo, (const String&, DeviceInfo&), (const override));
    MOCK_METHOD(Error, GetResourceInfo, (const String&, ResourceInfo&), (const override));
    MOCK_METHOD(Error, AllocateDevice, (const String&, const String&), (override));
    MOCK_METHOD(Error, ReleaseDevice, (const String&, const String&), (override));
    MOCK_METHOD(Error, ReleaseDevices, (const String&), (override));
    MOCK_METHOD(Error, ResetAllocatedDevices, (), (override));
    MOCK_METHOD(Error, GetDeviceInstances, (const String&, Array<StaticString<cInstanceIDLen>>&), (const override));
    MOCK_METHOD(Error, CheckNodeConfig, (const String&, const String&), (const override));
    MOCK_METHOD(Error, UpdateNodeConfig, (const String&, const String&), (override));
    MOCK_METHOD(Error, SubscribeCurrentNodeConfigChange, (NodeConfigReceiverItf&), (override));
    MOCK_METHOD(Error, UnsubscribeCurrentNodeConfigChange, (NodeConfigReceiverItf&), (override));
};

} // namespace aos::sm::resourcemanager

#endif
