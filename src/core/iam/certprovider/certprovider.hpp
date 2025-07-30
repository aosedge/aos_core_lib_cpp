/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CERTPROVIDER_HPP_
#define AOS_CERTPROVIDER_HPP_

#include "aos/common/tools/error.hpp"
#include "aos/iam/certhandler.hpp"

namespace aos::iam::certprovider {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Cert provider interface.
 */
class CertProviderItf {
public:
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
        certhandler::CertInfo& resCert) const
        = 0;

    /**
     * Subscribes certificates receiver.
     *
     * @param certType certificate type.
     * @param certReceiver certificate receiver.
     * @returns Error.
     */
    virtual Error SubscribeCertChanged(const String& certType, certhandler::CertReceiverItf& certReceiver) = 0;

    /**
     * Unsubscribes certificate receiver.
     *
     * @param certReceiver certificate receiver.
     * @returns Error.
     */
    virtual Error UnsubscribeCertChanged(certhandler::CertReceiverItf& certReceiver) = 0;

    /**
     * Destroys cert provider.
     */
    virtual ~CertProviderItf() = default;
};

/**
 * Cert provider.
 */
class CertProvider : public CertProviderItf {
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
        certhandler::CertInfo& resCert) const override;

    /**
     * Subscribes certificates receiver.
     *
     * @param certType certificate type.
     * @param certReceiver certificate receiver.
     * @returns Error.
     */
    Error SubscribeCertChanged(const String& certType, certhandler::CertReceiverItf& certReceiver) override;

    /**
     * Unsubscribes certificate receiver.
     *
     * @param certReceiver certificate receiver.
     * @returns Error.
     */
    Error UnsubscribeCertChanged(certhandler::CertReceiverItf& certReceiver) override;

private:
    certhandler::CertHandlerItf* mCertHandler {};
};

/** @} */ // end of iam group

} // namespace aos::iam::certprovider

#endif /* AOS_PROVISIONMANAGER_HPP_ */
