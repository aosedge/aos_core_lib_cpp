/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/crypto/cryptoutils.hpp>
#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

#include "certmodule.hpp"

namespace aos::iam::certhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error CertModule::Init(AllocatorItf& allocator, const String& certType, const ModuleConfig& config,
    crypto::x509::ProviderItf& x509Provider, HSMItf& hsm, StorageItf& storage)
{
    mAllocator    = &allocator;
    mCertType     = certType;
    mModuleConfig = config;
    mX509Provider = &x509Provider;
    mHSM          = &hsm;
    mStorage      = &storage;

    if (auto err = ValidateConfig(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mModuleConfig.mSkipValidation) {
        LOG_WRN() << "Skip validation: type=" << GetCertType();

        return ErrorEnum::eNone;
    }

    auto validCerts = MakeUnique<ModuleCertificates>(mAllocator);
    if (!validCerts) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (mModuleConfig.mCertType == CertModuleTypeEnum::eRootCerts) {
        if (auto err = mHSM->ValidateRootCertificates(*validCerts); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        if (auto err = mHSM->ValidateCertificates(mInvalidCerts, mInvalidKeys, *validCerts); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return SyncValidCerts(*validCerts);
}

Error CertModule::GetCertificate(const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert)
{
    auto certsInStorage = MakeUnique<ModuleCertificates>(mAllocator);
    if (!certsInStorage) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (serial.IsEmpty()) {
        if (auto err = mStorage->GetCertsInfo(GetCertType(), *certsInStorage); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (certsInStorage->IsEmpty()) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        auto clearCertInfo = MakeShared<CertInfo>(mAllocator);
        if (!clearCertInfo) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        resCert = *clearCertInfo;

        for (const auto& item : *certsInStorage) {
            if (resCert.mNotAfter.IsZero() || resCert.mNotAfter < item.mNotAfter) {
                resCert = item;
            }
        }

        return ErrorEnum::eNone;
    }

    if (auto err = mStorage->GetCertInfo(issuer, serial, resCert); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertModule::GetCertificates(Array<CertInfo>& infos)
{
    if (auto err = mStorage->GetCertsInfo(GetCertType(), infos); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertModule::SetOwner(const String& password)
{
    if (auto err = mHSM->SetOwner(password); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertModule::Clear()
{
    auto err = mHSM->Clear();
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mStorage->RemoveAllCertsInfo(GetCertType());
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<SharedPtr<crypto::PrivateKeyItf>> CertModule::CreateKey(const String& password)
{
    if (mModuleConfig.mCertType == CertModuleTypeEnum::eRootCerts) {
        return {nullptr, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
    }

    auto err = RemoveInvalidCerts(password);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    err = RemoveInvalidKeys(password);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    auto keyResult = mHSM->CreateKey(password, mModuleConfig.mKeyType);

    return {keyResult.mValue, AOS_ERROR_WRAP(keyResult.mError)};
}

Error CertModule::CreateCSR(const String& subjectCommonName, const crypto::PrivateKeyItf& privKey, String& pemCSR)
{
    auto templ = MakeUnique<crypto::x509::CSR>(mAllocator);
    if (!templ) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    StaticString<cDNStringLen> subject;

    templ->mDNSNames = mModuleConfig.mAlternativeNames;

    auto err = subject.Format("CN=%s", subjectCommonName.CStr());
    if (!err.IsNone()) {
        return err;
    }

    err = mX509Provider->ASN1EncodeDN(subject, templ->mSubject);
    if (!err.IsNone()) {
        return err;
    }

    StaticArray<crypto::asn1::ObjectIdentifier, crypto::cCertExtraExtCount> oids;

    for (const auto& extKeyUsage : mModuleConfig.mExtendedKeyUsage) {
        switch (extKeyUsage.GetValue()) {
        case ExtendedKeyUsageEnum::eClientAuth:
            (void)oids.PushBack(cOidExtKeyUsageClientAuth);
            break;

        case ExtendedKeyUsageEnum::eServerAuth:
            (void)oids.PushBack(cOidExtKeyUsageServerAuth);
            break;

        default:
            LOG_WRN() << "Unexpected extended key usage: type=" << GetCertType()
                      << ", value=" << extKeyUsage.ToString();
            break;
        }
    }

    if (!oids.IsEmpty()) {
        crypto::asn1::Extension ext;

        ext.mID = cOidExtensionExtendedKeyUsage;

        err = mX509Provider->ASN1EncodeObjectIds(oids, ext.mValue);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = templ->mExtraExtensions.PushBack(ext);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    err = mX509Provider->CreateCSR(*templ, privKey, pemCSR);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertModule::ApplyCert(const String& pemCert, CertInfo& info)
{
    if (mModuleConfig.mCertType == CertModuleTypeEnum::eRootCerts) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotSupported);
    }

    auto certificates = MakeUnique<crypto::x509::CertificateChain>(mAllocator);
    if (!certificates) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto err = mX509Provider->PEMToX509Certs(pemCert, *certificates);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = CheckCertChain(*certificates);
    if (!err.IsNone()) {
        return err;
    }

    StaticString<cPasswordLen> password;

    err = TrimCerts(password);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mHSM->ApplyCert(*certificates, info, password);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mStorage->AddCertInfo(GetCertType(), info);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertModule::UpdateCerts(
    const Array<StaticString<crypto::cCertPEMLen>>& pemCerts, const String& password, Array<CertInfo>& infos)
{
    if (pemCerts.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "empty certificate set is not allowed"));
    }

    auto existing = MakeUnique<ModuleCertificates>(mAllocator);
    if (!existing) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetCertsInfo(GetCertType(), *existing); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    auto certs = MakeUnique<StaticArray<crypto::x509::Certificate, cCertsPerModule>>(mAllocator);
    if (!certs) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    size_t newCertCount = 0;

    if (auto err = CollectUpdateCerts(pemCerts, *existing, *certs, newCertCount); !err.IsNone()) {
        return err;
    }

    if (certs->Size() > mModuleConfig.mMaxCertificates) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    // Temporary peak while old certs are still present (add first, then remove).
    if (existing->Size() + newCertCount > cCertsPerModule) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    infos.Clear();

    // Rollback newly added certs on failure (reused ones are left intact).
    auto rollbackAdded = DeferRelease(&infos, [this, &password, &existing](const Array<CertInfo>* added) {
        for (const auto& info : *added) {
            if (!HasCert(*existing, info.mIssuer, info.mSerial)) {
                (void)RemoveCert(info, password);
            }
        }
    });

    for (const auto& cert : *certs) {
        auto addedInfo = MakeUnique<CertInfo>(mAllocator);
        if (!addedInfo) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        if (auto err = AddCert(cert, *existing, infos, password, *addedInfo); !err.IsNone()) {
            return err;
        }

        if (auto err = infos.PushBack(*addedInfo); !err.IsNone()) {
            (void)RemoveCert(*addedInfo, password);

            return AOS_ERROR_WRAP(err);
        }
    }

    (void)rollbackAdded.Release();

    // Remove certs that are no longer in the new set.
    for (const auto& old : *existing) {
        if (HasCert(infos, old.mIssuer, old.mSerial)) {
            continue;
        }

        if (auto err = RemoveCert(old, password); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error CertModule::CollectUpdateCerts(const Array<StaticString<crypto::cCertPEMLen>>& pemCerts,
    const Array<CertInfo>& existing, Array<crypto::x509::Certificate>& certs, size_t& newCertCount)
{
    newCertCount = 0;

    for (const auto& pemCert : pemCerts) {
        auto certificates = MakeUnique<crypto::x509::CertificateChain>(mAllocator);
        if (!certificates) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        if (auto err = mX509Provider->PEMToX509Certs(pemCert, *certificates); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (certificates->Size() != 1) {
            return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
        }

        if (!HasCert(existing, (*certificates)[0].mIssuer, (*certificates)[0].mSerial)) {
            ++newCertCount;
        }

        if (auto err = certs.PushBack((*certificates)[0]); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error CertModule::AddCert(const crypto::x509::Certificate& cert, const Array<CertInfo>& curCerts,
    const Array<CertInfo>& newCerts, const String& password, CertInfo& resInfo)
{
    for (const auto& info : newCerts) {
        if (info.mIssuer == cert.mIssuer && info.mSerial == cert.mSerial) {
            LOG_WRN() << "Cert with the same issuer and serial already exists in update set"
                      << Log::Field("type", GetCertType());

            resInfo = info;

            return ErrorEnum::eNone;
        }
    }

    for (const auto& info : curCerts) {
        if (info.mIssuer == cert.mIssuer && info.mSerial == cert.mSerial) {
            resInfo = info;

            return ErrorEnum::eNone;
        }
    }

    auto addedInfo = MakeUnique<CertInfo>(mAllocator);
    if (!addedInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mHSM->AddCert(cert, password, *addedInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorage->AddCertInfo(GetCertType(), *addedInfo); !err.IsNone()) {
        (void)mHSM->RemoveCert(addedInfo->mCertURL, password);

        return AOS_ERROR_WRAP(err);
    }

    resInfo = *addedInfo;

    return ErrorEnum::eNone;
}

Error CertModule::RemoveCert(const CertInfo& info, const String& password)
{
    auto err = mHSM->RemoveCert(info.mCertURL, password);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mStorage->RemoveCertInfo(GetCertType(), info.mCertURL);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool CertModule::HasCert(const Array<CertInfo>& infos, const Array<uint8_t>& issuer, const Array<uint8_t>& serial)
{
    for (const auto& info : infos) {
        if (info.mIssuer == issuer && info.mSerial == serial) {
            return true;
        }
    }

    return false;
}

Error CertModule::CreateSelfSignedCert(const String& password)
{
    auto key = CreateKey(password);
    if (!key.mError.IsNone()) {
        return key.mError;
    }

    const uint64_t serial = Time::Now().UnixNano();

    auto templ = MakeUnique<crypto::x509::Certificate>(mAllocator);
    if (!templ) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    templ->mSerial    = Array<uint8_t>(reinterpret_cast<const uint8_t*>(&serial), sizeof(serial));
    templ->mNotBefore = Time::Now();
    templ->mNotAfter  = Time::Now().Add(cValidSelfSignedCertPeriod);

    auto err = mX509Provider->ASN1EncodeDN("CN=Aos Core", templ->mSubject);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mX509Provider->ASN1EncodeDN("CN=Aos Core", templ->mIssuer);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto pemCert = MakeUnique<SelfSignedCertificate>(mAllocator);
    if (!pemCert) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    err = mX509Provider->CreateCertificate(*templ, *templ, *key.mValue, *pemCert);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto certInfo = MakeUnique<CertInfo>(mAllocator);
    if (!certInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    return ApplyCert(*pemCert, *certInfo);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error CertModule::ValidateConfig() const
{
    if (mModuleConfig.mMaxCertificates == 0) {
        LOG_ERR() << "Max certificates module config must be greater than 0: type=" << GetCertType();

        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    if (mModuleConfig.mCertType == CertModuleTypeEnum::eCertKeyPair && mModuleConfig.mMaxCertificates < 2) {
        LOG_ERR() << "Max certificates module config must be set to at least 2 for cert/key pair modules: type="
                  << GetCertType() << ", value=" << static_cast<int32_t>(mModuleConfig.mMaxCertificates);

        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    if (mModuleConfig.mMaxCertificates > cCertsPerModule) {
        LOG_ERR() << "Max certificates module config exceeds application limit: type=" << GetCertType()
                  << ", value=" << static_cast<int32_t>(mModuleConfig.mMaxCertificates)
                  << ", limit=" << cCertsPerModule;

        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    return ErrorEnum::eNone;
}

Error CertModule::RemoveInvalidCerts(const String& password)
{
    for (const auto& url : mInvalidCerts) {
        LOG_DBG() << "Remove invalid cert: type=" << GetCertType() << ", url=" << url;

        const auto err = mHSM->RemoveCert(url, password);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    mInvalidCerts.Clear();

    return ErrorEnum::eNone;
}

Error CertModule::RemoveInvalidKeys(const String& password)
{
    for (const auto& url : mInvalidKeys) {
        LOG_DBG() << "Remove invalid key: type=" << GetCertType() << ", url=" << url;

        const auto err = mHSM->RemoveKey(url, password);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    mInvalidKeys.Clear();

    return ErrorEnum::eNone;
}

Error CertModule::TrimCerts(const String& password)
{
    auto certsInStorage = MakeUnique<ModuleCertificates>(mAllocator);
    if (!certsInStorage) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto err = mStorage->GetCertsInfo(GetCertType(), *certsInStorage);
    if (!err.IsNone() && err != ErrorEnum::eNotFound) {
        return AOS_ERROR_WRAP(err);
    }

    while (certsInStorage->Size() + 1 > mModuleConfig.mMaxCertificates) {
        Time            minTime;
        const CertInfo* info = nullptr;

        for (const auto& cert : *certsInStorage) {
            if (minTime.IsZero() || cert.mNotAfter < minTime) {
                minTime = cert.mNotAfter;
                info    = &cert;
            }
        }

        assert(info != nullptr);

        LOG_DBG() << "Trim certificate to allocate space for a new one: type=" << GetCertType()
                  << ", count=" << static_cast<int32_t>(certsInStorage->Size())
                  << ", max=" << static_cast<int32_t>(mModuleConfig.mMaxCertificates);

        err = mHSM->RemoveCert(info->mCertURL, password);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = mHSM->RemoveKey(info->mKeyURL, password);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = mStorage->RemoveCertInfo(GetCertType(), info->mCertURL);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        (void)certsInStorage->Erase(info);
    }

    return ErrorEnum::eNone;
}

Error CertModule::CheckCertChain(const Array<crypto::x509::Certificate>& chain)
{
    if (chain.IsEmpty()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    for (const auto& cert : chain) {
        StaticString<cDNStringLen> issuer;
        StaticString<cDNStringLen> subject;

        (void)mX509Provider->ASN1DecodeDN(cert.mIssuer, issuer);
        (void)mX509Provider->ASN1DecodeDN(cert.mSubject, subject);

        LOG_DBG() << "Check certificate chain: issuer=" << issuer << ", subject=" << subject;
    }

    size_t currentCert = 0;

    while (!(chain[currentCert].mIssuer.IsEmpty() || chain[currentCert].mIssuer == chain[currentCert].mSubject)) {
        size_t parentCert  = 0;
        bool   parentFound = false;

        for (size_t i = 0; i < chain.Size(); i++) {
            if (i == currentCert) {
                continue;
            }

            if (chain[currentCert].mIssuer == chain[i].mSubject
                || (!chain[currentCert].mAuthorityKeyId.IsEmpty()
                    && chain[currentCert].mAuthorityKeyId == chain[i].mSubjectKeyId)) {
                parentCert  = i;
                parentFound = true;

                break;
            }
        }

        if (!parentFound) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        if (auto err = crypto::ValidateCACert(chain[parentCert]); !err.IsNone()) {
            return err;
        }

        currentCert = parentCert;
    }

    return ErrorEnum::eNone;
}

Error CertModule::SyncValidCerts(const Array<CertInfo>& validCerts)
{
    auto certsInStorage = MakeUnique<ModuleCertificates>(mAllocator);
    if (!certsInStorage) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto err = mStorage->GetCertsInfo(GetCertType(), *certsInStorage);
    if (!err.IsNone() && err != ErrorEnum::eNotFound) {
        return AOS_ERROR_WRAP(err);
    }

    // Add module certificates to storage.
    for (const auto& moduleCert : validCerts) {
        const CertInfo* storedCert = nullptr;

        for (const auto& cert : *certsInStorage) {
            if (cert == moduleCert) {
                storedCert = &cert;

                break;
            }
        }

        if (storedCert != nullptr) {
            (void)certsInStorage->Erase(storedCert);
        } else {
            LOG_WRN() << "Add missing cert to DB: type=" << GetCertType() << ", certInfo=" << moduleCert;

            err = mStorage->AddCertInfo(GetCertType(), moduleCert);
            if (!err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    // Remove outdated certificates from storage.
    for (const auto& moduleCert : *certsInStorage) {
        LOG_WRN() << "Remove invalid cert from DB: type=" << GetCertType() << ", certInfo=" << moduleCert;

        err = mStorage->RemoveCertInfo(GetCertType(), moduleCert.mCertURL);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::iam::certhandler
