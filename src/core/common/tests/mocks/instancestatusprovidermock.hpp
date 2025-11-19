/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_INSTANCESTATUSPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_INSTANCESTATUSPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>

namespace aos::instancestatusprovider {

/**
 * Listener mock.
 */
class ListenerMock : public ListenerItf {
public:
    MOCK_METHOD(void, OnInstancesStatusesChanged, (const Array<InstanceStatus>&), (override));
};

/**
 * Provider mock.
 */
class ProviderMock : public ProviderItf {
public:
    MOCK_METHOD(Error, GetInstancesStatuses, (Array<InstanceStatus>&), (override));
    MOCK_METHOD(Error, SubscribeListener, (ListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (ListenerItf&), (override));
};

} // namespace aos::instancestatusprovider

#endif
