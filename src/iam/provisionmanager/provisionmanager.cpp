/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/provisionmanager.hpp"
#include "log.hpp"

namespace aos::iam::provisionmanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ProvisionManager::Init(ProvisionManagerCallbackItf& callback, certhandler::CertHandlerItf& certHandler)
{
    LOG_DBG() << "Init provision manager";

    mCallback    = &callback;
    mCertHandler = &certHandler;

    return aos::ErrorEnum::eNone;
}

Error ProvisionManager::StartProvisioning(const String& password)
{
    LOG_DBG() << "Start provisioning";

    auto err = mCallback->OnStartProvisioning(password);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    CertTypes certTypes;

    if (err = mCertHandler->GetCertTypes(certTypes); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& certType : certTypes) {
        LOG_DBG() << "Clear cert storage: type=" << certType;

        if (err = mCertHandler->Clear(certType); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& certType : certTypes) {
        LOG_DBG() << "Set owner: type=" << certType;

        if (err = mCertHandler->SetOwner(certType, password); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto certModuleConfig = mCertHandler->GetModuleConfig(certType);
        if (!certModuleConfig.mError.IsNone()) {
            return AOS_ERROR_WRAP(certModuleConfig.mError);
        }

        if (certModuleConfig.mValue.mIsSelfSigned) {
            LOG_DBG() << "Create self signed cert: type=" << certType;

            if (err = mCertHandler->CreateSelfSignedCert(certType, password); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    if (err = mCallback->OnEncryptDisk(password); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return aos::ErrorEnum::eNone;
}

Error ProvisionManager::FinishProvisioning(const String& password)
{
    LOG_DBG() << "Finish provisioning";

    return AOS_ERROR_WRAP(mCallback->OnFinishProvisioning(password));
}

Error ProvisionManager::Deprovision(const String& password)
{
    LOG_DBG() << "Deprovision";

    return AOS_ERROR_WRAP(mCallback->OnDeprovision(password));
}

RetWithError<CertTypes> ProvisionManager::GetCertTypes()
{
    LOG_DBG() << "Get cert types";

    CertTypes certTypes;

    auto err = mCertHandler->GetCertTypes(certTypes);
    if (!err.IsNone()) {
        return {certTypes, AOS_ERROR_WRAP(err)};
    }

    for (auto it = certTypes.begin(); it != certTypes.end();) {
        auto certModuleConfig = mCertHandler->GetModuleConfig(*it);
        if (!certModuleConfig.mError.IsNone()) {
            return {certTypes, AOS_ERROR_WRAP(certModuleConfig.mError)};
        }

        if (!certModuleConfig.mValue.mIsSelfSigned) {
            ++it;

            continue;
        }

        auto item = certTypes.Remove(it);
        if (!item.mError.IsNone()) {
            return {certTypes, item.mError};
        }

        it = item.mValue;
    }

    return {certTypes, aos::ErrorEnum::eNone};
}

Error ProvisionManager::CreateKey(const String& certType, const String& subject, const String& password, String& csr)
{
    LOG_DBG() << "Create key: type=" << certType;

    return AOS_ERROR_WRAP(mCertHandler->CreateKey(certType, subject, password, csr));
}

Error ProvisionManager::ApplyCert(const String& certType, const String& pemCert, certhandler::CertInfo& certInfo)
{
    LOG_DBG() << "Apply cert: type=" << certType;

    return AOS_ERROR_WRAP(mCertHandler->ApplyCertificate(certType, pemCert, certInfo));
}

Error ProvisionManager::GetCert(
    const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, certhandler::CertInfo& resCert)
{
    LOG_DBG() << "Get cert: type=" << certType;

    return AOS_ERROR_WRAP(mCertHandler->GetCertificate(certType, issuer, serial, resCert));
}

Error ProvisionManager::SubscribeCertChanged(const String& certType, certhandler::CertReceiverItf& certReceiver)
{
    LOG_DBG() << "Subscribe cert receiver: type=" << certType;

    return AOS_ERROR_WRAP(mCertHandler->SubscribeCertChanged(certType, certReceiver));
}

Error ProvisionManager::UnsubscribeCertChanged(certhandler::CertReceiverItf& certReceiver)
{
    LOG_DBG() << "Unsubscribe cert receiver";

    return AOS_ERROR_WRAP(mCertHandler->UnsubscribeCertChanged(certReceiver));
}

} // namespace aos::iam::provisionmanager
