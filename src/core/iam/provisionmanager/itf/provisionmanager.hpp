/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_PROVISIONMANAGER_ITF_PROVISIONMANAGER_HPP_
#define AOS_CORE_IAM_PROVISIONMANAGER_ITF_PROVISIONMANAGER_HPP_

#include <core/iam/certhandler/itf/certhandler.hpp>

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
using StorageIDs = aos::StaticArray<StaticString<cStorageIDLen>, certhandler::cIAMCertModulesMaxCount>;

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
     * @returns RetWithError<StorageIDs>.
     */
    virtual RetWithError<StorageIDs> GetCertKeyStorageIDs() const = 0;

    /**
     * Creates key.
     *
     * @param storageID certificate storage ID.
     * @param subject subject.
     * @param password password.
     * @param csr certificate signing request.
     * @returns Error.
     */
    virtual Error CreateKey(const String& storageID, const String& subject, const String& password, String& csr) = 0;

    /**
     * Applies certificate.
     *
     * @param storageID certificate storage ID.
     * @param pemCert certificate in PEM.
     * @param certInfo certificate info.
     * @returns Error.
     */
    virtual Error ApplyCert(const String& storageID, const String& pemCert, CertInfo& certInfo) = 0;

    /**
     * Updates root certificates.
     *
     * @param pemCerts root certificates in PEM format.
     * @param[out] infos result certificate information.
     * @returns Error.
     */
    virtual Error UpdateRootCerts(const Array<StaticString<crypto::cCertPEMLen>>& pemCerts, Array<CertInfo>& infos) = 0;

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

/** @} */ // end of iam group

} // namespace aos::iam::provisionmanager

#endif /* AOS_PROVISIONMANAGER_HPP_ */
