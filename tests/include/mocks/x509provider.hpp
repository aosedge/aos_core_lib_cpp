/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_MOCK_X509PROVIDER_HPP_
#define AOS_MOCK_X509PROVIDER_HPP_

#include "aos/common/crypto.hpp"
#include <gmock/gmock.h>

namespace aos {
namespace crypto {
namespace x509 {

class MockProvider : public ProviderItf {
public:
    MOCK_METHOD(Error, CreateCertificate,
        (const Certificate& templ, const Certificate& parent, const PrivateKeyItf& privKey, String& pemCert),
        (override));
    MOCK_METHOD(Error, PEMToX509Certs, (const String& pemBlob, Array<Certificate>& resultCerts), (override));
    MOCK_METHOD(RetWithError<SharedPtr<PrivateKeyItf>>, PEMToX509PrivKey, (const String& pemBlob), (override));
    MOCK_METHOD(Error, DERToX509Cert, (const Array<uint8_t>& derBlob, Certificate& resultCerts), (override));
    MOCK_METHOD(Error, CreateCSR, (const CSR& templ, const PrivateKeyItf& privKey, String& pemCSR), (override));
    MOCK_METHOD(Error, ASN1EncodeDN, (const String& commonName, Array<uint8_t>& result), (override));
    MOCK_METHOD(Error, ASN1DecodeDN, (const Array<uint8_t>& dn, String& result), (override));
    MOCK_METHOD(
        Error, ASN1EncodeObjectIds, (const Array<asn1::ObjectIdentifier>& src, Array<uint8_t>& asn1Value), (override));
    MOCK_METHOD(Error, ASN1EncodeBigInt, (const Array<uint8_t>& number, Array<uint8_t>& asn1Value), (override));
    MOCK_METHOD(
        Error, ASN1EncodeDERSequence, (const Array<Array<uint8_t>>& items, Array<uint8_t>& asn1Value), (override));
    MOCK_METHOD(Error, ASN1DecodeOctetString, (const Array<uint8_t>& src, Array<uint8_t>& dst), (override));
};

} // namespace x509
} // namespace crypto
} // namespace aos

#endif
