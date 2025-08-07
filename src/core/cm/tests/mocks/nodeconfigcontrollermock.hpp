/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_NODECONFIGCONTROLLERMOCK_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_NODECONFIGCONTROLLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/unitconfig/unitconfig.hpp>

namespace aos::cm::unitconfig {

/**
 * Node config controller mock.
 */
class NodeConfigControllerMock : public NodeConfigControllerItf {
public:
    MOCK_METHOD(Error, CheckNodeConfig,
        (const String& nodeID, const String& version, const cloudprotocol::NodeConfig& config), (override));
    MOCK_METHOD(Error, SetNodeConfig,
        (const String& nodeID, const String& version, const cloudprotocol::NodeConfig& config), (override));
    MOCK_METHOD(Error, GetNodeConfigStatuses, (Array<NodeConfigStatus> & statuses), (override));
    MOCK_METHOD(void, SubscribeNodeConfigStatus, (NodeConfigStatusListenerItf * listener), (override));
    MOCK_METHOD(void, UnsubscribeNodeConfigStatus, (NodeConfigStatusListenerItf * listener), (override));
};

} // namespace aos::cm::unitconfig

#endif // AOS_CORE_CM_TESTS_MOCKS_NODECONFIGCONTROLLERMOCK_HPP_
