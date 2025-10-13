/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_PERMPROVIDERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_PERMPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/permprovider.hpp>

namespace aos::iamclient {

/**
 * Permission provider mock.
 */
class PermProviderMock : public PermProviderItf {
public:
    MOCK_METHOD(
        Error, GetPermissions, (const String&, const String&, InstanceIdent&, Array<FunctionPermissions>&), (override));
};

} // namespace aos::iamclient

#endif
