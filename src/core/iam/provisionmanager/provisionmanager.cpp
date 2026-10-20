/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "provisionmanager.hpp"

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
    LOG_INF() << "Start provisioning";

    auto err = mCallback->OnStartProvisioning(password);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StorageIDs storageIDs;

    err = mCertHandler->GetStorageIDs(storageIDs);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& storageID : storageIDs) {
        LOG_DBG() << "Clear cert storage" << Log::Field("type", storageID);

        err = mCertHandler->Clear(storageID);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& storageID : storageIDs) {
        LOG_DBG() << "Set owner" << Log::Field("type", storageID);

        err = mCertHandler->SetOwner(storageID, password);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto certModuleConfig = mCertHandler->GetModuleConfig(storageID);
        if (!certModuleConfig.mError.IsNone()) {
            return AOS_ERROR_WRAP(certModuleConfig.mError);
        }

        if (certModuleConfig.mValue.mIsSelfSigned) {
            LOG_DBG() << "Create self signed cert" << Log::Field("type", storageID);

            err = mCertHandler->CreateSelfSignedCert(storageID, password);
            if (!err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    err = mCallback->OnEncryptDisk(password);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return aos::ErrorEnum::eNone;
}

Error ProvisionManager::FinishProvisioning(const String& password)
{
    LOG_INF() << "Finish provisioning";

    return AOS_ERROR_WRAP(mCallback->OnFinishProvisioning(password));
}

Error ProvisionManager::Deprovision(const String& password)
{
    LOG_INF() << "Deprovision";

    return AOS_ERROR_WRAP(mCallback->OnDeprovision(password));
}

RetWithError<StorageIDs> ProvisionManager::GetCertTypes() const
{
    LOG_DBG() << "Get cert types";

    StorageIDs storageIDs;

    if (auto err = mCertHandler->GetStorageIDs(storageIDs); !err.IsNone()) {
        return {storageIDs, AOS_ERROR_WRAP(err)};
    }

    for (auto it = storageIDs.begin(); it != storageIDs.end();) {
        auto certModuleConfig = mCertHandler->GetModuleConfig(*it);
        if (!certModuleConfig.mError.IsNone()) {
            return {storageIDs, AOS_ERROR_WRAP(certModuleConfig.mError)};
        }

        if (!certModuleConfig.mValue.mIsSelfSigned) {
            ++it;

            continue;
        }

        it = storageIDs.Erase(it);
    }

    return {storageIDs, aos::ErrorEnum::eNone};
}

Error ProvisionManager::CreateKey(const String& storageID, const String& subject, const String& password, String& csr)
{
    LOG_DBG() << "Create key" << Log::Field("type", storageID) << Log::Field("subject", subject);

    return AOS_ERROR_WRAP(mCertHandler->CreateKey(storageID, subject, password, csr));
}

Error ProvisionManager::ApplyCert(const String& storageID, const String& pemCert, CertInfo& certInfo)
{
    LOG_DBG() << "Apply cert" << Log::Field("type", storageID);

    return AOS_ERROR_WRAP(mCertHandler->ApplyCertificate(storageID, pemCert, certInfo));
}

} // namespace aos::iam::provisionmanager
