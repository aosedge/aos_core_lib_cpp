/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_CURRENTNODEINFOPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_CURRENTNODEINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/currentnodeinfoprovider.hpp>

namespace aos::iamclient {

/**
 * Current node info listener mock.
 */
class CurrentNodeInfoListenerMock : public CurrentNodeInfoListenerItf {
public:
    MOCK_METHOD(void, OnCurrentNodeInfoChanged, (const NodeInfo&), (override));
};

/**
 * Current node info provider mock.
 */
class CurrentNodeInfoProviderMock : public CurrentNodeInfoProviderItf {
public:
    MOCK_METHOD(Error, GetCurrentNodeInfo, (NodeInfo&), (const, override));
    MOCK_METHOD(Error, SubscribeListener, (CurrentNodeInfoListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (CurrentNodeInfoListenerItf&), (override));
};

} // namespace aos::iamclient

#endif
