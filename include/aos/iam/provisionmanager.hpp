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
class ProvisionManagerCallback {
public:
    /**
     * Destructor.
     */
    virtual ~ProvisionManagerCallback() = default;

    /**
     * Called when provisioning starts.
     *
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
 * Provision manager.
 */
class ProvisionManager {
public:
    /**
     * Initializes provision manager.
     *
     * @param certTypeConfigs certificate type configurations.
     * @param callback provision manager callback.
     * @param certHandler certificate handler.
     * @returns Error.
     */
    Error Init(ProvisionManagerCallback& callback, certhandler::CertHandlerItf& certHandler);

    /**
     * Starts provisioning.
     *
     * @param password password.
     * @returns Error.
     */
    Error StartProvisioning(const String& password);

    /**
     * Gets certificate types.
     *
     * @returns RetWithError<CertTypes>.
     */
    RetWithError<CertTypes> GetCertTypes();

    /**
     * Creates key.
     *
     * @param certType certificate type.
     * @param subject subject.
     * @param password password.
     * @param csr certificate signing request.
     * @returns Error.
     */
    Error CreateKey(const String& certType, const String& subject, const String& password, String& csr);

    /**
     * Applies certificate.
     *
     * @param certType certificate type.
     * @param pemCert certificate in PEM.
     * @param[out] certInfo certificate info.
     * @returns Error.
     */
    Error ApplyCert(const String& certType, const String& pemCert, certhandler::CertInfo& certInfo);

    /**
     * Finishes provisioning.
     *
     * @param password password.
     * @returns Error.
     */
    Error FinishProvisioning(const String& password);

    /**
     * Deprovisions.
     *
     * @param password password.
     * @returns Error.
     */
    Error Deprovision(const String& password);

private:
    ProvisionManagerCallback*    mCallback {};
    certhandler::CertHandlerItf* mCertHandler {};
};

/** @} */ // end of iam group

} // namespace aos::iam::provisionmanager

#endif /* AOS_PROVISIONMANAGER_HPP_ */
