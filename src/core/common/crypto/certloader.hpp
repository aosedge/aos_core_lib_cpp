/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_CRYPTOUTILS_HPP_
#define AOS_CORE_COMMON_CRYPTO_CRYPTOUTILS_HPP_

#include <core/common/pkcs11/pkcs11.hpp>
#include <core/common/pkcs11/privatekey.hpp>

#include "itf/certoloader.hpp"
#include "itf/crypto.hpp"

namespace aos::crypto {

/**
 * Loads certificates and keys by URL.
 */
class CertLoader : public CertLoaderItf {
public:
    /**
     * Initializes object instance.
     *
     * @param cryptoProvider crypto provider interface.
     * @param pkcs11Manager PKCS11 library manager.
     * @return Error.
     */
    Error Init(x509::ProviderItf& cryptoProvider, pkcs11::PKCS11Manager& pkcs11Manager);

    /**
     * Loads certificate chain by URL.
     *
     * @param url input url.
     * @return RetWithError<SharedPtr<x509::CertificateChain>>.
     */
    RetWithError<SharedPtr<x509::CertificateChain>> LoadCertsChainByURL(const String& url) override;

    /**
     * Loads private key by URL.
     *
     * @param url input url.
     * @return RetWithError<SharedPtr<PrivateKeyItf>>.
     */
    RetWithError<SharedPtr<PrivateKeyItf>> LoadPrivKeyByURL(const String& url) override;

private:
    using PEMCertChainBlob = StaticString<cCertPEMLen * cCertChainSize>;

    static constexpr auto cCertAllocatorSize
        = cCertChainsCount * cCertChainSize * sizeof(x509::Certificate) + sizeof(PEMCertChainBlob);
    static constexpr auto cKeyAllocatorSize
        = AOS_CONFIG_CRYPTO_PRIV_KEYS_COUNT * pkcs11::cPrivateKeyMaxSize + sizeof(cPrivKeyPEMLen);
    static constexpr auto cNumAllocation = AOS_CONFIG_CRYPTO_NUM_ALLOCATIONS;

    static constexpr auto cDefaultPKCS11Library = AOS_CONFIG_CRYPTO_DEFAULT_PKCS11_LIB;

    RetWithError<SharedPtr<pkcs11::SessionContext>> OpenSession(
        const String& libraryPath, const String& token, const String& userPIN);
    RetWithError<pkcs11::SlotID> FindToken(const pkcs11::LibraryContext& library, const String& token);

    RetWithError<SharedPtr<x509::CertificateChain>> LoadCertsFromFile(const String& fileName);
    RetWithError<SharedPtr<PrivateKeyItf>>          LoadPrivKeyFromFile(const String& fileName);

    x509::ProviderItf*     mCryptoProvider = nullptr;
    pkcs11::PKCS11Manager* mPKCS11         = nullptr;

    StaticAllocator<cCertAllocatorSize + cKeyAllocatorSize + pkcs11::Utils::cLocalObjectsMaxSize, cNumAllocation>
        mAllocator;
};

} // namespace aos::crypto

#endif
