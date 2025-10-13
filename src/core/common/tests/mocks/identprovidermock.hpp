/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_IDENTHANDLERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_IDENTHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/identprovider.hpp>

namespace aos::iamclient {

/**
 * Subjects listener mock.
 */
class SubjectsListenerMock : public SubjectsListenerItf {
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
    MOCK_METHOD(Error, SubscribeListener, (SubjectsListenerItf & subjectsListener), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (SubjectsListenerItf & subjectsListener), (override));
};

} // namespace aos::iamclient

#endif
