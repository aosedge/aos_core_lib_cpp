/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_INSTANCESTATUSPROVIERMOCK_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_INSTANCESTATUSPROVIERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/launcher/itf/instancestatusprovider.hpp>

namespace aos::cm::launcher {

/**
 * Instance status listener mock.
 */
class InstanceStatusListenerMock : public InstanceStatusListenerItf {
public:
    MOCK_METHOD(void, OnInstancesStatusesChanged, (const Array<InstanceStatus>&), (override));
};

/**
 * Instance status provider mock.
 */
class InstanceStatusProviderMock : public InstanceStatusProviderItf {
public:
    MOCK_METHOD(Error, GetInstancesStatuses, (Array<InstanceStatus>&), (override));
    MOCK_METHOD(Error, SubscribeListener, (InstanceStatusListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (InstanceStatusListenerItf&), (override));
};

} // namespace aos::cm::launcher

#endif
