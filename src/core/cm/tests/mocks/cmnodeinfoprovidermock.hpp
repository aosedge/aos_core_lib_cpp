/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/nodeinfoprovider/nodeinfoprovider.hpp>

namespace aos::cm::nodeinfoprovider {

/**
 * Node info provider mock.
 */
class NodeInfoProviderMock : public NodeInfoProviderItf {
public:
    MOCK_METHOD(StaticString<cNodeIDLen>, GetCurrentNodeID, (), (const, override));
    MOCK_METHOD(Error, GetNodeInfo, (const String& nodeID, NodeInfo& nodeInfo), (const, override));
    MOCK_METHOD(Error, GetAllNodeIds, (Array<StaticString<cNodeIDLen>> & ids), (const, override));
};

} // namespace aos::cm::nodeinfoprovider

#endif // AOS_CORE_CM_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_
