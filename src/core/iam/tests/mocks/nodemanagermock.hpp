/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_NODEMANAGERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_NODEMANAGERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/nodemanager/itf/nodemanager.hpp>

namespace aos::iam::nodemanager {

/**
 * Node manager mock.
 */
class NodeManagerMock : public NodeManagerItf {
public:
    MOCK_METHOD(Error, SetNodeInfo, (const NodeInfo&), (override));
    MOCK_METHOD(Error, SetNodeState, (const String&, const NodeState&, bool provisioned), (override));
    MOCK_METHOD(Error, GetNodeInfo, (const String&, NodeInfo&), (const, override));
    MOCK_METHOD(Error, GetAllNodeIDs, (Array<StaticString<cIDLen>>&), (const, override));
    MOCK_METHOD(Error, RemoveNodeInfo, (const String&), (override));
    MOCK_METHOD(Error, SubscribeNodeInfoChange, (NodeInfoListenerItf&), (override));
};

} // namespace aos::iam::nodemanager

#endif
