/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_CONNECTIONPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_CONNECTIONPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/cloudconnection/itf/cloudconnection.hpp>

namespace aos::cloudconnection {
/**
 * Connection subscriber mock.
 */
class ConnectionSubscriberMock : public ConnectionListenerItf {
public:
    MOCK_METHOD(void, OnConnect, (), (override));
    MOCK_METHOD(void, OnDisconnect, (), (override));
};

/**
 * Cloud connection mock.
 */
class CloudConnectionMock : public CloudConnectionItf {
public:
    MOCK_METHOD(Error, Subscribe, (ConnectionListenerItf & listener), (override));
    MOCK_METHOD(void, Unsubscribe, (ConnectionListenerItf & listener), (override));
};

} // namespace aos::cloudconnection

#endif
