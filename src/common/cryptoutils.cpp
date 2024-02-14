/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/cryptoutils.hpp"
#include "aos/common/tools/fs.hpp"

namespace aos {
namespace cryptoutils {

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

constexpr auto cSchemeFile      = "file";
constexpr auto cSchemePKCS11    = "pkcs11";
constexpr auto cSchemeMaxLength = Max(sizeof(cSchemeFile), sizeof(cSchemePKCS11));

/***********************************************************************************************************************
 * CertLoader
 **********************************************************************************************************************/

Error CertLoader::Init(crypto::x509::ProviderItf& cryptoProvider, pkcs11::PKCS11Manager& pkcs11Manager)
{
    mCryptoProvider = &cryptoProvider;
    mPKCS11         = &pkcs11Manager;

    return ErrorEnum::eNone;
}

RetWithError<SharedPtr<crypto::x509::CertificateChain>> CertLoader::LoadCertsChainByURL(const String& url)
{
    StaticString<cSchemeMaxLength> scheme;

    auto err = ParseURLScheme(url, scheme);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    if (scheme == cSchemeFile) {
        StaticString<cFilePathLen> path;

        err = ParseFileURL(url, path);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        return LoadCertsFromFile(path);
    } else if (scheme == cSchemePKCS11) {
        StaticString<cFilePathLen>       library;
        StaticString<pkcs11::cLabelLen>  token;
        StaticString<pkcs11::cLabelLen>  label;
        uuid::UUID                       id;
        StaticString<pkcs11::cPINLength> userPIN;

        err = ParsePKCS11URL(url, library, token, label, id, userPIN);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = OpenSession(library, token, userPIN);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        return pkcs11::Utils(session, *mCryptoProvider, mAllocator).FindCertificateChain(id, label);
    }

    return {nullptr, ErrorEnum::eInvalidArgument};
}

RetWithError<SharedPtr<crypto::PrivateKeyItf>> CertLoader::LoadPrivKeyByURL(const String& url)
{
    StaticString<cSchemeMaxLength> scheme;

    auto err = ParseURLScheme(url, scheme);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    if (scheme == cSchemeFile) {
        StaticString<cFilePathLen> path;

        err = ParseFileURL(url, path);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        return LoadPrivKeyFromFile(path);
    } else if (scheme == cSchemePKCS11) {
        StaticString<cFilePathLen>       library;
        StaticString<pkcs11::cLabelLen>  token;
        StaticString<pkcs11::cLabelLen>  label;
        uuid::UUID                       id;
        StaticString<pkcs11::cPINLength> userPIN;

        err = ParsePKCS11URL(url, library, token, label, id, userPIN);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = OpenSession(library, token, userPIN);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        auto key = pkcs11::Utils(session, *mCryptoProvider, mAllocator).FindPrivateKey(id, label);

        return {key.mValue.GetPrivKey(), key.mError};
    }

    return {nullptr, ErrorEnum::eInvalidArgument};
}

RetWithError<SharedPtr<pkcs11::SessionContext>> CertLoader::OpenSession(
    const String& libraryPath, const String& token, const String& userPIN)
{
    const char* correctedPath = libraryPath.IsEmpty() ? cDefaultPKCS11Library : libraryPath.CStr();

    auto library = mPKCS11->OpenLibrary(correctedPath);
    if (!library) {
        return {nullptr, ErrorEnum::eFailed};
    }

    pkcs11::SlotID                    slotID;
    Error                             err = ErrorEnum::eNone;
    SharedPtr<pkcs11::SessionContext> session;

    Tie(slotID, err) = FindToken(*library, token);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    Tie(session, err) = library->OpenSession(slotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    if (!userPIN.IsEmpty()) {
        err = session->Login(CKU_USER, userPIN);
        if (!err.IsNone() && !err.Is(ErrorEnum::eAlreadyLoggedIn)) {
            return {nullptr, err};
        }
    }

    return {Move(session), ErrorEnum::eNone};
}

RetWithError<pkcs11::SlotID> CertLoader::FindToken(pkcs11::LibraryContext& library, const String& token)
{
    StaticArray<pkcs11::SlotID, pkcs11::cSlotListSize> slotList;
    auto                                               tokenInfo = MakeUnique<pkcs11::TokenInfo>(&mAllocator);

    auto err = library.GetSlotList(true, slotList);
    if (!err.IsNone()) {
        return {0, err};
    }

    for (const auto slotID : slotList) {
        auto err = library.GetTokenInfo(slotID, *tokenInfo);
        if (!err.IsNone()) {
            return {0, err};
        }

        if (tokenInfo->mLabel == token) {
            return {slotID, ErrorEnum::eNone};
        }
    }

    return {0, ErrorEnum::eNotFound};
}

RetWithError<SharedPtr<crypto::x509::CertificateChain>> CertLoader::LoadCertsFromFile(const String& fileName)
{
    auto buff = MakeUnique<PEMCertChainBlob>(&mAllocator);

    auto err = FS::ReadFileToString(fileName, *buff);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    auto certificates = MakeShared<crypto::x509::CertificateChain>(&mAllocator);

    err = mCryptoProvider->PEMToX509Certs(*buff, *certificates);

    return {certificates, err};
}

RetWithError<SharedPtr<crypto::PrivateKeyItf>> CertLoader::LoadPrivKeyFromFile(const String& fileName)
{
    auto buff = MakeUnique<StaticString<crypto::cPrivKeyPEMLen>>(&mAllocator);

    auto err = FS::ReadFileToString(fileName, *buff);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    return mCryptoProvider->PEMToX509PrivKey(*buff);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error FindUrlParam(const String& url, const String& paramName, String& paramValue)
{
    Error  err   = ErrorEnum::eNone;
    size_t start = 0, end = 0;

    Tie(start, err) = url.FindSubstr(0, paramName);

    if (!err.IsNone()) {
        return ErrorEnum::eNone;
    }

    start += paramName.Size() + 1; // skip paramName=

    Tie(end, err) = url.FindAny(start, ";&?");

    paramValue.Clear();

    return paramValue.Insert(paramValue.end(), url.Get() + start, url.Get() + end);
}

/***********************************************************************************************************************
 * ParseURLScheme
 **********************************************************************************************************************/

Error ParseURLScheme(const String& url, String& scheme)
{
    Error  err = ErrorEnum::eNone;
    size_t pos = 0;

    Tie(pos, err) = url.FindSubstr(0, ":");

    if (!err.IsNone()) {
        return err;
    }

    scheme.Clear();

    return scheme.Insert(scheme.end(), url.CStr(), url.CStr() + pos);
}

/***********************************************************************************************************************
 * ParseFileURL
 **********************************************************************************************************************/

Error ParseFileURL(const String& url, String& path)
{
    StaticString<cSchemeMaxLength> scheme;

    auto err = ParseURLScheme(url, scheme);
    if (!err.IsNone() || scheme != cSchemeFile) {
        return ErrorEnum::eFailed;
    }

    path.Clear();

    return path.Insert(path.begin(), url.begin() + scheme.Size() + String(":").Size(), url.end());
}

/***********************************************************************************************************************
 * ParsePKCS11URL
 **********************************************************************************************************************/

Error ParsePKCS11URL(
    const String& url, String& library, String& token, String& label, Array<uint8_t>& id, String& userPin)
{
    StaticString<cSchemeMaxLength> scheme;

    auto err = ParseURLScheme(url, scheme);
    if (!err.IsNone() || scheme != cSchemePKCS11) {
        return ErrorEnum::eFailed;
    }

    err = FindUrlParam(url, "module-path", library);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    err = FindUrlParam(url, "token", token);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    err = FindUrlParam(url, "object", label);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<uuid::cUUIDStrLen> uuid;

    err = FindUrlParam(url, "id", uuid);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Tie(id, err) = uuid::StringToUUID(uuid);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = FindUrlParam(url, "pin-value", userPin);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace cryptoutils
} // namespace aos
