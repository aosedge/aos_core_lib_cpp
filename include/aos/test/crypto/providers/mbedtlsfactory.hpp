/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MBEDTLS_FACTORY_HPP_
#define MBEDTLS_FACTORY_HPP_

#include "cryptofactoryitf.hpp"

#include <aos/common/crypto/mbedtls/cryptoprovider.hpp>

namespace aos::crypto {

class MBedTLSCryptoFactory : public CryptoFactoryItf {
public:
    MBedTLSCryptoFactory();

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
    MbedTLSCryptoProvider mProvider;
};

} // namespace aos::crypto

#endif
