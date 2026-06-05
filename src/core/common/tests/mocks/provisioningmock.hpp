/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_PROVISIONINGMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_PROVISIONINGMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/provisioning.hpp>

namespace aos::iamclient {

/**
 * Provisioning mock.
 */
class ProvisioningMock : public ProvisioningItf {
public:
    MOCK_METHOD(Error, GetCertTypes, (const String&, Array<StaticString<cCertTypeLen>>&), (const, override));
    MOCK_METHOD(Error, StartProvisioning, (const String&, const String&), (override));
    MOCK_METHOD(Error, FinishProvisioning, (const String&, const String&), (override));
    MOCK_METHOD(Error, Deprovision, (const String&, const String&), (override));
};

} // namespace aos::iamclient

#endif
