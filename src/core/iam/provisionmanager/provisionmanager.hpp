/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_PROVISIONMANAGER_PROVISIONMANAGER_HPP_
#define AOS_CORE_IAM_PROVISIONMANAGER_PROVISIONMANAGER_HPP_

#include "itf/provisionmanager.hpp"

namespace aos::iam::provisionmanager {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Provision manager.
 */
class ProvisionManager : public ProvisionManagerItf {
public:
    /**
     * Initializes provision manager.
     *
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
    RetWithError<CertTypes> GetCertTypes() const override;

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
    Error ApplyCert(const String& certType, const String& pemCert, CertInfo& certInfo) override;

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
