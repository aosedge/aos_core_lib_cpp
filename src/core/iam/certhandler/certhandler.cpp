/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "certhandler.hpp"

namespace aos::iam::certhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

CertHandler::CertHandler(AllocatorItf& allocator)
    : mAllocator(&allocator)
{
}

Error CertHandler::RegisterModule(CertModule& certModule)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Register module: type=" << certModule.GetStorageID();

    return AOS_ERROR_WRAP(mModules.PushBack(&certModule));
}

Error CertHandler::GetStorageIDs(Array<StaticString<cStorageIDLen>>& storageIDs) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get all registered IAM certificate types";

    for (const auto certModule : mModules) {
        auto err = storageIDs.PushBack(certModule->GetStorageID());
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err.Value());
        }
    }

    return ErrorEnum::eNone;
}

Error CertHandler::SetOwner(const String& storageID, const String& password)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Set owner" << Log::Field("type", storageID);

    auto* certModule = FindModule(storageID);
    if (certModule == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = certModule->SetOwner(password); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::Clear(const String& storageID)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Clear" << Log::Field("type", storageID);

    auto* certModule = FindModule(storageID);
    if (certModule == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = certModule->Clear(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::CreateKey(
    const String& storageID, const String& subjectCommonName, const String& password, String& pemCSR)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Create key" << Log::Field("type", storageID) << Log::Field("subject", subjectCommonName);

    auto* certModule = FindModule(storageID);
    if (certModule == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto key = certModule->CreateKey(password);
    if (!key.mError.IsNone()) {
        return key.mError;
    }

    if (auto err = certModule->CreateCSR(subjectCommonName, *key.mValue, pemCSR); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::ApplyCertificate(const String& storageID, const String& pemCert, CertInfo& info)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Apply cert" << Log::Field("type", storageID);

    auto* certModule = FindModule(storageID);
    if (certModule == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = certModule->ApplyCert(pemCert, info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return UpdateCerts(*certModule);
}

Error CertHandler::GetCert(
    const String& storageID, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert) const
{
    LockGuard lock {mMutex};

    StaticString<crypto::cSerialNumStrLen> serialInHex;

    auto err = serialInHex.ByteArrayToHex(serial);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Get certificate" << Log::Field("type", storageID) << Log::Field("serial", serialInHex);

    auto* certModule = FindModule(storageID);
    if (certModule == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    err = certModule->GetCertificate(issuer, serial, resCert);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::SubscribeListener(const String& storageID, iamclient::CertListenerItf& certListener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe certificate listener" << Log::Field("type", storageID);

    auto* certModule = FindModule(storageID);
    if (certModule == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto certInfo = MakeUnique<CertInfo>(mAllocator);
    if (!certInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto err = certModule->GetCertificate(Array<uint8_t>(), Array<uint8_t>(), *certInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mCertListenerSubscriptions.EmplaceBack(storageID, *certInfo, &certListener);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::UnsubscribeListener(iamclient::CertListenerItf& certListener)
{
    LockGuard lock {mMutex};

    if (mCertListenerSubscriptions.RemoveIf([&certListener](const CertListenerSubscription& subscription) {
            return subscription.mCertListener == &certListener;
        })
        == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::CreateSelfSignedCert(const String& storageID, const String& password)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Create self signed cert" << Log::Field("type", storageID);

    auto* certModule = FindModule(storageID);
    if (certModule == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = certModule->CreateSelfSignedCert(password); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return UpdateCerts(*certModule);
}

RetWithError<ModuleConfig> CertHandler::GetModuleConfig(const String& storageID) const
{
    LockGuard lock {mMutex};

    const auto* certModule = FindModule(storageID);
    if (certModule == nullptr) {
        return {ModuleConfig(), AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
    }

    return {certModule->GetModuleConfig(), ErrorEnum::eNone};
}

CertHandler::~CertHandler()
{
    LOG_DBG() << "Close certificate handler";
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

CertModule* CertHandler::FindModule(const String& storageID) const
{
    auto certModule
        = mModules.FindIf([storageID](const CertModule* certModule) { return certModule->GetStorageID() == storageID; });

    return certModule != mModules.end() ? *certModule : nullptr;
}

Error CertHandler::UpdateCerts(CertModule& certModule)
{
    auto certInfo = MakeUnique<CertInfo>(mAllocator);
    if (!certInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = certModule.GetCertificate(Array<uint8_t>(), Array<uint8_t>(), *certInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (auto& subscription : mCertListenerSubscriptions) {
        if (subscription.mStorageID != certModule.GetStorageID()) {
            continue;
        }

        if (subscription.mCertInfo != *certInfo) {
            LOG_INF() << "Cert changed" << Log::Field("type", subscription.mStorageID);

            subscription.mCertListener->OnCertChanged(*certInfo);
            subscription.mCertInfo = *certInfo;
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::iam::certhandler
