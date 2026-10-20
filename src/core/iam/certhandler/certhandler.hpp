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
     *
     * @param allocator allocator to use for temporary objects.
     */
    explicit CertHandler(AllocatorItf& allocator);

    /**
     * Registers module.
     *
     * @param certModule a reference to a module.
     * @returns Error.
     */
    Error RegisterModule(CertModule& certModule);

    /**
     * Returns IAM cert types.
     *
     * @param[out] storageIDs result certificate storage IDs.
     * @returns Error.
     */
    Error GetStorageIDs(Array<StaticString<cStorageIDLen>>& storageIDs) const override;

    /**
     * Owns security storage.
     *
     * @param storageID certificate storage ID.
     * @param password owner password.
     * @returns Error.
     */
    Error SetOwner(const String& storageID, const String& password) override;

    /**
     * Clears security storage.
     *
     * @param storageID certificate storage ID.
     * @returns Error.
     */
    Error Clear(const String& storageID) override;

    /**
     * Creates key pair.
     *
     * @param storageID certificate storage ID.
     * @param subjectCommonName common name of the subject.
     * @param password owner password.
     * @param[out] pemCSR certificate signing request in PEM.
     * @returns Error.
     */
    Error CreateKey(
        const String& storageID, const String& subjectCommonName, const String& password, String& pemCSR) override;

    /**
     * Applies certificate.
     *
     * @param storageID certificate storage ID.
     * @param pemCert certificate in a pem format.
     * @param[out] info result certificate information.
     * @returns Error.
     */
    Error ApplyCertificate(const String& storageID, const String& pemCert, CertInfo& info) override;

    /**
     * Updates root certificates (replaces the whole set).
     *
     * @param pemCerts certificates in PEM format.
     * @param[out] infos result certificate information.
     * @returns Error.
     */
    Error UpdateRootCerts(const Array<StaticString<crypto::cCertPEMLen>>& pemCerts, Array<CertInfo>& infos) override;

    /**
     * Creates a self signed certificate.
     *
     * @param storageID certificate storage ID.
     * @param password owner password.
     * @returns Error.
     */
    Error CreateSelfSignedCert(const String& storageID, const String& password) override;

    /**
     * Returns module configuration.
     *
     * @param storageID certificate storage ID.
     * @return RetWithError<ModuleConfig>
     */
    RetWithError<ModuleConfig> GetModuleConfig(const String& storageID) const override;

    /**
     * Returns certificate info.
     *
     * @param storageID certificate storage ID.
     * @param issuer issuer name.
     * @param serial serial number.
     * @param[out] resCert result certificate.
     * @returns Error.
     */
    Error GetCert(const String& storageID, const Array<uint8_t>& issuer, const Array<uint8_t>& serial,
        CertInfo& resCert) const override;

    /**
     * Returns root certificates.
     *
     * @param[out] infos result certificates.
     * @returns Error.
     */
    Error GetRootCerts(Array<CertInfo>& infos) const override;

    /**
     * Subscribes certificates listener.
     *
     * @param storageID certificate storage ID.
     * @param certListener certificate listener.
     * @returns Error.
     */
    Error SubscribeListener(const String& storageID, iamclient::CertListenerItf& certListener) override;

    /**
     * Subscribes root certificates listener.
     *
     * @param certListener certificate listener.
     * @returns Error.
     */
    Error SubscribeRootCertsListener(iamclient::CertListenerItf& certListener) override;

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

    CertModule* FindModule(const String& storageID) const;
    CertModule* FindRootModule() const;
    Error       Subscribe(CertModule& certModule, iamclient::CertListenerItf& certListener);
    Error       UpdateCerts(CertModule& certModule);

    mutable Mutex                                     mMutex;
    StaticArray<CertModule*, cIAMCertModulesMaxCount> mModules;

    struct CertListenerSubscription {
        CertListenerSubscription(
            const String& storageID, const CertInfo& certInfo, iamclient::CertListenerItf* listener)
            : mStorageID(storageID)
            , mCertInfo(certInfo)
            , mCertListener(listener)
        {
        }

        StaticString<cStorageIDLen> mStorageID;
        CertInfo                    mCertInfo;
        iamclient::CertListenerItf* mCertListener {};
    };

    StaticArray<CertListenerSubscription, cIAMCertSubsMaxCount> mCertListenerSubscriptions;
    AllocatorItf*                                               mAllocator {};
};

/** @}*/

} // namespace aos::iam::certhandler

#endif
