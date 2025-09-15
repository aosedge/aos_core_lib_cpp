/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_PERMHANDLERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_PERMHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/permhandler/permhandler.hpp>

namespace aos::iam::permhandler {

/**
 * Permission handler mock.
 */
class PermHandlerMock : public PermHandlerItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cSecretLen>>, RegisterInstance,
        (const InstanceIdent&, const Array<FunctionServicePermissions>&), (override));
    MOCK_METHOD(Error, UnregisterInstance, (const InstanceIdent&), (override));
    MOCK_METHOD(
        Error, GetPermissions, (const String&, const String&, InstanceIdent&, Array<FunctionPermissions>&), (override));
};

} // namespace aos::iam::permhandler

#endif
