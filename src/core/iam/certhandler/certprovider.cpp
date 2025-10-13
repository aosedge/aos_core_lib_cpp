/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "certprovider.hpp"

namespace aos::iam::certhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error CertProvider::Init(certhandler::CertHandlerItf& certHandler)
{
    LOG_DBG() << "Init cert provider";

    mCertHandler = &certHandler;

    return aos::ErrorEnum::eNone;
}

Error CertProvider::GetCert(
    const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert) const
{
    LOG_DBG() << "Get cert: type=" << certType;

    return AOS_ERROR_WRAP(mCertHandler->GetCertificate(certType, issuer, serial, resCert));
}

Error CertProvider::SubscribeListener(const String& certType, iamclient::CertListenerItf& certListener)
{
    LOG_DBG() << "Subscribe cert listener: type=" << certType;

    return AOS_ERROR_WRAP(mCertHandler->SubscribeListener(certType, certListener));
}

Error CertProvider::UnsubscribeListener(iamclient::CertListenerItf& certListener)
{
    LOG_DBG() << "Unsubscribe cert listener";

    return AOS_ERROR_WRAP(mCertHandler->UnsubscribeListener(certListener));
}

} // namespace aos::iam::certhandler
