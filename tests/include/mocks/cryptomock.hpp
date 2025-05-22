/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CRYPTO_MOCK_HPP_
#define AOS_CRYPTO_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/common/crypto/crypto.hpp"

namespace aos::crypto {

namespace x509 {

/**
 * Provides interface to mock manage certificate requests.
 */
class ProviderMock : public ProviderItf {
public:
    MOCK_METHOD(Error, CreateCertificate,
        (const x509::Certificate&, const x509::Certificate&, const PrivateKeyItf&, String&), (override));
    MOCK_METHOD(Error, CreateClientCert, (const String&, const String&, const String&, const Array<uint8_t>&, String&),
        (override));
    MOCK_METHOD(Error, PEMToX509Certs, (const String&, Array<x509::Certificate>&), (override));
    MOCK_METHOD(Error, X509CertToPEM, (const x509::Certificate&, String&), (override));
    MOCK_METHOD(RetWithError<SharedPtr<PrivateKeyItf>>, PEMToX509PrivKey, (const String&), (override));
    MOCK_METHOD(Error, DERToX509Cert, (const Array<uint8_t>&, x509::Certificate&), (override));
    MOCK_METHOD(Error, CreateCSR, (const x509::CSR&, const PrivateKeyItf&, String&), (override));
    MOCK_METHOD(Error, ASN1EncodeDN, (const String&, Array<uint8_t>&), (override));
    MOCK_METHOD(Error, ASN1DecodeDN, (const Array<uint8_t>&, String&), (override));
    MOCK_METHOD(Error, ASN1EncodeObjectIds, (const Array<asn1::ObjectIdentifier>&, Array<uint8_t>&), (override));
    MOCK_METHOD(Error, ASN1EncodeBigInt, (const Array<uint8_t>&, Array<uint8_t>&), (override));
    MOCK_METHOD(Error, ASN1EncodeDERSequence, (const Array<Array<uint8_t>>&, Array<uint8_t>&), (override));
    MOCK_METHOD(Error, ASN1DecodeOctetString, (const Array<uint8_t>&, Array<uint8_t>&), (override));
    MOCK_METHOD(Error, ASN1DecodeOID, (const Array<uint8_t>&, Array<uint8_t>&), (override));
};

} // namespace x509

} // namespace aos::crypto

#endif
