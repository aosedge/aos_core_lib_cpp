/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRYPTO_FACTORY_ITF_HPP_
#define CRYPTO_FACTORY_ITF_HPP_

#include <memory>
#include <string>

#include <aos/common/config.hpp>
#include <aos/common/crypto/crypto.hpp>

namespace aos::crypto {

/**
 * Crypto factory interface.
 */
class CryptoFactoryItf {
public:
    /**
     * Destructor.
     */
    virtual ~CryptoFactoryItf() = default;

    /**
     * Initializes crypto factory.
     *
     * @return Error.
     */
    virtual Error Init() = 0;

    /**
     * Returns crypto factory name.
     *
     * @return std::string.
     */
    virtual std::string GetName() = 0;

    /**
     * Returns crypto provider.
     *
     * @return x509::ProviderItf&.
     */
    virtual x509::ProviderItf& GetCryptoProvider() = 0;

    /**
     * Returns hash provider.
     *
     * @return HasherItf&.
     */
    virtual HasherItf& GetHashProvider() = 0;

    /**
     * Returns random provider.
     *
     * @return RandomItf&.
     */
    virtual RandomItf& GetRandomProvider() = 0;

    /**
     * Generates RSA private key.
     *
     * @return RetWithError<std::shared_ptr<PrivateKeyItf>>.
     */
    virtual RetWithError<std::shared_ptr<PrivateKeyItf>> GenerateRSAPrivKey() = 0;

    /**
     * Generates ECDSA private key.
     *
     * @return RetWithError<std::shared_ptr<PrivateKeyItf>>.
     */
    virtual RetWithError<std::shared_ptr<PrivateKeyItf>> GenerateECDSAPrivKey() = 0;

    /**
     * Converts PEM certificate to DER format.
     *
     * @param pem PEM certificate.
     * @return RetWithError<std::vector<uint8_t>>.
     */
    virtual RetWithError<std::vector<uint8_t>> PemCertToDer(const char* pem) = 0;

    /**
     * Verifies PEM certificate.
     *
     * @param pemCert PEM certificate.
     * @return bool.
     */
    virtual bool VerifyCertificate(const std::string& pemCert) = 0;

    /**
     * Verifies PEM CSR.
     *
     * @param pemCSR PEM CSR.
     * @return bool.
     */
    virtual bool VerifyCSR(const std::string& pemCSR) = 0;

    /**
     * Verifies RSA signature.
     *
     * @param pubKey public key.
     * @param signature signature.
     * @param digest digest.
     * @return bool.
     */
    virtual bool VerifySignature(
        const RSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
        = 0;

    /**
     * Verifies ECDSA signature.
     *
     * @param pubKey public key.
     * @param signature signature.
     * @param digest digest.
     * @return bool.
     */
    virtual bool VerifySignature(
        const ECDSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
        = 0;

    /**
     * Encrypts message using RSA public key.
     *
     * @param pubKey RSA public key.
     * @param msg message to encrypt.
     * @param cipher encrypted message.
     * @return Error.
     */
    virtual Error Encrypt(const crypto::RSAPublicKey& pubKey, const Array<uint8_t>& msg, Array<uint8_t>& cipher) = 0;
};

} // namespace aos::crypto

#endif
