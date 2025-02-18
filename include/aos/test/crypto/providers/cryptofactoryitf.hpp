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

class CryptoFactoryItf {
public:
    virtual ~CryptoFactoryItf() = default;

    virtual Error                                        Init()                                        = 0;
    virtual std::string                                  GetName()                                     = 0;
    virtual x509::ProviderItf&                           GetCryptoProvider()                           = 0;
    virtual HasherItf&                                   GetHashProvider()                             = 0;
    virtual RetWithError<std::shared_ptr<PrivateKeyItf>> GenerateRSAPrivKey()                          = 0;
    virtual RetWithError<std::shared_ptr<PrivateKeyItf>> GenerateECDSAPrivKey()                        = 0;
    virtual RetWithError<std::vector<uint8_t>>           PemCertToDer(const char* pem)                 = 0;
    virtual bool                                         VerifyCertificate(const std::string& pemCert) = 0;
    virtual bool                                         VerifyCSR(const std::string& pemCSR)          = 0;
    virtual bool                                         VerifySignature(
                                                const RSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
        = 0;
    virtual bool VerifySignature(
        const ECDSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
        = 0;
    virtual Error Encrypt(const crypto::RSAPublicKey& pubKey, const Array<uint8_t>& msg, Array<uint8_t>& cipher) = 0;
};

} // namespace aos::crypto

#endif
