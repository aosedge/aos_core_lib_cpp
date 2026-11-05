/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IAMCLIENT_ITF_CERTPROVIDER_HPP_
#define AOS_CORE_COMMON_IAMCLIENT_ITF_CERTPROVIDER_HPP_

#include <core/common/types/common.hpp>

namespace aos::iamclient {

/**
 * Certificate listener interface.
 */
class CertKeyListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~CertKeyListenerItf() = default;

    /**
     * Processes certificate updates.
     *
     * @param info certificate info.
     */
    virtual void OnCertKeyChanged(const CertInfo& info) = 0;
};

/**
 * Cert provider interface.
 */
class CertProviderItf {
public:
    /**
     * Destroys cert provider.
     */
    virtual ~CertProviderItf() = default;

    /**
     * Returns certificate info.
     *
     * @param certType certificate type.
     * @param issuer issuer name.
     * @param serial serial number.
     * @param[out] resCert result certificate.
     * @returns Error.
     */
    virtual Error GetCertKey(
        const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert) const
        = 0;

    /**
     * Subscribes certificates listener.
     *
     * @param certType certificate type.
     * @param certListener certificate listener.
     * @returns Error.
     */
    virtual Error SubscribeCertKeyListener(const String& certType, CertKeyListenerItf& certListener) = 0;

    /**
     * Unsubscribes certificate listener.
     *
     * @param certListener certificate listener.
     * @returns Error.
     */
    virtual Error UnsubscribeListener(CertKeyListenerItf& certListener) = 0;
};

} // namespace aos::iamclient

#endif
