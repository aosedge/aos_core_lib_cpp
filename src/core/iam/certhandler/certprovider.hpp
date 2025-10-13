/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CERTHANDLER_CERTPROVIDER_HPP_
#define AOS_CORE_IAM_CERTHANDLER_CERTPROVIDER_HPP_

#include <core/common/iamclient/itf/certprovider.hpp>

#include "certhandler.hpp"

namespace aos::iam::certhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Cert provider.
 */
class CertProvider : public iamclient::CertProviderItf {
public:
    /**
     * Initializes cert provider.
     *
     * @param certHandler certificate handler.
     * @returns Error.
     */
    Error Init(certhandler::CertHandlerItf& certHandler);

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
     * Subscribes certificates receiver.
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

private:
    certhandler::CertHandlerItf* mCertHandler {};
};

/** @} */ // end of iam group

} // namespace aos::iam::certhandler

#endif /* AOS_PROVISIONMANAGER_HPP_ */
