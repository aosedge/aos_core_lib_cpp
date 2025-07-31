/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_CRYPTO_PROVIDERS_MBEDTLSFACTORY_HPP_
#define AOS_CORE_COMMON_TESTS_CRYPTO_PROVIDERS_MBEDTLSFACTORY_HPP_

#include <core/common/crypto/mbedtls/cryptoprovider.hpp>

#include "cryptofactoryitf.hpp"

namespace aos::crypto {

/**
 * Mbed TLS crypto factory.
 */
class MBedTLSCryptoFactory : public CryptoFactoryItf {
public:
    /**
     * Constructor.
     */
    MBedTLSCryptoFactory();

    /**
     * Initializes crypto factory.
     *
     * @return Error.
     */
    Error Init() override;

    /**
     * Returns crypto factory name.
     *
     * @return std::string.
     */
    std::string GetName() override;

    /**
     * Returns crypto provider.
     *
     * @return CryptoProviderItf&.
     */
    CryptoProviderItf& GetCryptoProvider() override;

    /**
     * Returns hash provider.
     *
     * @return HasherItf&.
     */
    HasherItf& GetHashProvider() override;

    /**
     * Returns random provider.
     *
     * @return RandomItf&.
     */
    RandomItf& GetRandomProvider() override;

    /**
     * Generates RSA private key.
     *
     * @return RetWithError<std::shared_ptr<PrivateKeyItf>>.
     */
    RetWithError<std::shared_ptr<PrivateKeyItf>> GenerateRSAPrivKey() override;

    /**
     * Generates ECDSA private key.
     *
     * @return RetWithError<std::shared_ptr<PrivateKeyItf>>.
     */
    RetWithError<std::shared_ptr<PrivateKeyItf>> GenerateECDSAPrivKey() override;

    /**
     * Converts PEM certificate to DER format.
     *
     * @param pem PEM certificate.
     * @return RetWithError<std::vector<uint8_t>>.
     */
    RetWithError<std::vector<uint8_t>> PemCertToDer(const char* pem) override;

    /**
     * Verifies PEM certificate.
     *
     * @param pemCert PEM certificate.
     * @return bool.
     */
    bool VerifyCertificate(const std::string& pemCert) override;

    /**
     * Verifies PEM CSR.
     *
     * @param pemCSR PEM CSR.
     * @return bool.
     */
    bool VerifyCSR(const std::string& pemCSR) override;

    /**
     * Verifies RSA signature.
     *
     * @param pubKey public key.
     * @param signature signature.
     * @param digest digest.
     * @return bool.
     */
    bool VerifySignature(
        const RSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest) override;

    /**
     * Verifies ECDSA signature.
     *
     * @param pubKey public key.
     * @param signature signature.
     * @param digest digest.
     * @return bool.
     */
    bool VerifySignature(
        const ECDSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest) override;

    /**
     * Encrypts message using RSA public key.
     *
     * @param pubKey RSA public key.
     * @param msg message to encrypt.
     * @param cipher encrypted message.
     * @return Error.
     */
    Error Encrypt(const crypto::RSAPublicKey& pubKey, const Array<uint8_t>& msg, Array<uint8_t>& cipher) override;

private:
    MbedTLSCryptoProvider mProvider;
};

} // namespace aos::crypto

#endif
