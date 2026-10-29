/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_IDENTMODULEMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_IDENTMODULEMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/identhandler/itf/identmodule.hpp>

namespace aos::iam::identhandler {

/**
 * Ident module mock.
 */
class IdentModuleMock : public IdentModuleItf {
public:
    MOCK_METHOD(Error, Start, (), (override));
    MOCK_METHOD(Error, Stop, (), (override));
    MOCK_METHOD(Error, GetUnitModelInfo, (UnitModelInfo&), (override));
    MOCK_METHOD(Error, GetSubjects, (Array<StaticString<cIDLen>>&), (override));
    MOCK_METHOD(Error, SubscribeListener, (iamclient::SubjectsListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (iamclient::SubjectsListenerItf&), (override));
};

} // namespace aos::iam::identhandler

#endif
