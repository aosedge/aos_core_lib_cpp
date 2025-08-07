/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_IDENTHANDLERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_IDENTHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/identhandler/identhandler.hpp>

namespace aos::identhandler {

/**
 * Subjects observer mock.
 */
class SubjectsObserverMock : public SubjectsObserverItf {
public:
    MOCK_METHOD(Error, SubjectsChanged, (const Array<StaticString<cSubjectIDLen>>&), (override));
};

/**
 * Subjects publisher mock.
 */
class SubjectsPublisherMock : public SubjectsPublisherItf {
public:
    MOCK_METHOD(Error, SubscribeSubjectsChanged, (SubjectsObserverItf&), (override));
    MOCK_METHOD(void, UnsubscribeSubjectsChanged, (SubjectsObserverItf&), (override));
};

/**
 * IdentHandler interface mock
 */
class IdentHandlerMock : public IdentHandlerItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cSystemIDLen>>, GetSystemID, (), (override));
    MOCK_METHOD(RetWithError<StaticString<cUnitModelLen>>, GetUnitModel, (), (override));
    MOCK_METHOD(Error, GetSubjects, (Array<StaticString<cSubjectIDLen>> & subjects), (override));
};

} // namespace aos::identhandler

#endif
