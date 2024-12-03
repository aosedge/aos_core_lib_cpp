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

} // namespace resourcemanager
} // namespace sm
} // namespace aos

#endif
