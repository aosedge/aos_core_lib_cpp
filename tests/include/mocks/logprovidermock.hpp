/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LOGPROVIDER_MOCK_HPP_
#define AOS_LOGPROVIDER_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/sm/logprovider.hpp"

namespace aos::sm::logprovider {

/**
 * Log observer mock.
 */
class LogObserverMock : public LogObserverItf {
public:
    MOCK_METHOD(Error, OnLogReceived, (const cloudprotocol::PushLog& log), (override));
};

/**
 * Log provider mock.
 */
class LogProviderMock : public LogProviderItf {
public:
    MOCK_METHOD(Error, GetInstanceLog, (const cloudprotocol::RequestLog& request), (override));
    MOCK_METHOD(Error, GetInstanceCrashLog, (const cloudprotocol::RequestLog& request), (override));
    MOCK_METHOD(Error, GetSystemLog, (const cloudprotocol::RequestLog& request), (override));
    MOCK_METHOD(Error, Subscribe, (LogObserverItf & observer), (override));
    MOCK_METHOD(Error, Unsubscribe, (LogObserverItf & observer), (override));
};

} // namespace aos::sm::logprovider

#endif
