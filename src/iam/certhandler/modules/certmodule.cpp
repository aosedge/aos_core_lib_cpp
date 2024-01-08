/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/modules/certmodule.hpp"
#include "../log.hpp"
#include "aos/common/tools/memory.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

CertModule::CertModule(const String& certType, const ModuleConfig& config)
    : mCertType(certType)
    , mModuleConfig(config)
{
}

Error CertModule::Init(crypto::x509::ProviderItf& x509Provider, HSMItf& hsm, StorageItf& storage)
{
    mX509Provider = &x509Provider;
    mHSM          = &hsm;
    mStorage      = &storage;

    if (mModuleConfig.mSkipValidation) {
        LOG_WRN() << "Skip validation: type = " << GetCertType();

        return ErrorEnum::eNone;
    }

    auto validCerts = MakeUnique<ModuleCertificates>(&mAllocator);

    auto err = mHSM->ValidateCertificates(mInvalidCerts, mInvalidKeys, *validCerts);
    if (!err.IsNone()) {
        return err;
    }

    return SyncValidCerts(*validCerts);
}

Error CertModule::GetCertificate(const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert)
{
    auto certsInStorage = MakeUnique<ModuleCertificates>(&mAllocator);

    if (serial.IsEmpty()) {
        const auto err = mStorage->GetCertsInfo(GetCertType(), *certsInStorage);
        if (!err.IsNone()) {
            return err;
        }

        if (certsInStorage->Size() == 0) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        resCert = {};
        for (const auto& item : *certsInStorage) {
            if (resCert.mNotAfter.IsZero() || item.mNotAfter < resCert.mNotAfter) {
                resCert = item;
            }
        }

        return ErrorEnum::eNone;
    }

    return mStorage->GetCertInfo(issuer, serial, resCert);
}

Error CertModule::SetOwner(const String& password)
{
    return mHSM->SetOwner(password);
}

Error CertModule::Clear()
{
    auto err = mHSM->Clear();
    if (!err.IsNone()) {
        return err;
    }

    return mStorage->RemoveAllCertsInfo(GetCertType());
}

RetWithError<SharedPtr<crypto::PrivateKeyItf>> CertModule::CreateKey(const String& password)
{
    auto err = RemoveInvalidCerts(password);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    err = RemoveInvalidKeys(password);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    return mHSM->CreateKey(password, mModuleConfig.mKeyGenAlgorithm);
}

Error CertModule::CreateCSR(
    const String& subjectCommonName, const crypto::PrivateKeyItf& privKey, Array<uint8_t>& pemCSR)
{
    crypto::x509::CSR templ;

    templ.mDNSNames = mModuleConfig.mAlternativeNames;

    auto err = mX509Provider->ASN1EncodeDN(subjectCommonName, templ.mSubject);
    if (!err.IsNone()) {
        return err;
    }

    StaticArray<crypto::asn1::ObjectIdentifier, crypto::cCertExtraExtCount> oids;

    for (const auto& extKeyUsage : mModuleConfig.mExtendedKeyUsage) {
        switch (extKeyUsage.GetValue()) {
        case ExtendedKeyUsageEnum::eClientAuth:
            oids.PushBack(cOidExtKeyUsageClientAuth);
            break;

        case ExtendedKeyUsageEnum::eServerAuth:
            oids.PushBack(cOidExtKeyUsageServerAuth);
            break;

        default:
            LOG_WRN() << "Unexpected extended key usage: type = " << GetCertType()
                      << ", value = " << extKeyUsage.ToString();
            break;
        }
    }

    if (!oids.IsEmpty()) {
        crypto::asn1::Extension ext;

        ext.mId = cOidExtensionExtendedKeyUsage;

        auto err = mX509Provider->ASN1EncodeObjectIds(oids, ext.mValue);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = templ.mExtraExtensions.PushBack(ext);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return mX509Provider->CreateCSR(templ, privKey, pemCSR);
}

Error CertModule::ApplyCert(const Array<uint8_t>& pemCert, CertInfo& info)
{
    auto certificates = MakeUnique<crypto::x509::CertificateChain>(&mAllocator);

    auto err = mX509Provider->PEMToX509Certs(pemCert, *certificates);
    if (!err.IsNone()) {
        return err;
    }

    err = CheckCertChain(*certificates);
    if (!err.IsNone()) {
        return err;
    }

    StaticString<cPasswordLen> password;

    err = mHSM->ApplyCert(*certificates, info, password);
    if (!err.IsNone()) {
        return err;
    }

    err = mStorage->AddCertInfo(GetCertType(), info);
    if (!err.IsNone()) {
        return err;
    }

    return TrimCerts(password);
}

Error CertModule::CreateSelfSignedCert(const String& password)
{
    auto key = CreateKey(password);
    if (!key.mError.IsNone()) {
        return key.mError;
    }

    const uint64_t serial = Time::Now().UnixNano();
    auto           templ  = MakeUnique<crypto::x509::Certificate>(&mAllocator);

    templ->mSerial    = Array<uint8_t>(reinterpret_cast<const uint8_t*>(&serial), sizeof(serial));
    templ->mNotBefore = Time::Now();
    templ->mNotAfter  = Time::Now().Add(cValidSelfSignedCertPeriod);

    auto err = mX509Provider->ASN1EncodeDN("Aos Core", templ->mSubject);
    if (!err.IsNone()) {
        return err;
    }

    err = mX509Provider->ASN1EncodeDN("Aos Core", templ->mIssuer);
    if (!err.IsNone()) {
        return err;
    }

    auto pemCert = MakeUnique<SelfSignedCertificate>(&mAllocator);

    err = mX509Provider->CreateCertificate(*templ, *templ, *key.mValue, *pemCert);
    if (!err.IsNone()) {
        return err;
    }

    CertInfo certInfo;

    return ApplyCert(*pemCert, certInfo);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error CertModule::RemoveInvalidCerts(const String& password)
{
    for (const auto& url : mInvalidCerts) {
        LOG_WRN() << "Remove invalid cert: type = " << GetCertType() << ", url = " << url;

        const auto err = mHSM->RemoveCert(url, password);
        if (!err.IsNone()) {
            return err;
        }
    }

    mInvalidCerts.Clear();

    return ErrorEnum::eNone;
}

Error CertModule::RemoveInvalidKeys(const String& password)
{
    for (const auto& url : mInvalidKeys) {
        LOG_WRN() << "Remove invalid key: type = " << GetCertType() << ", url = " << url;

        const auto err = mHSM->RemoveKey(url, password);
        if (!err.IsNone()) {
            return err;
        }
    }

    mInvalidKeys.Clear();

    return ErrorEnum::eNone;
}

Error CertModule::TrimCerts(const String& password)
{
    auto certsInStorage = MakeUnique<ModuleCertificates>(&mAllocator);

    auto err = mStorage->GetCertsInfo(GetCertType(), *certsInStorage);
    if (!err.IsNone()) {
        return err;
    }

    if (certsInStorage->Size() > mModuleConfig.mMaxCertificates) {
        LOG_WRN() << "Current cert count exceeds max count: " << certsInStorage->Size() << " > "
                  << mModuleConfig.mMaxCertificates << ". Remove old certificates";
    }

    while (certsInStorage->Size() > mModuleConfig.mMaxCertificates) {
        Time      minTime;
        CertInfo* info = nullptr;

        for (auto& cert : *certsInStorage) {
            if (minTime.IsZero() || cert.mNotAfter < minTime) {
                minTime = cert.mNotAfter;
                info    = &cert;
            }
        }

        err = mHSM->RemoveCert(info->mCertURL, password);
        if (!err.IsNone()) {
            return err;
        }

        err = mHSM->RemoveKey(info->mKeyURL, password);
        if (!err.IsNone()) {
            return err;
        }

        err = mStorage->RemoveCertInfo(GetCertType(), info->mCertURL);
        if (!err.IsNone()) {
            return err;
        }

        certsInStorage->Remove(info);
    }

    return ErrorEnum::eNone;
}

Error CertModule::CheckCertChain(const Array<crypto::x509::Certificate>& chain)
{
    if (chain.IsEmpty()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    for (const auto& cert : chain) {
        StaticString<cDNStringLen> issuer, subject;

        mX509Provider->ASN1DecodeDN(cert.mIssuer, issuer);
        mX509Provider->ASN1DecodeDN(cert.mSubject, subject);

        LOG_DBG() << "Check certificate chain: issuer = " << issuer << ", subject = " << subject;
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
                || chain[currentCert].mAuthorityKeyId == chain[i].mSubjectKeyId) {
                parentCert  = i;
                parentFound = true;

                break;
            }
        }

        if (!parentFound) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        currentCert = parentCert;
    }

    return ErrorEnum::eNone;
}

Error CertModule::SyncValidCerts(const Array<CertInfo>& validCerts)
{
    auto certsInStorage = MakeUnique<ModuleCertificates>(&mAllocator);

    auto err = mStorage->GetCertsInfo(GetCertType(), *certsInStorage);
    if (!err.IsNone() && err != ErrorEnum::eNotFound) {
        return err;
    }

    // Add module certificates to storage.
    for (const auto& moduleCert : validCerts) {
        CertInfo* storedCert = nullptr;

        for (auto& cert : *certsInStorage) {
            if (cert == moduleCert) {
                storedCert = &cert;

                break;
            }
        }

        if (storedCert != nullptr) {
            certsInStorage->Remove(storedCert);
        } else {
            LOG_WRN() << "Add missing cert to DB: type = " << GetCertType() << ", certInfo = " << moduleCert;

            err = mStorage->AddCertInfo(GetCertType(), moduleCert);
            if (!err.IsNone()) {
                return err;
            }
        }
    }

    // Remove outdated certificates from storage.
    for (const auto& moduleCert : *certsInStorage) {
        LOG_WRN() << "Remove invalid cert from DB: type = " << GetCertType() << ", certInfo = " << moduleCert;

        err = mStorage->RemoveCertInfo(GetCertType(), moduleCert.mCertURL);
        if (!err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

} // namespace certhandler
} // namespace iam
} // namespace aos
