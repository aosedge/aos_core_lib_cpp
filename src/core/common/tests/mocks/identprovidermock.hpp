/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_IDENTPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_IDENTPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/identprovider/itf/identprovider.hpp>

namespace aos::identprovider {

/**
 * Subjects observer mock.
 */
class SubjectsObserverMock : public SubjectsObserverItf {
public:
    MOCK_METHOD(Error, SubjectsChanged, (const Array<StaticString<cIDLen>>&), (override));
};

/**
 * IdentProvider interface mock
 */
class IdentProviderMock : public IdentProviderItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cIDLen>>, GetSystemID, (), (override));
    MOCK_METHOD(RetWithError<StaticString<cUnitModelLen>>, GetUnitModel, (), (override));
    MOCK_METHOD(Error, GetSubjects, (Array<StaticString<cIDLen>> & subjects), (override));
    MOCK_METHOD(Error, SubscribeSubjectsChanged, (SubjectsObserverItf&), (override));
    MOCK_METHOD(void, UnsubscribeSubjectsChanged, (SubjectsObserverItf&), (override));
};

} // namespace aos::identprovider

#endif
