/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IAM_MODULES_HSM_HPP_
#define AOS_IAM_MODULES_HSM_HPP_

#include "aos/common/crypto.hpp"
#include "aos/common/tools/array.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/iam/modules/config.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/**
 * Certificate type name length.
 */
constexpr auto cCertTypeLen = AOS_CONFIG_CERTHANDLER_CERT_TYPE_NAME_LEN;

/**
 * Max number of IAM certificates per module.
 */
constexpr auto cCertsPerModule = AOS_CONFIG_CERTHANDLER_CERTS_PER_MODULE;

/**
 * Public/private keys generating algorithm
 */
class KeyGenAlgorithmType {
public:
    enum class Enum { eRSA, eECC };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sContentTypeStrings[] = {"RSA", "ECC"};
        return Array<const char* const>(sContentTypeStrings, ArraySize(sContentTypeStrings));
    };
};

using KeyGenAlgorithmEnum = KeyGenAlgorithmType::Enum;
using KeyGenAlgorithm     = EnumStringer<KeyGenAlgorithmType>;

/**
 * General certificate information.
 */
struct CertInfo {
    /**
     * Certificate issuer.
     */
    StaticArray<uint8_t, crypto::cCertIssuerSize> mIssuer;
    /**
     * Certificate serial number.
     */
    StaticString<crypto::cSerialNumSize> mSerial;
    /**
     * Certificate url.
     */
    StaticString<cURLLen> mCertURL;
    /**
     * Certificate's private key url.
     */
    StaticString<cURLLen> mKeyURL;
    /**
     * Certificate expiration time.
     */
    time::Time mNotAfter;
    /**
     * Checks whether certificate info is equal the the current one.
     *
     * @param certInfo info to compare.
     * @return bool.
     */
    bool operator==(const CertInfo& certInfo) const
    {
        return certInfo.mCertURL == mCertURL && certInfo.mIssuer == mIssuer && certInfo.mKeyURL == mKeyURL
            && certInfo.mNotAfter == mNotAfter && certInfo.mSerial == mSerial;
    }
    /**
     * Checks whether certificate info is equal the the current one.
     *
     * @param certInfo info to compare.
     * @return bool.
     */
    bool operator!=(const CertInfo& certInfo) const { return !operator==(certInfo); }

    /**
     * Prints object to log.
     *
     * @param log log to output.
     * @param certInfo object instance.
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const CertInfo& certInfo)
    {
        return log << "{certURL = " << certInfo.mCertURL << ", keyURL = " << certInfo.mKeyURL
                   << ", notAfter = " << certInfo.mNotAfter << "}";
    }
};

/**
 * Platform dependent secure certificate storage.
 */
class HSMItf {
public:
    /**
     * Owns the module.
     *
     * @param password certficate password.
     * @return Error.
     */
    virtual Error SetOwner(const String& password) = 0;

    /**
     * Removes all module certificates.
     *
     * @return Error.
     */
    virtual Error Clear() = 0;

    /**
     * Generates private key.
     *
     * @param password owner password.
     * @param algorithm key generation algorithm.
     * @return RetWithError<SharedPtr<crypto::PrivateKeyItf>>.
     */
    virtual RetWithError<SharedPtr<crypto::PrivateKeyItf>> CreateKey(const String& password, KeyGenAlgorithm algorithm)
        = 0;

    /**
     * Applies certificate chain to a module.
     *
     * @param certChain certificate chain.
     * @param[out] certInfo info about a top certificate in a chain.
     * @param[out] password owner password.
     * @return Error.
     */
    virtual Error ApplyCert(const Array<crypto::x509::Certificate>& certChain, CertInfo& certInfo, String& password)
        = 0;

    /**
     * Removes certificate chain using top level certificate URL and password.
     *
     * @param certURL top level certificate URL.
     * @param password owner password.
     * @return Error.
     */
    virtual Error RemoveCert(const String& certURL, const String& password) = 0;

    /**
     * Removes private key from a module.
     *
     * @param keyURL private key URL.
     * @param password owner password.
     * @return Error.
     */
    virtual Error RemoveKey(const String& keyURL, const String& password) = 0;

    /**
     * Returns valid/invalid certificates.
     *
     * @param[out] invalidCerts invalid certificate URLs.
     * @param[out] invalidKeys invalid key URLs.
     * @param[out] validCerts information about valid certificates.
     * @return Error.
     **/
    virtual Error ValidateCertificates(Array<StaticString<cURLLen>>& invalidCerts,
        Array<StaticString<cURLLen>>& invalidKeys, Array<CertInfo>& validCerts)
        = 0;

    /**
     * Destroys object instace.
     */
    virtual ~HSMItf() = default;
};

} // namespace certhandler
} // namespace iam
} // namespace aos

#endif
