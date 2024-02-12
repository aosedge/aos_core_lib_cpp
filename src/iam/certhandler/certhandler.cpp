/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/certhandler.hpp"
#include "log.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

CertHandler::CertHandler()
{
}

Error CertHandler::RegisterModule(CertModule& module)
{
    LockGuard lock(mMutex);

    LOG_INF() << "Register module: type = " << module.GetCertType();

    return AOS_ERROR_WRAP(mModules.PushBack(&module));
}

Error CertHandler::GetCertTypes(Array<StaticString<cCertTypeLen>>& certTypes)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get all registered IAM certificate types";

    for (const auto module : mModules) {
        auto err = certTypes.PushBack(module->GetCertType());
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err.Value());
        }
    }

    return ErrorEnum::eNone;
}

Error CertHandler::SetOwner(const String& certType, const String& password)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Set owner: type = " << certType;

    auto* module = FindModule(certType);
    if (module == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto err = module->SetOwner(password);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::Clear(const String& certType)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Clear all certificates: type = " << certType;

    auto* module = FindModule(certType);
    if (module == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto err = module->Clear();
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::CreateKey(
    const String& certType, const String& subjectCommonName, const String& password, String& pemCSR)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Create key: type = " << certType << ", subject = " << subjectCommonName;

    auto* module = FindModule(certType);
    if (module == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto key = module->CreateKey(password);
    if (!key.mError.IsNone()) {
        return key.mError;
    }

    auto err = module->CreateCSR(subjectCommonName, *key.mValue, pemCSR);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::ApplyCertificate(const String& certType, const String& cert, CertInfo& info)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Apply cert: type = " << certType;

    auto* module = FindModule(certType);
    if (module == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto err = module->ApplyCert(cert, info);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::GetCertificate(
    const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& resCert)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get certificate: type = " << certType << ", serial = " << serial;

    auto* module = FindModule(certType);
    if (module == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto err = module->GetCertificate(issuer, serial, resCert);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CertHandler::CreateSelfSignedCert(const String& certType, const String& password)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Create self signed cert: type = " << certType;

    auto* module = FindModule(certType);
    if (module == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto err = module->CreateSelfSignedCert(password);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

CertHandler::~CertHandler()
{
    LOG_DBG() << "Close certificate handler";
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

CertModule* CertHandler::FindModule(const String& certType) const
{
    auto module = mModules.Find([certType](CertModule* module) { return module->GetCertType() == certType; });

    return !module.mError.IsNone() ? nullptr : *module.mValue;
}

} // namespace certhandler
} // namespace iam
} // namespace aos
