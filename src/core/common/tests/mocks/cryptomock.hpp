/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_CRYPTOMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_CRYPTOMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/crypto/itf/crypto.hpp>
#include <core/common/crypto/itf/privkey.hpp>

namespace aos::crypto {

/**
 * Provides interface to mock crypto helper.
 */
class CryptoHelperMock : public CryptoHelperItf {
public:
    MOCK_METHOD(Error, Decrypt,
        (const String& encryptedPath, const String& decryptedPath, const crypto::DecryptInfo& decryptionInfo),
        (override));
    MOCK_METHOD(Error, ValidateSigns,
        (const String& decryptedPath, const crypto::SignInfo& signs, const Array<crypto::CertificateChainInfo>& chains,
            const Array<crypto::CertificateInfo>& certs),
        (override));
    MOCK_METHOD(Error, DecryptMetadata, (const Array<uint8_t>& input, Array<uint8_t>& output), (override));
};

/**
 * Provides interface to mock a private (or symmetric, e.g. pkcs11::AESPrivateKey) key that never exposes
 * its own value.
 */
class PrivateKeyMock : public PrivateKeyItf {
public:
    MOCK_METHOD(const PublicKeyItf&, GetPublic, (), (const, override));
    MOCK_METHOD(Error, Sign, (const Array<uint8_t>& digest, const SignOptions& options, Array<uint8_t>& signature),
        (const, override));
    MOCK_METHOD(Error, Decrypt,
        (const Array<uint8_t>& cipher, const DecryptionOptions& options, Array<uint8_t>& result), (const, override));
    MOCK_METHOD(Error, StreamDecrypt,
        (ChunkProviderItf & chunkProvider, const DecryptionOptions& options, ChunkReceiverItf& chunkReceiver),
        (const, override));
};

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
    MOCK_METHOD(Error, Verify,
        ((const Variant<ECDSAPublicKey, RSAPublicKey>&), const Hash&, const x509::Padding&, const Array<uint8_t>&,
            const Array<uint8_t>&),
        (override));
    MOCK_METHOD(Error, Verify,
        (const Array<Certificate>&, const Array<Certificate>&, const VerifyOptions&, const Certificate&), (override));
};

} // namespace x509

} // namespace aos::crypto

#endif
