/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NODEINFOPROVIDER_MOCK_HPP_
#define AOS_NODEINFOPROVIDER_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/iam/nodeinfoprovider.hpp"

namespace aos::iam::nodeinfoprovider {

/**
 * Node status observer mock.
 */
class NodeStatusObserverMock : public NodeStatusObserverItf {
public:
    MOCK_METHOD(Error, OnNodeStatusChanged, (const String& nodeID, const NodeStatus& status), (override));
};

/**
 * Node info provider mock.
 */
class NodeInfoProviderMock : public NodeInfoProviderItf {
public:
    MOCK_METHOD(Error, GetNodeInfo, (NodeInfo & nodeInfo), (const, override));
    MOCK_METHOD(Error, SetNodeStatus, (const NodeStatus& nodeInfo), (override));
    MOCK_METHOD(Error, SubscribeNodeStatusChanged, (NodeStatusObserverItf & observer), (override));
    MOCK_METHOD(Error, UnsubscribeNodeStatusChanged, (NodeStatusObserverItf & observer), (override));
};

} // namespace aos::iam::nodeinfoprovider

#endif
