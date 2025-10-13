/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_PERMHANDLERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_PERMHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/permhandler.hpp>

namespace aos::iamclient {

/**
 * Permission handler mock.
 */
class PermHandlerMock : public PermHandlerItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cSecretLen>>, RegisterInstance,
        (const InstanceIdent&, const Array<FunctionServicePermissions>&), (override));
    MOCK_METHOD(Error, UnregisterInstance, (const InstanceIdent&), (override));
};

} // namespace aos::iamclient

#endif
