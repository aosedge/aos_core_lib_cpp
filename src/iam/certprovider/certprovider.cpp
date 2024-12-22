/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/certprovider.hpp"
#include "log.hpp"

namespace aos::iam::certprovider {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error CertProvider::Init(certhandler::CertHandlerItf& certHandler)
{
    LOG_DBG() << "Init cert provider";

    mCertHandler = &certHandler;

    return aos::ErrorEnum::eNone;
}

Error CertProvider::GetCert(const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial,
    certhandler::CertInfo& resCert) const
{
    LOG_DBG() << "Get cert: type=" << certType;

    return AOS_ERROR_WRAP(mCertHandler->GetCertificate(certType, issuer, serial, resCert));
}

Error CertProvider::SubscribeCertChanged(const String& certType, certhandler::CertReceiverItf& certReceiver)
{
    LOG_DBG() << "Subscribe cert receiver: type=" << certType;

    return AOS_ERROR_WRAP(mCertHandler->SubscribeCertChanged(certType, certReceiver));
}

Error CertProvider::UnsubscribeCertChanged(certhandler::CertReceiverItf& certReceiver)
{
    LOG_DBG() << "Unsubscribe cert receiver";

    return AOS_ERROR_WRAP(mCertHandler->UnsubscribeCertChanged(certReceiver));
}

} // namespace aos::iam::certprovider
