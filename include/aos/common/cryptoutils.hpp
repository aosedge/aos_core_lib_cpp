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
#include "aos/common/pkcs11/privatekey.hpp"
#include "aos/common/tools/uuid.hpp"

namespace aos {
namespace cryptoutils {

/**
 * Loads certificates and keys interface.
 */
class CertLoaderItf {
public:
    /**
     * Loads certificate chain by URL.
     *
     * @param url input url.
     * @return RetWithError<SharedPtr<crypto::x509::CertificateChain>>.
     */
    virtual RetWithError<SharedPtr<crypto::x509::CertificateChain>> LoadCertsChainByURL(const String& url) = 0;

    /**
     * Loads private key by URL.
     *
     * @param url input url.
     * @return RetWithError<SharedPtr<crypto::PrivateKeyItf>>.
     */
    virtual RetWithError<SharedPtr<crypto::PrivateKeyItf>> LoadPrivKeyByURL(const String& url) = 0;

    /**
     * Destroys cert loader instance.
     */
    virtual ~CertLoaderItf() = default;
};

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
    Error Init(crypto::x509::ProviderItf& cryptoProvider, pkcs11::PKCS11Manager& pkcs11Manager);

    /**
     * Loads certificate chain by URL.
     *
     * @param url input url.
     * @return RetWithError<SharedPtr<crypto::x509::CertificateChain>>.
     */
    RetWithError<SharedPtr<crypto::x509::CertificateChain>> LoadCertsChainByURL(const String& url) override;

    /**
     * Loads private key by URL.
     *
     * @param url input url.
     * @return RetWithError<SharedPtr<crypto::PrivateKeyItf>>.
     */
    RetWithError<SharedPtr<crypto::PrivateKeyItf>> LoadPrivKeyByURL(const String& url) override;

private:
    using PEMCertChainBlob = StaticString<crypto::cCertPEMLen * crypto::cCertChainSize>;

    static constexpr auto cCertAllocatorSize
        = AOS_CONFIG_CRYPTOUTILS_CERTIFICATE_CHAINS_COUNT * crypto::cCertChainSize * sizeof(crypto::x509::Certificate)
        + sizeof(PEMCertChainBlob);
    static constexpr auto cKeyAllocatorSize
        = AOS_CONFIG_CRYPTOUTILS_KEYS_COUNT * pkcs11::cPrivateKeyMaxSize + sizeof(crypto::cPrivKeyPEMLen);

    static constexpr auto cDefaultPKCS11Library = AOS_CONFIG_CRYPTOUTILS_DEFAULT_PKCS11_LIB;

    RetWithError<SharedPtr<pkcs11::SessionContext>> OpenSession(
        const String& libraryPath, const String& token, const String& userPIN);
    RetWithError<pkcs11::SlotID> FindToken(const pkcs11::LibraryContext& library, const String& token);

    RetWithError<SharedPtr<crypto::x509::CertificateChain>> LoadCertsFromFile(const String& fileName);
    RetWithError<SharedPtr<crypto::PrivateKeyItf>>          LoadPrivKeyFromFile(const String& fileName);

    crypto::x509::ProviderItf* mCryptoProvider = nullptr;
    pkcs11::PKCS11Manager*     mPKCS11         = nullptr;

    StaticAllocator<cCertAllocatorSize + cKeyAllocatorSize + pkcs11::Utils::cLocalObjectsMaxSize> mAllocator;
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
 * Encodes PKCS11 ID to percent-encoded string.
 *
 * @param id PKCS11 ID.
 * @param idStr percent-encoded string.
 * @return Error.
 */
Error EncodePKCS11ID(const Array<uint8_t>& id, String& idStr);

/**
 * Decodes PKCS11 ID from percent-encoded string.
 *
 * @param idStr percent-encoded string.
 * @param id PKCS11 ID.
 * @return Error.
 */
Error DecodeToPKCS11ID(const String& idStr, Array<uint8_t>& id);

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
