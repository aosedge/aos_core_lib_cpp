/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CERTHANDLER_ITF_CERTHANDLER_HPP_
#define AOS_CORE_IAM_CERTHANDLER_ITF_CERTHANDLER_HPP_

#include <core/common/iamclient/itf/certprovider.hpp>
#include <core/common/tools/thread.hpp>
#include <core/iam/config.hpp>

namespace aos::iam::certhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Max number of certificate modules.
 */
constexpr auto cIAMCertModulesMaxCount = AOS_CONFIG_CERTHANDLER_MODULES_MAX_COUNT;

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

    static Array<const char* const> GetStrings()
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
    size_t mMaxCertificates {};
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
    bool mSkipValidation {};
    /**
     * Self-signed certificate flag.
     */
    bool mIsSelfSigned {};
};

/**
 * Certificate handler interface.
 */
class CertHandlerItf : public iamclient::CertProviderItf {
public:
    /**
     * Returns IAM cert types.
     *
     * @param[out] storageIDs result certificate storage IDs.
     * @returns Error.
     */
    virtual Error GetStorageIDs(Array<StaticString<cStorageIDLen>>& storageIDs) const = 0;

    /**
     * Owns security storage.
     *
     * @param storageID certificate storage ID.
     * @param password owner password.
     * @returns Error.
     */
    virtual Error SetOwner(const String& storageID, const String& password) = 0;

    /**
     * Clears security storage.
     *
     * @param storageID certificate storage ID.
     * @returns Error.
     */
    virtual Error Clear(const String& storageID) = 0;

    /**
     * Creates key pair.
     *
     * @param storageID certificate storage ID.
     * @param subjectCommonName common name of the subject.
     * @param password owner password.
     * @param[out] pemCSR certificate signing request in PEM.
     * @returns Error.
     */
    virtual Error CreateKey(
        const String& storageID, const String& subjectCommonName, const String& password, String& pemCSR)
        = 0;

    /**
     * Applies certificate.
     *
     * @param storageID certificate storage ID.
     * @param pemCert certificate in a pem format.
     * @param[out] info result certificate information.
     * @returns Error.
     */
    virtual Error ApplyCertificate(const String& storageID, const String& pemCert, CertInfo& info) = 0;

    /**
     * Creates a self signed certificate.
     *
     * @param storageID certificate storage ID.
     * @param password owner password.
     * @returns Error.
     */
    virtual Error CreateSelfSignedCert(const String& storageID, const String& password) = 0;

    /**
     * Returns module configuration.
     *
     * @param storageID certificate storage ID.
     * @returns RetWithError<ModuleConfig>.
     */
    virtual RetWithError<ModuleConfig> GetModuleConfig(const String& storageID) const = 0;

    /**
     * Destroys certificate handler interface.
     */
    virtual ~CertHandlerItf() = default;
};

/** @}*/

} // namespace aos::iam::certhandler

#endif
