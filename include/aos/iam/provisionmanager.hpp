/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_PROVISIONMANAGER_HPP_
#define AOS_PROVISIONMANAGER_HPP_

#include "aos/common/tools/error.hpp"
#include "aos/iam/certhandler.hpp"

namespace aos::iam::provisionmanager {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Provision manager callback interface.
 */
class ProvisionManagerCallbackItf {
public:
    /**
     * Destructor.
     */
    virtual ~ProvisionManagerCallbackItf() = default;

    /**
     * Called when provisioning starts.
     *
     * @param password password.
     * @returns Error.
     */
    virtual Error OnStartProvisioning(const String& password) = 0;

    /**
     * Called when provisioning finishes.
     *
     * @param password password.
     * @returns Error.
     */
    virtual Error OnFinishProvisioning(const String& password) = 0;

    /**
     * Called on deprovisioning.
     *
     * @param password password.
     * @returns Error.
     */
    virtual Error OnDeprovision(const String& password) = 0;

    /**
     * Called on disk encryption.
     *
     * @param password password.
     * @returns Error.
     */
    virtual Error OnEncryptDisk(const String& password) = 0;
};

/**
 * Certificate types.
 */
using CertTypes = aos::StaticArray<StaticString<certhandler::cCertTypeLen>, certhandler::cIAMCertModulesMaxCount>;

/**
 * ProvisionManager interface.
 */
class ProvisionManagerItf {
public:
    /**
     * Starts provisioning.
     *
     * @param password password.
     * @returns Error.
     */
    virtual Error StartProvisioning(const String& password) = 0;

    /**
     * Gets certificate types.
     *
     * @returns RetWithError<CertTypes>.
     */
    virtual RetWithError<CertTypes> GetCertTypes() = 0;

    /**
     * Creates key.
     *
     * @param certType certificate type.
     * @param subject subject.
     * @param password password.
     * @param csr certificate signing request.
     * @returns Error.
     */
    virtual Error CreateKey(const String& certType, const String& subject, const String& password, String& csr) = 0;

    /**
     * Applies certificate.
     *
     * @param certType certificate type.
     * @param pemCert certificate in PEM.
     * @param certInfo certificate info.
     * @returns Error.
     */
    virtual Error ApplyCert(const String& certType, const String& pemCert, certhandler::CertInfo& certInfo) = 0;

    /**
     * Returns certificate info.
     *
     * @param certType certificate type.
     * @param issuer issuer name.
     * @param serial serial number.
     * @param[out] resCert result certificate.
     * @returns Error.
     */
    virtual Error GetCert(const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial,
        certhandler::CertInfo& resCert)
        = 0;

    /**
     * Finishes provisioning.
     *
     * @param password password.
     * @returns Error.
     */
    virtual Error FinishProvisioning(const String& password) = 0;

    /**
     * Deprovisions.
     *
     * @param password password.
     * @returns Error.
     */
    virtual Error Deprovision(const String& password) = 0;

    /**
     * Destroys object instance.
     */
    virtual ~ProvisionManagerItf() = default;
};

/**
 * Provision manager.
 */
class ProvisionManager : public ProvisionManagerItf {
public:
    /**
     * Initializes provision manager.
     *
     * @param certTypeConfigs certificate type configurations.
     * @param callback provision manager callback.
     * @param certHandler certificate handler.
     * @returns Error.
     */
    Error Init(ProvisionManagerCallbackItf& callback, certhandler::CertHandlerItf& certHandler);

    /**
     * Starts provisioning.
     *
     * @param password password.
     * @returns Error.
     */
    Error StartProvisioning(const String& password) override;

    /**
     * Gets certificate types.
     *
     * @returns RetWithError<CertTypes>.
     */
    RetWithError<CertTypes> GetCertTypes() override;

    /**
     * Creates key.
     *
     * @param certType certificate type.
     * @param subject subject.
     * @param password password.
     * @param csr certificate signing request.
     * @returns Error.
     */
    Error CreateKey(const String& certType, const String& subject, const String& password, String& csr) override;

    /**
     * Applies certificate.
     *
     * @param certType certificate type.
     * @param pemCert certificate in PEM.
     * @param[out] certInfo certificate info.
     * @returns Error.
     */
    Error ApplyCert(const String& certType, const String& pemCert, certhandler::CertInfo& certInfo) override;

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
        certhandler::CertInfo& resCert) override;

    /**
     * Finishes provisioning.
     *
     * @param password password.
     * @returns Error.
     */
    Error FinishProvisioning(const String& password) override;

    /**
     * Deprovisions.
     *
     * @param password password.
     * @returns Error.
     */
    Error Deprovision(const String& password) override;

private:
    ProvisionManagerCallbackItf* mCallback {};
    certhandler::CertHandlerItf* mCertHandler {};
};

/** @} */ // end of iam group

} // namespace aos::iam::provisionmanager

#endif /* AOS_PROVISIONMANAGER_HPP_ */
