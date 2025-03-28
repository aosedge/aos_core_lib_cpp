/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef OPENSSL_FACTORY_HPP_
#define OPENSSL_FACTORY_HPP_

#include "cryptofactoryitf.hpp"

#include <aos/common/crypto/openssl/cryptoprovider.hpp>

namespace aos::crypto {

class OpenSSLCryptoFactory : public CryptoFactoryItf {
public:
    OpenSSLCryptoFactory();

    Error                                        Init() override;
    std::string                                  GetName() override;
    x509::ProviderItf&                           GetCryptoProvider() override;
    HasherItf&                                   GetHashProvider() override;
    RetWithError<std::shared_ptr<PrivateKeyItf>> GenerateRSAPrivKey() override;
    RetWithError<std::shared_ptr<PrivateKeyItf>> GenerateECDSAPrivKey() override;
    RetWithError<std::vector<uint8_t>>           PemCertToDer(const char* pem) override;
    bool                                         VerifyCertificate(const std::string& pemCert) override;
    bool                                         VerifyCSR(const std::string& pemCSR) override;
    bool                                         VerifySignature(
                                                const RSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest) override;
    bool VerifySignature(
        const ECDSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest) override;
    Error Encrypt(const crypto::RSAPublicKey& pubKey, const Array<uint8_t>& msg, Array<uint8_t>& cipher) override;

private:
    OpenSSLCryptoProvider mProvider;
};

} // namespace aos::crypto

#endif
