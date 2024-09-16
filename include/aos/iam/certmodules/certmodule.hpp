/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CERTMODULE_HPP_
#define AOS_CERTMODULE_HPP_

#include "aos/common/tools/allocator.hpp"
#include "aos/common/tools/enum.hpp"
#include "aos/common/tools/string.hpp"
#include "aos/common/tools/utils.hpp"

#include "aos/common/tools/time.hpp"

#include "aos/iam/certmodules/hsm.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/**
 * Maximum number of module key usages.
 */
constexpr auto cModuleKeyUsagesMaxCount = AOS_CONFIG_CERTHANDLER_KEY_USAGE_MAX_COUNT;

/**
 * Password max length.
 */
constexpr auto cPasswordLen = AOS_CONFIG_CERTHANDLER_PASSWORD_LEN;

/**
 * Extended key usage type.
 */
class ExtendedKeyUsageType {
public:
    enum class Enum { eClientAuth, eServerAuth };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sContentTypeStrings[] = {"clientAuth", "serverAuth"};
        return Array<const char* const>(sContentTypeStrings, ArraySize(sContentTypeStrings));
    };
};

using ExtendedKeyUsageEnum = ExtendedKeyUsageType::Enum;
using ExtendedKeyUsage     = EnumStringer<ExtendedKeyUsageType>;

/**
 * Module configuration.
 */
struct ModuleConfig {
    /**
     * Key type.
     */
    crypto::KeyType mKeyType;
    /**
     * Maximum number of certificates for module.
     */
    size_t mMaxCertificates;
    /**
     * Extra extensions needed for CSR. Current supported values: [clientAuth, serverAuth]
     */
    StaticArray<ExtendedKeyUsage, cModuleKeyUsagesMaxCount> mExtendedKeyUsage;
    /**
     * Alternative DNS names.
     */
    StaticArray<StaticString<crypto::cDNSNameLen>, crypto::cAltDNSNamesCount> mAlternativeNames;
    /**
     * Skip certificate chain validation.
     */
    bool mSkipValidation;
    /**
     * Self-signed certificate flag.
     */
    bool mIsSelfSigned;
};

/**
 * StorageItf provides API to store/retrieve certificates info.
 */
class StorageItf {
public:
    /**
     * Adds new certificate info to the storage.
     *
     * @param certType certificate type.
     * @param certInfo certificate information.
     * @return Error.
     */
    virtual Error AddCertInfo(const String& certType, const CertInfo& certInfo) = 0;

    /**
     * Returns information about certificate with specified issuer and serial number.
     *
     * @param issuer certificate issuer.
     * @param serial serial number.
     * @param cert result certificate.
     * @return Error.
     */
    virtual Error GetCertInfo(const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& cert) = 0;

    /**
     * Returns info for all certificates with specified certificate type.
     *
     * @param certType certificate type.
     * @param[out] certsInfo result certificates info.
     * @return Error.
     */
    virtual Error GetCertsInfo(const String& certType, Array<CertInfo>& certsInfo) = 0;

    /**
     * Removes certificate with specified certificate type and url.
     *
     * @param certType certificate type.
     * @param certURL certificate URL.
     * @return Error.
     */
    virtual Error RemoveCertInfo(const String& certType, const String& certURL) = 0;

    /**
     * Removes all certificates with specified certificate type.
     *
     * @param certType certificate type.
     * @return Error.
     */
    virtual Error RemoveAllCertsInfo(const String& certType) = 0;

    /**
     * Destroys certificate info storage.
     */
    virtual ~StorageItf() = default;
};

/**
 * Provides API to manage certificates of the same IAM certificate type.
 */
class CertModule {
public:
    /**
     * Initializes certificate module.
     *
     * @param certType certificate type.
     * @param config module config.
     * @param x509Provider provider of x509 certificates, csr, keys.
     * @param hsm a reference to hardware security module.
     * @param storage a reference to certificate storage.
     * @return Error.
     */
    Error Init(const String& certType, const ModuleConfig& config, crypto::x509::ProviderItf& x509Provider, HSMItf& hsm,
        StorageItf& storage);

    /**
     * Returns IAM module certificate type.
     *
     * @return const String&.
     */
    const String& GetCertType() const { return mCertType; }

    /**
     * Returns information about certificate with specified issuer and serial number.
     *
     * @param issuer certificate issuer.
     * @param serial serial number.
     * @param resCert result certificate.
     * @return Error.
     */
    Error GetCertificate(const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert);

    /**
     * Owns the module.
     *
     * @param password certificate password.
     * @return Error.
     */
    Error SetOwner(const String& password);

    /**
     * Removes all module certificates.
     *
     * @return Error.
     */
    Error Clear();

    /**
     * Generates private key.
     *
     * @param password owner password.
     * @return RetWithError<SharedPtr<crypto::PrivateKeyItf>> .
     */
    RetWithError<SharedPtr<crypto::PrivateKeyItf>> CreateKey(const String& password);

    /**
     * Creates certificate request.
     *
     * @param subjectCommonName common name of the subject.
     * @param privKey private key.
     * @param[out] pemCSR result csr in PEM.
     * @return Error.
     */
    Error CreateCSR(const String& subjectCommonName, const crypto::PrivateKeyItf& privKey, String& pemCSR);

    /**
     * Applies certificate to a module.
     *
     * @param pemCert certificate chain in PEM format.
     * @param[out] info result certificate information.
     * @returns Error.
     */
    Error ApplyCert(const String& pemCert, CertInfo& info);

    /**
     * Creates a self signed certificate.
     *
     * @param password owner password.
     * @returns Error.
     */
    Error CreateSelfSignedCert(const String& password);

    /**
     * Returns certificate module configuration.
     *
     * @return ModuleConfig.
     */
    ModuleConfig GetModuleConfig() const { return mModuleConfig; }

private:
    static constexpr auto cDNStringLen               = AOS_CONFIG_CERTHANDLER_DN_STRING_LEN;
    static constexpr auto cValidSelfSignedCertPeriod = Years(100);

    static constexpr auto cOidExtensionExtendedKeyUsage = "2.5.29.37";
    static constexpr auto cOidExtKeyUsageServerAuth     = "1.3.6.1.5.5.7.3.1";
    static constexpr auto cOidExtKeyUsageClientAuth     = "1.3.6.1.5.5.7.3.2";

    using ModuleCertificates    = StaticArray<CertInfo, cCertsPerModule>;
    using CertificateChain      = StaticArray<crypto::x509::Certificate, crypto::cCertChainSize>;
    using SelfSignedCertificate = StaticString<crypto::cCertPEMLen>;

    Error ValidateConfig();
    Error RemoveInvalidCerts(const String& password);
    Error RemoveInvalidKeys(const String& password);
    Error TrimCerts(const String& password);
    Error CheckCertChain(const Array<crypto::x509::Certificate>& chain);
    Error SyncValidCerts(const Array<CertInfo>& validCert);

    crypto::x509::ProviderItf* mX509Provider {};
    HSMItf*                    mHSM {};
    StorageItf*                mStorage {};

    StaticString<cCertTypeLen> mCertType;
    ModuleConfig               mModuleConfig {};

    StaticArray<StaticString<cURLLen>, cCertsPerModule> mInvalidCerts, mInvalidKeys;

    StaticAllocator<Max(2U * sizeof(ModuleCertificates),
        sizeof(SelfSignedCertificate) + sizeof(CertInfo) + sizeof(crypto::x509::CertificateChain)
            + sizeof(ModuleCertificates) + sizeof(crypto::x509::Certificate),
        sizeof(crypto::x509::CSR))>
        mAllocator;
};

} // namespace certhandler
} // namespace iam
} // namespace aos

#endif
