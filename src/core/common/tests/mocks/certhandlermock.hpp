/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_CERTHANDLERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_CERTHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/certhandler.hpp>

namespace aos::iamclient {

/**
 * Certificate handler mock.
 */
class CertHandlerMock : public CertHandlerItf {
public:
    MOCK_METHOD(Error, CreateKey, (const String&, const String&, const String&, const String&, String&), (override));
    MOCK_METHOD(Error, ApplyCert, (const String&, const String&, const String&, CertInfo&), (override));
};

} // namespace aos::iamclient

#endif
