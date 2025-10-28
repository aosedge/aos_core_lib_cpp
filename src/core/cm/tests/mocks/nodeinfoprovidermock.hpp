/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>

namespace aos::cm::nodeinfoprovider {

/**
 * NodeInfoListenerItf mock.
 */
class NodeInfoListenerMock : public NodeInfoListenerItf {
public:
    MOCK_METHOD(void, OnNodeInfoChanged, (const UnitNodeInfo& info), (override));
};

/**
 * NodeInfoProviderItf mock.
 */
class NodeInfoProviderMock : public NodeInfoProviderItf {
public:
    MOCK_METHOD(Error, GetAllNodeIDs, (Array<StaticString<cIDLen>> & ids), (const, override));
    MOCK_METHOD(Error, GetNodeInfo, (const String& nodeID, UnitNodeInfo& nodeInfo), (const, override));
    MOCK_METHOD(Error, SubscribeListener, (NodeInfoListenerItf & listener), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (NodeInfoListenerItf & listener), (override));
};

} // namespace aos::cm::nodeinfoprovider

#endif
