/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CRYPTOUTILS_HPP_
#define AOS_CRYPTOUTILS_HPP_

#include "aos/common/crypto.hpp"
#include "aos/common/pkcs11/pkcs11.hpp"
#include "aos/common/uuid.hpp"

namespace aos {
namespace cryptoutils {

/**
 * Loads certificates and keys by URL
 */
class CertLoader {
public:
    /**
     * Initializes object instance.
     *
     * @param cryptoProvider crypto provider interface.
     * @param pkcs11Manager PKCS11 library manager.
     * @return Error.
     */
    Error Init(crypto::x509::ProviderItf& cryptoProvider, pkcs11::PKCS11Manager& pkcs11Manager);

    /**
     * Loads certificate chain by URL.
     *
     * @param url input url.
     * @return RetWithError<SharedPtr<crypto::x509::CertificateChain>>.
     */
    RetWithError<SharedPtr<crypto::x509::CertificateChain>> LoadCertsChainByURL(const String& url);

    /**
     * Loads private key by URL.
     *
     * @param url input url.
     * @return RetWithError<SharedPtr<crypto::PrivateKeyItf>>.
     */
    RetWithError<SharedPtr<crypto::PrivateKeyItf>> LoadPrivKeyByURL(const String& url);

private:
    using PEMCertChainBlob = StaticArray<uint8_t, crypto::cCertPEMSize * crypto::cCertChainSize>;

    static constexpr auto cCertLoaderCapacity   = AOS_CONFIG_CRYPTOUTILS_CERTLOADER_CAPACITY;
    static constexpr auto cDefaultPKCS11Library = AOS_CONFIG_CRYPTOUTILS_DEFAULT_PKCS11_LIB;

    RetWithError<UniquePtr<pkcs11::SessionContext>> OpenSession(
        const String& libraryPath, const String& token, const String& userPIN);
    RetWithError<pkcs11::SlotID> FindToken(pkcs11::LibraryContext& library, const String& token);

    RetWithError<SharedPtr<crypto::x509::CertificateChain>> LoadCertsFromFile(const String& fileName);
    RetWithError<SharedPtr<crypto::PrivateKeyItf>>          LoadPrivKeyFromFile(const String& fileName);

    crypto::x509::ProviderItf* mCryptoProvider = nullptr;
    pkcs11::PKCS11Manager*     mPKCS11         = nullptr;

    StaticAllocator<cCertLoaderCapacity> mAllocator;
};

/**
 * Parses scheme part of URL.
 *
 * @param url input url.
 * @param[out] scheme url scheme.
 * @return Error.
 */
Error ParseURLScheme(const String& url, String& scheme);

/**
 * Parses URL with file scheme.
 *
 * @param url input url.
 * @param[out] path file path.
 * @return Error.
 */
Error ParseFileURL(const String& url, String& path);

/**
 * Parses url with PKCS11 scheme.
 *
 * @param url input url.
 * @param[out] library PKCS11 library.
 * @param[out] token token label.
 * @param[out] label certificate label.
 * @param[out] id certificate id.
 * @param[out] userPin user PIN.
 * @return Error.
 */
Error ParsePKCS11URL(
    const String& url, String& library, String& token, String& label, Array<uint8_t>& id, String& userPin);

} // namespace cryptoutils
} // namespace aos

#endif
