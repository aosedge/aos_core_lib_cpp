/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_NODEINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/nodeinfoprovider.hpp>

namespace aos::iamclient {

/**
 * Node info listener mock.
 */
class NodeInfoListenerMock : public NodeInfoListenerItf {
public:
    MOCK_METHOD(void, OnNodeInfoChanged, (const NodeInfo&), (override));
};

/**
 * Node info provider mock.
 */
class NodeInfoProviderMock : public NodeInfoProviderItf {
public:
    MOCK_METHOD(Error, GetAllNodeIDs, (Array<StaticString<cIDLen>>&), (const, override));
    MOCK_METHOD(Error, GetNodeInfo, (const String&, NodeInfo&), (const, override));
    MOCK_METHOD(Error, SubscribeListener, (NodeInfoListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (NodeInfoListenerItf&), (override));
};

} // namespace aos::iamclient

#endif
