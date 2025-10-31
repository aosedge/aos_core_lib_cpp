/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_NODECONFIGHANDLERMOCK_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_NODECONFIGHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/unitconfig/itf/nodeconfighandler.hpp>

namespace aos::cm::unitconfig {

/**
 * Node config handler mock.
 */
class NodeConfigHandlerMock : public NodeConfigHandlerItf {
public:
    MOCK_METHOD(Error, CheckNodeConfig, (const String& nodeID, const aos::NodeConfig& config), (override));
    MOCK_METHOD(Error, UpdateNodeConfig, (const String& nodeID, const aos::NodeConfig& config), (override));
    MOCK_METHOD(Error, GetNodeConfigStatus, (const String& nodeID, aos::NodeConfigStatus& status), (override));
};

} // namespace aos::cm::unitconfig

#endif // AOS_CORE_CM_TESTS_MOCKS_NODECONFIGHANDLERMOCK_HPP_
