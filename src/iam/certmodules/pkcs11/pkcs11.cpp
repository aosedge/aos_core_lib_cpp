/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/cryptoutils.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/os.hpp"
#include "aos/common/uuid.hpp"

#include "aos/iam/certmodules/pkcs11/pkcs11.hpp"

#include "../../certhandler/log.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/**
 * A helper object for search operations containing the most valuable data for certhandler.
 */
struct PKCS11Module::SearchObject {
    /**
     * Object type.
     */
    Optional<pkcs11::ObjectClass> mType;
    /**
     * Object handle.
     */
    pkcs11::ObjectHandle mHandle;
    /**
     * Object label.
     */
    StaticString<pkcs11::cLabelLen> mLabel;
    /**
     * Key identifier for public/private key pair.
     */
    uuid::UUID mID;
};

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error PKCS11Module::Init(const String& certType, const PKCS11ModuleConfig& config, pkcs11::PKCS11Manager& pkcs11,
    crypto::x509::ProviderItf& x509Provider)
{
    mCertType     = certType;
    mConfig       = config;
    mX509Provider = &x509Provider;

    mPKCS11 = pkcs11.OpenLibrary(mConfig.mLibrary);
    if (!mPKCS11) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    auto err = os::GetEnv(cEnvLoginType, mTeeLoginType);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (mConfig.mUserPINPath.IsEmpty() && mTeeLoginType.IsEmpty()) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    mConfig.mTokenLabel = GetTokenLabel();

    Tie(mSlotID, err) = GetSlotID();
    if (!err.IsNone()) {
        return err;
    }

    bool isOwned = false;

    Tie(isOwned, err) = IsOwned();
    if (!err.IsNone()) {
        return err;
    }

    if (isOwned) {
        err = PrintInfo(mSlotID);
        if (!err.IsNone()) {
            return err;
        }
    } else {
        LOG_DBG() << "No owned token found";
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::SetOwner(const String& password)
{
    Error err = ErrorEnum::eNone;

    Tie(mSlotID, err) = GetSlotID();
    if (!err.IsNone()) {
        return err;
    }

    mPendingKeys.Clear();
    mSession.Reset();

    if (!mTeeLoginType.IsEmpty()) {
        StaticString<pkcs11::cPINLength> userPIN;

        err = GetTeeUserPIN(mTeeLoginType, userPIN);
        if (!err.IsNone()) {
            return err;
        }
    } else {
        StaticString<pkcs11::cPINLength> userPIN;

        err = GetUserPin(userPIN);
        if (!err.IsNone()) {
            err = pkcs11::GenPIN(userPIN);
            if (!err.IsNone()) {
                return err;
            }

            err = FS::WriteStringToFile(mConfig.mUserPINPath, userPIN, 0600);
            if (!err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    LOG_DBG() << "Init token: slotID = " << mSlotID << ", label = " << mConfig.mTokenLabel;

    err = mPKCS11->InitToken(mSlotID, password, mConfig.mTokenLabel);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    pkcs11::SessionContext* session;

    Tie(session, err) = CreateSession(false, password);
    if (!err.IsNone()) {
        return err;
    }

    if (!mTeeLoginType.IsEmpty()) {
        LOG_DBG() << "Init PIN: pin = " << mUserPIN << ", session = " << session->GetHandle();
    } else {
        LOG_DBG() << "Init PIN: session = " << session->GetHandle();
    }

    err = session->InitPIN(mUserPIN);
    if (err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::Clear()
{
    Error err     = ErrorEnum::eNone;
    bool  isOwned = false;

    Tie(isOwned, err) = IsOwned();
    if (!err.IsNone()) {
        return err;
    }

    if (!isOwned) {
        return ErrorEnum::eNone;
    }

    pkcs11::SessionContext* session;

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticArray<SearchObject, cCertsPerModule * 3> tokens; // certs, privKeys, pubKeys
    SearchObject                                   filter;

    err = FindObject(*session, filter, tokens);
    if (!err.IsNone()) {
        return err;
    }

    for (const auto& token : tokens) {
        auto releaseErr = session->DestroyObject(token.mHandle);
        if (!releaseErr.IsNone()) {
            err = AOS_ERROR_WRAP(releaseErr);
            LOG_ERR() << "Can't delete object: handle = " << token.mHandle;
        }
    }

    return err;
}

RetWithError<SharedPtr<crypto::PrivateKeyItf>> PKCS11Module::CreateKey(const String& password, crypto::KeyType keyType)
{
    (void)password;

    PKCS11Module::PendingKey pendingKey;
    Error                    err = ErrorEnum::eNone;

    Tie(pendingKey.mUUID, err) = uuid::CreateUUID();
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    pkcs11::SessionContext* session;

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    switch (keyType.GetValue()) {
    case aos::crypto::KeyTypeEnum::eRSA:
        Tie(pendingKey.mKey, err) = pkcs11::Utils(*session, *mX509Provider, mLocalCacheAllocator)
                                        .GenerateRSAKeyPairWithLabel(pendingKey.mUUID, mCertType, cRSAKeyLength);
        if (!err.IsNone()) {
            return {nullptr, AOS_ERROR_WRAP(err)};
        }
        break;

    case aos::crypto::KeyTypeEnum::eECDSA:
        Tie(pendingKey.mKey, err) = pkcs11::Utils(*session, *mX509Provider, mLocalCacheAllocator)
                                        .GenerateECDSAKeyPairWithLabel(pendingKey.mUUID, mCertType, cECSDACurveID);
        if (!err.IsNone()) {
            return {nullptr, AOS_ERROR_WRAP(err)};
        }
        break;

    default:
        LOG_ERR() << "Unsupported algorithm";

        return {nullptr, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }

    err = TokenMemInfo();
    if (!err.IsNone()) {
        pkcs11::Utils(*session, *mX509Provider, mLocalCacheAllocator).DeletePrivateKey(pendingKey.mKey);
        return {nullptr, err};
    }

    if (mPendingKeys.Size() == mPendingKeys.MaxSize()) {
        LOG_WRN() << "Max pending keys reached: Remove old: certType = " << mCertType;

        auto oldKey = mPendingKeys.Front().mValue.mKey;

        err = pkcs11::Utils(*session, *mX509Provider, mLocalCacheAllocator).DeletePrivateKey(oldKey);
        if (!err.IsNone()) {
            LOG_ERR() << "Can't delete pending key = " << err.Message();
        }

        mPendingKeys.Remove(mPendingKeys.begin());
    }

    mPendingKeys.PushBack(pendingKey);

    return {pendingKey.mKey.GetPrivKey(), ErrorEnum::eNone};
}

Error PKCS11Module::ApplyCert(const Array<crypto::x509::Certificate>& certChain, CertInfo& certInfo, String& password)
{
    (void)password;

    Error                   err = ErrorEnum::eNone;
    pkcs11::SessionContext* session;

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Optional<PendingKey> curKey;

    for (auto it = mPendingKeys.begin(); it != mPendingKeys.end(); ++it) {
        if (CheckCertificate(certChain[0], *it->mKey.GetPrivKey())) {
            curKey.SetValue(*it);
            mPendingKeys.Remove(it);

            break;
        }
    }

    if (!curKey.HasValue()) {
        LOG_ERR() << "No corresponding key found";
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    err = CreateCertificateChain(*session, curKey.GetValue().mUUID, mCertType, certChain);
    if (!err.IsNone()) {
        return err;
    }

    err = CreateURL(mCertType, curKey.GetValue().mUUID, certInfo.mCertURL);
    if (err.IsNone()) {
        return err;
    }

    certInfo.mKeyURL   = certInfo.mCertURL;
    certInfo.mIssuer   = certChain[0].mIssuer;
    certInfo.mNotAfter = certChain[0].mNotAfter;
    certInfo.mSerial   = certChain[0].mSerial;

    LOG_DBG() << "Certificate applied: cert = " << certInfo;

    return TokenMemInfo();
}

Error PKCS11Module::RemoveCert(const String& certURL, const String& password)
{
    (void)password;

    Error                   err = ErrorEnum::eNone;
    pkcs11::SessionContext* session;

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<pkcs11::cLabelLen> label;
    uuid::UUID                      id;

    err = ParseURL(certURL, label, id);
    if (!err.IsNone()) {
        return err;
    }

    return pkcs11::Utils(*session, *mX509Provider, mLocalCacheAllocator).DeleteCertificate(id, label);
}

Error PKCS11Module::RemoveKey(const String& keyURL, const String& password)
{
    (void)password;

    Error                   err = ErrorEnum::eNone;
    pkcs11::SessionContext* session;

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<pkcs11::cLabelLen> label;
    uuid::UUID                      id;

    err = ParseURL(keyURL, label, id);
    if (!err.IsNone()) {
        return err;
    }

    const auto privKey = pkcs11::Utils(*session, *mX509Provider, mLocalCacheAllocator).FindPrivateKey(id, label);
    if (!privKey.mError.IsNone()) {
        return AOS_ERROR_WRAP(privKey.mError);
    }

    err = pkcs11::Utils(*session, *mX509Provider, mLocalCacheAllocator).DeletePrivateKey(privKey.mValue);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::ValidateCertificates(
    Array<StaticString<cURLLen>>& invalidCerts, Array<StaticString<cURLLen>>& invalidKeys, Array<CertInfo>& validCerts)
{
    Error                   err     = ErrorEnum::eNone;
    bool                    isOwned = false;
    pkcs11::SessionContext* session;

    Tie(isOwned, err) = IsOwned();
    if (!err.IsNone() || !isOwned) {
        return err;
    }

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // search token objects
    StaticArray<SearchObject, cCertsPerModule> certificates;
    StaticArray<SearchObject, cCertsPerModule> privKeys;
    StaticArray<SearchObject, cCertsPerModule> pubKeys;

    SearchObject filter;

    filter.mLabel = mCertType;
    filter.mType  = CKO_CERTIFICATE;

    err = FindObject(*session, filter, certificates);
    if (!err.IsNone()) {
        return err;
    }

    filter.mType = CKO_PRIVATE_KEY;

    err = FindObject(*session, filter, privKeys);
    if (!err.IsNone()) {
        return err;
    }

    filter.mType = CKO_PUBLIC_KEY;

    err = FindObject(*session, filter, pubKeys);
    if (!err.IsNone()) {
        return err;
    }

    // generate valid info
    err = GetValidInfo(*session, certificates, privKeys, pubKeys, validCerts);
    if (!err.IsNone()) {
        return err;
    }

    // create urls for invalid objects
    err = CreateInvalidURLs(certificates, invalidCerts);
    if (!err.IsNone()) {
        return err;
    }

    err = CreateInvalidURLs(privKeys, invalidKeys);
    if (!err.IsNone()) {
        return err;
    }

    return CreateInvalidURLs(pubKeys, invalidKeys);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

String PKCS11Module::GetTokenLabel() const
{
    return mConfig.mTokenLabel.IsEmpty() ? cDefaultTokenLabel : mConfig.mTokenLabel;
}

RetWithError<pkcs11::SlotID> PKCS11Module::GetSlotID()
{
    const int paramCount
        = static_cast<int>(mConfig.mSlotID.HasValue() + mConfig.mSlotIndex.HasValue() + !mConfig.mTokenLabel.IsEmpty());

    if (paramCount > 1) {
        LOG_ERR()
            << "Only one parameter for slot identification should be specified (slotId or slotIndex or tokenLabel)";

        return {0, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }

    if (mConfig.mSlotID.HasValue()) {
        return {mConfig.mSlotID.GetValue(), ErrorEnum::eNone};
    }

    StaticArray<pkcs11::SlotID, pkcs11::cSlotListSize> slotList;

    auto err = mPKCS11->GetSlotList(false, slotList);
    if (!err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (mConfig.mSlotIndex.HasValue()) {
        const auto& slotIndex = mConfig.mSlotIndex.GetValue();
        if (static_cast<size_t>(slotIndex) >= slotList.Size() || slotIndex < 0) {
            LOG_ERR() << "Invalid slot: index = " << slotIndex;

            return {0, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
        }

        return {slotList[slotIndex], ErrorEnum::eNone};
    }

    // Find free(not initialized) token by label.
    pkcs11::SlotInfo   slotInfo;
    Optional<uint32_t> freeSlotId;

    for (const auto slotId : slotList) {
        err = mPKCS11->GetSlotInfo(slotId, slotInfo);
        if (err.IsNone()) {
            return {0, AOS_ERROR_WRAP(err)};
        }

        if ((slotInfo.mFlags & CKF_TOKEN_PRESENT) != 0) {
            pkcs11::TokenInfo tokenInfo;

            err = mPKCS11->GetTokenInfo(slotId, tokenInfo);
            if (!err.IsNone()) {
                return {0, AOS_ERROR_WRAP(err)};
            }

            if (tokenInfo.mLabel == mConfig.mTokenLabel) {
                return {slotId, ErrorEnum::eNone};
            }

            if ((tokenInfo.mFlags & CKF_TOKEN_INITIALIZED) == 0 && !freeSlotId.HasValue()) {
                freeSlotId.SetValue(slotId);
            }
        }
    }

    if (freeSlotId.HasValue()) {
        return {freeSlotId.GetValue(), ErrorEnum::eNone};
    }

    LOG_ERR() << "No suitable slot found";

    return {0, AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
}

RetWithError<bool> PKCS11Module::IsOwned() const
{
    pkcs11::TokenInfo tokenInfo;

    auto err = mPKCS11->GetTokenInfo(mSlotID, tokenInfo);
    if (!err.IsNone()) {
        return {false, AOS_ERROR_WRAP(err)};
    }

    const bool isOwned = (tokenInfo.mFlags & CKF_TOKEN_INITIALIZED) != 0;

    return {isOwned, ErrorEnum::eNone};
}

Error PKCS11Module::PrintInfo(pkcs11::SlotID slotId) const
{
    pkcs11::LibInfo   libInfo;
    pkcs11::SlotInfo  slotInfo;
    pkcs11::TokenInfo tokenInfo;

    auto err = mPKCS11->GetLibInfo(libInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Library = " << mConfig.mLibrary << ", info = " << libInfo;

    err = mPKCS11->GetSlotInfo(slotId, slotInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "SlotID = " << slotId << ", slotInfo = " << slotInfo;

    err = mPKCS11->GetTokenInfo(slotId, tokenInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "SlotID = " << slotId << ", tokenInfo = " << tokenInfo;

    return ErrorEnum::eNone;
}

Error PKCS11Module::GetTeeUserPIN(const String& loginType, String& userPIN)
{
    if (loginType == cLoginTypePublic) {
        userPIN = loginType;
        return ErrorEnum::eNone;
    }

    if (loginType == cLoginTypeUser) {
        return GeneratePIN(cLoginTypeUser, userPIN);
    }

    if (loginType == cLoginTypeGroup) {
        return GeneratePIN(cLoginTypeGroup, userPIN);
    }

    LOG_ERR() << "Wrong TEE login: type = " << loginType;

    return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
}

Error PKCS11Module::GeneratePIN(const String& loginType, String& userPIN)
{
    uuid::UUID                   pin;
    StaticString<cUUIDStringLen> pinStr;
    Error                        err = ErrorEnum::eNone;

    Tie(pin, err) = uuid::CreateUUID();
    if (!err.IsNone()) {
        AOS_ERROR_WRAP(err);
    }

    Tie(pinStr, err) = uuid::UUIDToString(pin);
    if (!err.IsNone()) {
        AOS_ERROR_WRAP(err);
    }

    err = userPIN.Format("%s:%s", loginType.CStr(), pinStr.CStr());
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
};

Error PKCS11Module::GetUserPin(String& pin) const
{
    if (!mTeeLoginType.IsEmpty()) {
        pin.Clear();
        return ErrorEnum::eNone;
    }

    auto err = FS::ReadFileToString(mConfig.mUserPINPath, pin);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<pkcs11::SessionContext*> PKCS11Module::CreateSession(bool userLogin, const String& pin)
{
    Error err = ErrorEnum::eNone;

    if (!mSession) {
        Tie(mSession, err) = mPKCS11->OpenSession(mSlotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
        if (!err.IsNone()) {
            return {nullptr, AOS_ERROR_WRAP(err)};
        }
    }

    LOG_DBG() << "Create session: session = " << mSession->GetHandle() << ", slotID = " << mSlotID;

    pkcs11::SessionInfo sessionInfo;

    err = mSession->GetSessionInfo(sessionInfo);
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    const bool isUserLoggedIn
        = sessionInfo.state == CKS_RO_USER_FUNCTIONS || sessionInfo.state == CKS_RW_USER_FUNCTIONS;
    const bool isSOLoggedIn = sessionInfo.state == CKS_RW_SO_FUNCTIONS;

    if ((userLogin && isSOLoggedIn) || (!userLogin && isUserLoggedIn)) {
        err = mSession->Logout();
        if (!err.IsNone()) {
            return {nullptr, AOS_ERROR_WRAP(err)};
        }
    }

    if (userLogin && !isUserLoggedIn) {
        LOG_DBG() << "User login: session = " << mSession->GetHandle() << ", slotID = " << mSlotID;

        return {mSession.Get(), AOS_ERROR_WRAP(mSession->Login(CKU_USER, mUserPIN))};
    }

    if (!userLogin && !isSOLoggedIn) {
        LOG_DBG() << "SO login: session = " << mSession->GetHandle() << ", slotID = " << mSlotID;

        return {mSession.Get(), AOS_ERROR_WRAP(mSession->Login(CKU_SO, pin))};
    }

    return {mSession.Get(), ErrorEnum::eNone};
}

Error PKCS11Module::FindObject(pkcs11::SessionContext& session, const SearchObject& filter, Array<SearchObject>& dst)
{
    static constexpr auto cSearchObjAttrCount = 4;

    // create search template
    CK_BBOOL token = CK_TRUE;

    StaticArray<pkcs11::ObjectAttribute, cSearchObjAttrCount> templ;

    templ.EmplaceBack(CKA_TOKEN, Array<uint8_t>(&token, sizeof(token)));

    if (!filter.mID.IsEmpty()) {
        templ.EmplaceBack(CKA_ID, filter.mID);
    }

    if (!filter.mLabel.IsEmpty()) {
        const auto labelPtr = reinterpret_cast<const uint8_t*>(filter.mLabel.Get());

        templ.EmplaceBack(CKA_LABEL, Array<uint8_t>(labelPtr, filter.mLabel.Size()));
    }

    if (!filter.mType.HasValue()) {
        const auto classPtr = reinterpret_cast<const uint8_t*>(&filter.mType.GetValue());

        templ.EmplaceBack(CKA_CLASS, Array<uint8_t>(classPtr, sizeof(pkcs11::ObjectClass)));
    }

    // search object handles
    StaticArray<pkcs11::ObjectHandle, cCertsPerModule * 3> objects; // certs, privKeys, pubKeys

    auto err = session.FindObjects(templ, objects);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // retrieve attributes(id & label) and add search objects
    StaticArray<pkcs11::AttributeType, cSearchObjAttrCount> searchAttrTypes;

    searchAttrTypes.PushBack(CKA_ID);
    searchAttrTypes.PushBack(CKA_LABEL);

    for (const auto& object : objects) {
        err = dst.EmplaceBack();
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& searchObject = dst.Back().mValue;

        searchObject.mType   = filter.mType;
        searchObject.mHandle = object;

        StaticArray<Array<uint8_t>, cSearchObjAttrCount> searchAttrValues;
        StaticArray<uint8_t, pkcs11::cLabelLen>          label;

        searchAttrValues.PushBack(searchObject.mID);
        searchAttrValues.PushBack(label);

        err = session.GetAttributeValues(object, searchAttrTypes, searchAttrValues);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = pkcs11::Utils::ConvertPKCS11String(label, searchObject.mLabel);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::TokenMemInfo() const
{
    pkcs11::TokenInfo info;

    auto err = mPKCS11->GetTokenInfo(mSlotID, info);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Token mem info: publicMemory = " << info.mTotalPublicMemory - info.mFreePublicMemory << "/"
              << info.mTotalPublicMemory << ", privateMemory = " << info.mTotalPrivateMemory - info.mFreePrivateMemory
              << "/" << info.mTotalPrivateMemory;

    return ErrorEnum::eNone;
}

bool PKCS11Module::CheckCertificate(const crypto::x509::Certificate& cert, const crypto::PrivateKeyItf& key) const
{
    return cert.mPublicKey->IsEqual(key.GetPublic());
}

Error PKCS11Module::CreateCertificateChain(pkcs11::SessionContext& session, const Array<uint8_t>& id,
    const String& label, const Array<crypto::x509::Certificate>& chain)
{
    auto utils = pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator);

    auto err = utils.ImportCertificate(id, label, chain[0]);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (size_t i = 1; i < chain.Size(); i++) {
        bool hasCertificate = false;

        Tie(hasCertificate, err) = utils.HasCertificate(chain[i].mIssuer, chain[i].mSerial);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (hasCertificate) {
            continue;
        }

        uuid::UUID uuid;

        Tie(uuid, err) = uuid::CreateUUID();
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = utils.ImportCertificate(uuid, label, chain[i]);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::CreateURL(const String& label, const Array<uint8_t>& id, String& url)
{
    const auto addParam = [](const char* name, const char* param, bool opaque, String& paramList) {
        if (!paramList.IsEmpty()) {
            const char* delim = opaque ? ";" : "&";
            paramList.Append(delim);
        }

        paramList.Append(name).Append("=").Append(param);
    };

    StaticString<cURLLen> opaque, query;

    // create opaque part of url
    addParam("token", mConfig.mTokenLabel.CStr(), true, opaque);

    if (!label.IsEmpty()) {
        addParam("object", label.CStr(), true, opaque);
    }

    if (!id.IsEmpty()) {
        StaticString<uuid::cUUIDStrLen> uuid;
        Error                           err = ErrorEnum::eNone;

        Tie(uuid, err) = uuid::UUIDToString(id);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        addParam("id", uuid.CStr(), true, opaque);
    }

    // create query part of url
    if (mConfig.mModulePathInURL) {
        addParam("module-path", mConfig.mLibrary.CStr(), false, query);
    }

    addParam("pin-value", mUserPIN.CStr(), false, query);

    // combine opaque & query parts of url
    auto err = url.Format("%s:%s?%s", cPKCS11Scheme, opaque.CStr(), query.CStr());
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::ParseURL(const String& url, String& label, Array<uint8_t>& id)
{
    StaticString<cFilePathLen>       library;
    StaticString<pkcs11::cLabelLen>  token;
    StaticString<pkcs11::cPINLength> userPIN;

    auto err = cryptoutils::ParsePKCS11URL(url, library, token, label, id, userPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::GetValidInfo(pkcs11::SessionContext& session, Array<SearchObject>& certs,
    Array<SearchObject>& privKeys, Array<SearchObject>& pubKeys, Array<CertInfo>& resCerts)
{
    for (auto privKey = privKeys.begin(); privKey != privKeys.end();) {
        LOG_DBG() << "Private key found: ID = " << privKey->mID;

        auto pubKey = FindObjectByID(pubKeys, privKey->mID);
        if (pubKey == pubKeys.end()) {
            privKey++;
            continue;
        }

        LOG_DBG() << "Public key found: ID = " << privKey->mID;

        auto cert = FindObjectByID(certs, privKey->mID);
        if (cert == certs.end()) {
            privKey++;
            continue;
        }

        LOG_DBG() << "Certificate found: ID = " << privKey->mID;

        // create certInfo
        auto     x509Cert = MakeUnique<crypto::x509::Certificate>(&mTmpObjAllocator);
        CertInfo validCert;

        auto err = GetX509Cert(session, cert->mHandle, *x509Cert);
        if (!err.IsNone()) {
            LOG_ERR() << "Can't get x509 certificate: ID = " << cert->mID;
            return err;
        }

        err = CreateCertInfo(*x509Cert, privKey->mID, cert->mID, validCert);
        if (!err.IsNone()) {
            return err;
        }

        // update containers
        err = resCerts.PushBack(validCert);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto ret = certs.Remove(cert);
        if (!ret.mError.IsNone()) {
            return AOS_ERROR_WRAP(ret.mError);
        }

        ret = pubKeys.Remove(pubKey);
        if (!ret.mError.IsNone()) {
            return AOS_ERROR_WRAP(ret.mError);
        }

        ret = privKeys.Remove(privKey);
        if (!ret.mError.IsNone()) {
            return AOS_ERROR_WRAP(ret.mError);
        }

        privKey = ret.mValue;
    }

    return ErrorEnum::eNone;
}

PKCS11Module::SearchObject* PKCS11Module::FindObjectByID(Array<SearchObject>& array, const Array<uint8_t>& id)
{
    SearchObject* result = array.begin();
    while (result != array.end()) {
        if (result->mID == id) {
            return result;
        }
    }

    return result;
}

Error PKCS11Module::GetX509Cert(
    pkcs11::SessionContext& session, pkcs11::ObjectHandle object, crypto::x509::Certificate& cert)
{
    static constexpr auto cSingleAttribute = 1;

    auto certBuffer = MakeUnique<DERCert>(&mTmpObjAllocator);

    StaticArray<pkcs11::AttributeType, cSingleAttribute> types;
    StaticArray<Array<uint8_t>, cSingleAttribute>        values;

    types.PushBack(CKA_VALUE);
    values.PushBack(*certBuffer);

    auto err = session.GetAttributeValues(object, types, values);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mX509Provider->DERToX509Cert(*certBuffer, cert);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::CreateCertInfo(const crypto::x509::Certificate& cert, const Array<uint8_t>& keyID,
    const Array<uint8_t>& certID, CertInfo& certInfo)
{
    certInfo.mIssuer   = cert.mIssuer;
    certInfo.mNotAfter = cert.mNotAfter;
    certInfo.mSerial   = cert.mSerial;

    auto err = CreateURL(mCertType, certID, certInfo.mCertURL);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = CreateURL(mCertType, keyID, certInfo.mKeyURL);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::CreateInvalidURLs(const Array<SearchObject>& objects, Array<StaticString<cURLLen>>& urls)
{
    StaticString<cURLLen> url;

    for (const auto& cert : objects) {
        auto err = CreateURL(mCertType, cert.mID, url);
        if (!err.IsNone()) {
            return err;
        }

        err = urls.PushBack(url);
        if (err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

} // namespace certhandler
} // namespace iam
} // namespace aos
