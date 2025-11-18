/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CERTHANDLER_CERTHANDLER_HPP_
#define AOS_CORE_IAM_CERTHANDLER_CERTHANDLER_HPP_

#include <core/common/iamclient/itf/certprovider.hpp>
#include <core/common/tools/thread.hpp>
#include <core/iam/config.hpp>

#include "certmodule.hpp"
#include "itf/certhandler.hpp"

namespace aos::iam::certhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

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
    Error GetCertTypes(Array<StaticString<cCertTypeLen>>& certTypes) const override;

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
     * Returns certificate info.
     *
     * @param certType certificate type.
     * @param issuer issuer name.
     * @param serial serial number.
     * @param[out] resCert result certificate.
     * @returns Error.
     */
    Error GetCert(const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial,
        CertInfo& resCert) const override;

    /**
     * Subscribes certificates listener.
     *
     * @param certType certificate type.
     * @param certListener certificate listener.
     * @returns Error.
     */
    Error SubscribeListener(const String& certType, iamclient::CertListenerItf& certListener) override;

    /**
     * Unsubscribes certificate listener.
     *
     * @param certListener certificate listener.
     * @returns Error.
     */
    Error UnsubscribeListener(iamclient::CertListenerItf& certListener) override;

    /**
     * Destroys certificate handler object instance.
     */
    virtual ~CertHandler();

private:
    static constexpr auto cIAMCertSubsMaxCount = AOS_CONFIG_CERTHANDLER_CERT_SUBS_MAX_COUNT;

    CertModule* FindModule(const String& certType) const;
    Error       UpdateCerts(CertModule& module);

    mutable Mutex                                     mMutex;
    StaticArray<CertModule*, cIAMCertModulesMaxCount> mModules;

    struct CertListenerSubscription {
        CertListenerSubscription(const String& certType, const CertInfo& certInfo, iamclient::CertListenerItf* listener)
            : mCertType(certType)
            , mCertInfo(certInfo)
            , mCertListener(listener)
        {
        }

        StaticString<cCertTypeLen>  mCertType;
        CertInfo                    mCertInfo;
        iamclient::CertListenerItf* mCertListener {};
    };

    StaticArray<CertListenerSubscription, cIAMCertSubsMaxCount> mCertListenerSubscriptions;
};

/** @}*/

} // namespace aos::iam::certhandler

#endif
