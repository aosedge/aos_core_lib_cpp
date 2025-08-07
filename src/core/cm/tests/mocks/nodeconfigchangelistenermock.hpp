/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_NODECONFIGCHANGELISTENERMOCK_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_NODECONFIGCHANGELISTENERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/unitconfig/unitconfig.hpp>

namespace aos::cm::unitconfig {

/**
 * Node config change listener mock.
 */
class NodeConfigChangeListenerMock : public NodeConfigChangeListenerItf {
public:
    MOCK_METHOD(void, OnNodeConfigChange, (const cloudprotocol::NodeConfig& config), (override));
};

} // namespace aos::cm::unitconfig

#endif // AOS_CORE_CM_TESTS_MOCKS_NODECONFIGCHANGELISTENERMOCK_HPP_