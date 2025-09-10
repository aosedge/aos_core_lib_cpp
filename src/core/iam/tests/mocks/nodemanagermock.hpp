/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_NODEMANAGERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_NODEMANAGERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/nodemanager/nodemanager.hpp>

namespace aos::iam::nodemanager {

/**
 * Node manager mock.
 */
class NodeManagerMock : public NodeManagerItf {
public:
    MOCK_METHOD(Error, SetNodeInfo, (const NodeInfoObsolete&), (override));
    MOCK_METHOD(Error, SetNodeState, (const String&, NodeStateObsolete), (override));
    MOCK_METHOD(Error, GetNodeInfo, (const String&, NodeInfoObsolete&), (const, override));
    MOCK_METHOD(Error, GetAllNodeIds, (Array<StaticString<cNodeIDLen>>&), (const, override));
    MOCK_METHOD(Error, RemoveNodeInfo, (const String&), (override));
    MOCK_METHOD(Error, SubscribeNodeInfoChange, (NodeInfoListenerItf&), (override));
};

} // namespace aos::iam::nodemanager

#endif
