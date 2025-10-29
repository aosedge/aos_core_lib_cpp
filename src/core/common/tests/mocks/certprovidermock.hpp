
/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_IAMCLIENTMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_IAMCLIENTMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/certprovider.hpp>

namespace aos::iamclient {

/**
 * Certificate listener mock.
 */
class CertListenerMock : public CertListenerItf {
public:
    MOCK_METHOD(void, OnCertChanged, (const CertInfo&), (override));
};

/**
 * Mocks certificate provider.
 */
class CertProviderMock : public CertProviderItf {
public:
    MOCK_METHOD(
        Error, GetCert, (const String&, const Array<uint8_t>&, const Array<uint8_t>&, CertInfo&), (const override));
    MOCK_METHOD(Error, SubscribeListener, (const String&, CertListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (CertListenerItf&), (override));
};

} // namespace aos::iamclient

#endif
