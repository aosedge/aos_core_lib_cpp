/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NODEMANAGER_MOCK_HPP_
#define NODEMANAGER_MOCK_HPP_

#include <gmock/gmock.h>

#include <aos/iam/nodemanager.hpp>

namespace aos::iam::nodemanager {

/**
 * Node manager mock.
 */
class NodeManagerMock : public NodeManagerItf {
public:
    MOCK_METHOD(Error, SetNodeInfo, (const NodeInfo&), (override));
    MOCK_METHOD(Error, SetNodeStatus, (const String&, NodeStatus), (override));
    MOCK_METHOD(Error, GetNodeInfo, (const String&, NodeInfo&), (const, override));
    MOCK_METHOD(Error, GetAllNodeIds, (Array<StaticString<cNodeIDLen>>&), (const, override));
    MOCK_METHOD(Error, RemoveNodeInfo, (const String&), (override));
    MOCK_METHOD(Error, SubscribeNodeInfoChange, (NodeInfoListenerItf&), (override));
};

} // namespace aos::iam::nodemanager

#endif
