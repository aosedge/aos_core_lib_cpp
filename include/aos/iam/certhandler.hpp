/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CERTHANDLER_HPP_
#define AOS_CERTHANDLER_HPP_

#include "aos/common/tools/thread.hpp"

#include "aos/iam/config.hpp"
#include "aos/iam/modules/certmodule.hpp"

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
 * Handles keys and certificates.
 */
class CertHandler {
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
    Error GetCertTypes(Array<StaticString<cCertTypeLen>>& certTypes);

    /**
     * Owns security storage.
     *
     * @param certType certificate type.
     * @param password owner password.
     * @returns Error.
     */
    Error SetOwner(const String& certType, const String& password);

    /**
     * Clears security storage.
     *
     * @param certType certificate type.
     * @returns Error.
     */
    Error Clear(const String& certType);

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
        const String& certType, const String& subjectCommonName, const String& password, Array<uint8_t>& pemCSR);

    /**
     * Applies certificate.
     *
     * @param certType certificate type.
     * @param pemCert certificate in a pem format.
     * @param[out] info result certificate information.
     * @returns Error.
     */
    Error ApplyCertificate(const String& certType, const Array<uint8_t>& pemCert, CertInfo& info);

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
        const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert);

    /**
     * Creates a self signed certificate.
     *
     * @param certType certificate type.
     * @param password owner password.
     * @returns Error.
     */
    Error CreateSelfSignedCert(const String& certType, const String& password);

    /**
     * Destroys certificate handler object instance.
     */
    virtual ~CertHandler();

private:
    CertModule* FindModule(const String& certType) const;

    Mutex                                             mMutex;
    StaticArray<CertModule*, cIAMCertModulesMaxCount> mModules;
};

/** @}*/

} // namespace certhandler
} // namespace iam
} // namespace aos

#endif
