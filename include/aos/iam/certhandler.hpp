/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CERTHANDLER_HPP_
#define AOS_CERTHANDLER_HPP_

#include "aos/common/tools/thread.hpp"

#include "aos/iam/certmodules/certmodule.hpp"
#include "aos/iam/config.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Max number of certificate modules.
 */
constexpr auto cIAMCertModulesMaxCount = AOS_CONFIG_CERTHANDLER_MODULES_MAX_COUNT;

/**
 * Certificate handler interface.
 */
class CertHandlerItf {
public:
    /**
     * Returns IAM cert types.
     *
     * @param[out] certTypes result certificate types.
     * @returns Error.
     */
    virtual Error GetCertTypes(Array<StaticString<cCertTypeLen>>& certTypes) = 0;

    /**
     * Owns security storage.
     *
     * @param certType certificate type.
     * @param password owner password.
     * @returns Error.
     */
    virtual Error SetOwner(const String& certType, const String& password) = 0;

    /**
     * Clears security storage.
     *
     * @param certType certificate type.
     * @returns Error.
     */
    virtual Error Clear(const String& certType) = 0;

    /**
     * Creates key pair.
     *
     * @param certType certificate type.
     * @param subjectCommonName common name of the subject.
     * @param password owner password.
     * @param[out] pemCSR certificate signing request in PEM.
     * @returns Error.
     */
    virtual Error CreateKey(
        const String& certType, const String& subjectCommonName, const String& password, String& pemCSR)
        = 0;

    /**
     * Applies certificate.
     *
     * @param certType certificate type.
     * @param pemCert certificate in a pem format.
     * @param[out] info result certificate information.
     * @returns Error.
     */
    virtual Error ApplyCertificate(const String& certType, const String& pemCert, CertInfo& info) = 0;

    /**
     * Returns certificate info.
     *
     * @param certType certificate type.
     * @param issuer issuer name.
     * @param serial serial number.
     * @param[out] resCert result certificate.
     * @returns Error.
     */
    virtual Error GetCertificate(
        const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert)
        = 0;

    /**
     * Creates a self signed certificate.
     *
     * @param certType certificate type.
     * @param password owner password.
     * @returns Error.
     */
    virtual Error CreateSelfSignedCert(const String& certType, const String& password) = 0;

    /**
     * Returns module configuration.
     *
     * @param certType certificate type.
     * @returns RetWithError<ModuleConfig>.
     */
    virtual RetWithError<ModuleConfig> GetModuleConfig(const String& certType) const = 0;

    /**
     * Destroys certificate handler interface.
     */
    virtual ~CertHandlerItf() = default;

private:
    CertModule* FindModule(const String& certType) const;

    Mutex                                             mMutex;
    StaticArray<CertModule*, cIAMCertModulesMaxCount> mModules;
};

/**
 * Handles keys and certificates.
 */
class CertHandler : public CertHandlerItf, private NonCopyable {
public:
    /**
     * Creates a new object instance.
     */
    CertHandler();

    /**
     * Registers module.
     *
     * @param module a reference to a module.
     * @returns Error.
     */
    Error RegisterModule(CertModule& module);

    /**
     * Returns IAM cert types.
     *
     * @param[out] certTypes result certificate types.
     * @returns Error.
     */
    Error GetCertTypes(Array<StaticString<cCertTypeLen>>& certTypes) override;

    /**
     * Owns security storage.
     *
     * @param certType certificate type.
     * @param password owner password.
     * @returns Error.
     */
    Error SetOwner(const String& certType, const String& password) override;

    /**
     * Clears security storage.
     *
     * @param certType certificate type.
     * @returns Error.
     */
    Error Clear(const String& certType) override;

    /**
     * Creates key pair.
     *
     * @param certType certificate type.
     * @param subjectCommonName common name of the subject.
     * @param password owner password.
     * @param[out] pemCSR certificate signing request in PEM.
     * @returns Error.
     */
    Error CreateKey(
        const String& certType, const String& subjectCommonName, const String& password, String& pemCSR) override;

    /**
     * Applies certificate.
     *
     * @param certType certificate type.
     * @param pemCert certificate in a pem format.
     * @param[out] info result certificate information.
     * @returns Error.
     */
    Error ApplyCertificate(const String& certType, const String& pemCert, CertInfo& info) override;

    /**
     * Returns certificate info.
     *
     * @param certType certificate type.
     * @param issuer issuer name.
     * @param serial serial number.
     * @param[out] resCert result certificate.
     * @returns Error.
     */
    Error GetCertificate(
        const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert) override;

    /**
     * Creates a self signed certificate.
     *
     * @param certType certificate type.
     * @param password owner password.
     * @returns Error.
     */
    Error CreateSelfSignedCert(const String& certType, const String& password) override;

    /**
     * Returns module configuration.
     *
     * @param certType certificate type.
     * @return RetWithError<ModuleConfig>
     */
    RetWithError<ModuleConfig> GetModuleConfig(const String& certType) const override;

    /**
     * Destroys certificate handler object instance.
     */
    virtual ~CertHandler();

private:
    CertModule* FindModule(const String& certType) const;

    mutable Mutex                                     mMutex;
    StaticArray<CertModule*, cIAMCertModulesMaxCount> mModules;
};

/** @}*/

} // namespace certhandler
} // namespace iam
} // namespace aos

#endif
