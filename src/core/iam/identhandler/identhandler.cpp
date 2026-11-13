/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/crypto/cryptoutils.hpp>
#include <core/common/tools/logger.hpp>

#include "identhandler.hpp"

namespace aos::iam::identhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error IdentHandler::Init(IdentModuleItf& identModule, iamclient::CertProviderItf& certProvider,
    crypto::CertLoaderItf& certLoader, AllocatorItf& allocator, bool provisioningMode)
{
    LOG_DBG() << "Init ident handler" << Log::Field("provisioningMode", provisioningMode);

    mIdentModule      = &identModule;
    mCertProvider     = &certProvider;
    mCertLoader       = &certLoader;
    mAllocator        = &allocator;
    mProvisioningMode = provisioningMode;
    mSubscribed       = false;
    mSystemID.Clear();

    return ErrorEnum::eNone;
}

Error IdentHandler::Start()
{
    LOG_DBG() << "Start ident handler";

    if (mProvisioningMode) {
        return ErrorEnum::eNone;
    }

    // Cert provider is called without holding mMutex: it notifies listeners under its own lock.
    StaticString<cIDLen> systemID;

    if (auto err = GetSystemIDFromOnlineCert(systemID); !err.IsNone()) {
        LOG_WRN() << "System ID is not available yet" << Log::Field(err);
    } else {
        LockGuard lock {mMutex};

        mSystemID = systemID;
    }

    if (auto err = mCertProvider->SubscribeCertKeyListener(cOnlineCert, *this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LockGuard lock {mMutex};

    mSubscribed = true;

    return ErrorEnum::eNone;
}

Error IdentHandler::Stop()
{
    LOG_DBG() << "Stop ident handler";

    bool subscribed = false;

    {
        LockGuard lock {mMutex};

        subscribed  = mSubscribed;
        mSubscribed = false;
    }

    if (subscribed) {
        if (auto err = mCertProvider->UnsubscribeListener(*this); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error IdentHandler::GetSystemInfo(SystemInfo& info)
{
    LockGuard lock {mMutex};

    if (auto err = mIdentModule->GetUnitModelInfo(info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // Once subscribed, System ID is updated by OnCertChanged. Fetching the cert here is safe only while
    // not subscribed: cert provider calls OnCertChanged under its own lock, which would invert the lock order.
    if (mSystemID.IsEmpty() && !mSubscribed) {
        if (auto err = GetSystemIDFromOnlineCert(mSystemID); !err.IsNone()) {
            LOG_WRN() << "System ID is not available yet" << Log::Field(err);
        }
    }

    info.mSystemID = mSystemID;

    LOG_DBG() << "Get system info" << Log::Field("systemID", info.mSystemID) << Log::Field("unitModel", info.mUnitModel)
              << Log::Field("version", info.mVersion);

    return ErrorEnum::eNone;
}

Error IdentHandler::GetSubjects(Array<StaticString<cIDLen>>& subjects)
{
    LOG_DBG() << "Get subjects";

    if (auto err = mIdentModule->GetSubjects(subjects); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error IdentHandler::SubscribeListener(iamclient::SubjectsListenerItf& subjectsListener)
{
    LOG_DBG() << "Subscribe subjects listener";

    return mIdentModule->SubscribeListener(subjectsListener);
}

Error IdentHandler::UnsubscribeListener(iamclient::SubjectsListenerItf& subjectsListener)
{
    LOG_DBG() << "Unsubscribe subjects listener";

    return mIdentModule->UnsubscribeListener(subjectsListener);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void IdentHandler::OnCertKeyChanged(const CertInfo& info)
{
    LOG_DBG() << "Online certificate changed, updating System ID";

    // Called by cert provider under its lock: use the provided cert info instead of calling back into it.
    StaticString<cIDLen> systemID;

    if (auto err = GetSystemIDFromCertURL(info.mCertURL, systemID); !err.IsNone()) {
        LOG_ERR() << "Failed to update System ID from online certificate" << Log::Field(err);

        return;
    }

    LockGuard lock {mMutex};

    mSystemID = systemID;

    LOG_INF() << "System ID updated from online certificate" << Log::Field("systemID", mSystemID);
}

Error IdentHandler::GetSystemIDFromOnlineCert(String& systemID)
{
    auto certInfo = MakeUnique<CertInfo>(mAllocator);
    if (!certInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mCertProvider->GetCertKey(cOnlineCert, {}, {}, *certInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return GetSystemIDFromCertURL(certInfo->mCertURL, systemID);
}

Error IdentHandler::GetSystemIDFromCertURL(const String& certURL, String& systemID)
{
    auto [chain, err] = mCertLoader->LoadCertsChainByURL(certURL);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!chain || chain->IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "online certificate chain is empty"));
    }

    StaticString<cIDLen> foundSystemID;
    Error                extractErr = ErrorEnum::eNotFound;

    for (const auto& uri : (*chain)[0].mSubjectURLs) {
        extractErr = crypto::GetSystemIDFromCert(uri, foundSystemID);
        if (extractErr.IsNone()) {
            break;
        }
    }

    if (!extractErr.IsNone()) {
        return extractErr;
    }

    if (auto assignErr = systemID.Assign(foundSystemID); !assignErr.IsNone()) {
        return AOS_ERROR_WRAP(assignErr);
    }

    LOG_DBG() << "System ID found in certificate" << Log::Field("systemID", systemID);

    return ErrorEnum::eNone;
}

} // namespace aos::iam::identhandler
