/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_CERTLOADERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_CERTLOADERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/crypto/cryptoutils.hpp>

namespace aos::crypto {

/**
 * Mocks load certificates and keys interface.
 */

class CertLoaderMock : public CertLoaderItf {
public:
    MOCK_METHOD(
        RetWithError<SharedPtr<crypto::x509::CertificateChain>>, LoadCertsChainByURL, (const String&), (override));
    MOCK_METHOD(RetWithError<SharedPtr<crypto::PrivateKeyItf>>, LoadPrivKeyByURL, (const String&), (override));
};

} // namespace aos::crypto

#endif
