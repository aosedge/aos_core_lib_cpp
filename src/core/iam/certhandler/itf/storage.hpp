/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CERTHANDLER_ITF_STORAGE_HPP_
#define AOS_CORE_IAM_CERTHANDLER_ITF_STORAGE_HPP_

#include <core/common/types/common.hpp>

namespace aos::iam::certhandler {

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

} // namespace aos::iam::certhandler

#endif
