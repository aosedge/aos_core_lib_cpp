/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IAMCLIENT_ITF_CERTHANDLER_HPP_
#define AOS_CORE_COMMON_IAMCLIENT_ITF_CERTHANDLER_HPP_

#include <core/common/types/common.hpp>

namespace aos::iamclient {

/**
 * Certificate handler interface.
 */
class CertHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~CertHandlerItf() = default;

    /**
     * Creates key.
     *
     * @param nodeID node ID.
     * @param certType certificate type.
     * @param subject subject.
     * @param password password.
     * @param[out] csr certificate signing request.
     * @returns Error.
     */
    virtual Error CreateKey(
        const String& nodeID, const String& certType, const String& subject, const String& password, String& csr)
        = 0;

    /**
     * Applies certificate.
     *
     * @param nodeID node ID.
     * @param certType certificate type.
     * @param pemCert certificate in PEM.
     * @param certInfo certificate info.
     * @returns Error.
     */
    virtual Error ApplyCert(const String& nodeID, const String& certType, const String& pemCert, CertInfo& certInfo)
        = 0;
};

} // namespace aos::iamclient

#endif
