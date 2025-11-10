/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/nodeinfoprovider/nodeinfoprovider.hpp>

namespace aos::iam::nodeinfoprovider {

/**
 * Node state observer mock.
 */
class NodeStateObserverMock : public NodeStateObserverItf {
public:
    MOCK_METHOD(Error, OnNodeStateChanged, (const String& nodeID, const NodeState& state), (override));
};

/**
 * Node info provider mock.
 */
class NodeInfoProviderMock : public NodeInfoProviderItf {
public:
    MOCK_METHOD(Error, GetNodeInfo, (NodeInfo & nodeInfo), (const, override));
    MOCK_METHOD(Error, SetNodeState, (const NodeState& state, bool provisioned), (override));
    MOCK_METHOD(Error, SubscribeNodeStateChanged, (NodeStateObserverItf & observer), (override));
    MOCK_METHOD(Error, UnsubscribeNodeStateChanged, (NodeStateObserverItf & observer), (override));
};

} // namespace aos::iam::nodeinfoprovider

#endif
