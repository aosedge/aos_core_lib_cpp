/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_LOGPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_LOGPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/logprovider/logprovider.hpp>

namespace aos::sm::logprovider {

/**
 * Log observer mock.
 */
class LogObserverMock : public LogObserverItf {
public:
    MOCK_METHOD(Error, OnLogReceived, (const PushLog& log), (override));
};

/**
 * Log provider mock.
 */
class LogProviderMock : public LogProviderItf {
public:
    MOCK_METHOD(Error, GetInstanceLog, (const RequestLog& request), (override));
    MOCK_METHOD(Error, GetInstanceCrashLog, (const RequestLog& request), (override));
    MOCK_METHOD(Error, GetSystemLog, (const RequestLog& request), (override));
    MOCK_METHOD(Error, Subscribe, (LogObserverItf & observer), (override));
    MOCK_METHOD(Error, Unsubscribe, (LogObserverItf & observer), (override));
};

} // namespace aos::sm::logprovider

#endif
