/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CONNECTIONSUBSC_MOCK_HPP_
#define AOS_CONNECTIONSUBSC_MOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/connectionprovider/connectionprovider.hpp>

namespace aos {

/**
 * Connection subscriber mock.
 */
class ConnectionSubscriberMock : public ConnectionSubscriberItf {
public:
    MOCK_METHOD(void, OnConnect, (), (override));
    MOCK_METHOD(void, OnDisconnect, (), (override));
};

/**
 * Connection publisher mock.
 */
class ConnectionPublisherMock : public ConnectionPublisherItf {
public:
    MOCK_METHOD(Error, Subscribe, (ConnectionSubscriberItf & subscriber), (override));
    MOCK_METHOD(void, Unsubscribe, (ConnectionSubscriberItf & subscriber), (override));
};

} // namespace aos

#endif
