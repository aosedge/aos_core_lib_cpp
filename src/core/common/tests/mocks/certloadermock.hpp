/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CERTLOADER_MOCK_HPP_
#define CERTLOADER_MOCK_HPP_

#include <gmock/gmock.h>

#include <aos/common/crypto/utils.hpp>

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
