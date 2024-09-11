/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/crypto/utils.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/os.hpp"
#include "aos/common/tools/uuid.hpp"

#include "aos/iam/certmodules/pkcs11/pkcs11.hpp"

#include "../../certhandler/log.hpp"

namespace aos::iam::certhandler {

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

    mTokenLabel = GetTokenLabel();

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

        err = GetUserPin(mUserPIN);
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

    CloseSession();
    err = mPKCS11->CloseAllSessions(mSlotID);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<pkcs11::cPINLen> userPIN, soPIN;

    if (!mTeeLoginType.IsEmpty()) {
        err = GetTeeUserPIN(mTeeLoginType, mConfig.mUID, mConfig.mGID, userPIN);
        if (!err.IsNone()) {
            return err;
        }

        mUserPIN.Clear();
        soPIN.Clear();
    } else {
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

        mUserPIN = userPIN;
        soPIN    = password;
    }

    LOG_DBG() << "Init token: slotID=" << mSlotID << ", label=" << mTokenLabel;

    err = mPKCS11->InitToken(mSlotID, soPIN, mTokenLabel);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    SharedPtr<pkcs11::SessionContext> session;

    Tie(session, err) = CreateSession(false, soPIN);
    if (!err.IsNone()) {
        return err;
    }

    if (!mTeeLoginType.IsEmpty()) {
        LOG_DBG() << "Init PIN: pin=" << userPIN << ", session=" << session->GetHandle();
    } else {
        LOG_DBG() << "Init PIN: session=" << session->GetHandle();
    }

    err = session->InitPIN(userPIN);

    CloseSession();

    return AOS_ERROR_WRAP(err);
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

    SharedPtr<pkcs11::SessionContext> session;

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // certs, privKeys, pubKeys
    auto objects = MakeUnique<StaticArray<SearchObject, cCertsPerModule * 3>>(&mTmpObjAllocator);
    auto filter  = MakeUnique<SearchObject>(&mTmpObjAllocator);

    err = FindObject(*session, *filter, *objects);
    if (err.IsNone()) {
        for (const auto& object : *objects) {
            LOG_DBG() << "Destroy object: " << object.mHandle;

            auto destroyErr = session->DestroyObject(object.mHandle);
            if (!destroyErr.IsNone()) {
                err = AOS_ERROR_WRAP(destroyErr);
                LOG_ERR() << "Can't delete object: handle=" << object.mHandle;
            }
        }
    }

    CloseSession();

    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return err;
    }

    return ErrorEnum::eNone;
}

RetWithError<SharedPtr<crypto::PrivateKeyItf>> PKCS11Module::CreateKey(const String& password, crypto::KeyType keyType)
{
    (void)password;

    PKCS11Module::PendingKey pendingKey;
    Error                    err = ErrorEnum::eNone;

    pendingKey.mUUID = uuid::CreateUUID();

    SharedPtr<pkcs11::SessionContext> session;

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    switch (keyType.GetValue()) {
    case crypto::KeyTypeEnum::eRSA:
        Tie(pendingKey.mKey, err) = pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator)
                                        .GenerateRSAKeyPairWithLabel(pendingKey.mUUID, mCertType, cRSAKeyLength);
        if (!err.IsNone()) {
            return {nullptr, AOS_ERROR_WRAP(err)};
        }
        break;

    case crypto::KeyTypeEnum::eECDSA:
        Tie(pendingKey.mKey, err) = pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator)
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
        pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator).DeletePrivateKey(pendingKey.mKey);
        return {nullptr, err};
    }

    if (mPendingKeys.Size() == mPendingKeys.MaxSize()) {
        LOG_WRN() << "Max pending keys reached: Remove old: certType=" << mCertType;

        auto oldKey = mPendingKeys.Front().mKey;

        err = pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator).DeletePrivateKey(oldKey);
        if (!err.IsNone()) {
            LOG_ERR() << "Can't delete pending key: err=" << err;
        }

        mPendingKeys.Erase(mPendingKeys.begin());
    }

    mPendingKeys.PushBack(pendingKey);

    return {pendingKey.mKey.GetPrivKey(), ErrorEnum::eNone};
}

Error PKCS11Module::ApplyCert(const Array<crypto::x509::Certificate>& certChain, CertInfo& certInfo, String& password)
{
    (void)password;

    Error                             err = ErrorEnum::eNone;
    SharedPtr<pkcs11::SessionContext> session;

    Tie(session, err) = CreateSession(true, mUserPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Optional<PendingKey> curKey;

    for (auto it = mPendingKeys.begin(); it != mPendingKeys.end(); ++it) {
        if (CheckCertificate(certChain[0], *it->mKey.GetPrivKey())) {
            curKey.SetValue(*it);
            mPendingKeys.Erase(it);

            break;
        }
    }

    if (!curKey.HasValue()) {
        LOG_ERR() << "No corresponding key found";
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    err = CreateCertificateChain(session, curKey.GetValue().mUUID, mCertType, certChain);
    if (!err.IsNone()) {
        return err;
    }

    err = CreateURL(mCertType, curKey.GetValue().mUUID, certInfo.mCertURL);
    if (!err.IsNone()) {
        return err;
    }

    certInfo.mKeyURL   = certInfo.mCertURL;
    certInfo.mIssuer   = certChain[0].mIssuer;
    certInfo.mNotAfter = certChain[0].mNotAfter;
    certInfo.mSerial   = certChain[0].mSerial;

    LOG_DBG() << "Certificate applied: cert=" << certInfo;

    return ErrorEnum::eNone;
}

Error PKCS11Module::RemoveCert(const String& certURL, const String& password)
{
    (void)password;

    Error                             err = ErrorEnum::eNone;
    SharedPtr<pkcs11::SessionContext> session;

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

    return pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator).DeleteCertificate(id, label);
}

Error PKCS11Module::RemoveKey(const String& keyURL, const String& password)
{
    (void)password;

    Error                             err = ErrorEnum::eNone;
    SharedPtr<pkcs11::SessionContext> session;

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

    const auto privKey = pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator).FindPrivateKey(id, label);
    if (!privKey.mError.IsNone()) {
        return AOS_ERROR_WRAP(privKey.mError);
    }

    err = pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator).DeletePrivateKey(privKey.mValue);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::ValidateCertificates(
    Array<StaticString<cURLLen>>& invalidCerts, Array<StaticString<cURLLen>>& invalidKeys, Array<CertInfo>& validCerts)
{
    Error                             err     = ErrorEnum::eNone;
    bool                              isOwned = false;
    SharedPtr<pkcs11::SessionContext> session;

    LOG_DBG() << "Validate certificates: certType=" << mCertType;

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
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return err;
    }

    filter.mType = CKO_PRIVATE_KEY;

    err = FindObject(*session, filter, privKeys);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return err;
    }

    filter.mType = CKO_PUBLIC_KEY;

    err = FindObject(*session, filter, pubKeys);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return err;
    }

    // generate valid info
    err = GetValidInfo(*session, certificates, privKeys, pubKeys, validCerts);
    if (!err.IsNone()) {
        return err;
    }

    PrintInvalidObjects("certificate", certificates);
    PrintInvalidObjects("public key", pubKeys);
    PrintInvalidObjects("private key", privKeys);

    // create urls for invalid objects
    err = CreateInvalidURLs(certificates, invalidCerts);
    if (!err.IsNone()) {
        return err;
    }

    // Return either private or public keys, otherwise we will have the same URLs in the list
    // Currently removing key-pairs is supported only. Priv/Pub keys without pair can't be deleted from the storage.
    return CreateInvalidURLs(pubKeys, invalidKeys);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

StaticString<pkcs11::cLabelLen> PKCS11Module::GetTokenLabel() const
{
    return mConfig.mTokenLabel.IsEmpty() ? cDefaultTokenLabel : mConfig.mTokenLabel;
}

RetWithError<pkcs11::SlotID> PKCS11Module::GetSlotID()
{
    const int paramCount
        = static_cast<int>(mConfig.mSlotID.HasValue() + mConfig.mSlotIndex.HasValue() + !mConfig.mTokenLabel.IsEmpty());

    if (paramCount > 1) {
        LOG_ERR()
            << "Only one parameter for slot identification should be specified (slotID or slotIndex or tokenLabel)";

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
            LOG_ERR() << "Invalid slot: index=" << slotIndex;

            return {0, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
        }

        return {slotList[slotIndex], ErrorEnum::eNone};
    }

    // Find free(not initialized) token by label.
    pkcs11::SlotInfo   slotInfo;
    Optional<uint32_t> freeSlotID;

    for (const auto slotID : slotList) {
        err = mPKCS11->GetSlotInfo(slotID, slotInfo);
        if (!err.IsNone()) {
            return {0, AOS_ERROR_WRAP(err)};
        }

        if ((slotInfo.mFlags & CKF_TOKEN_PRESENT) != 0) {
            auto tokenInfo = MakeUnique<pkcs11::TokenInfo>(&mTmpObjAllocator);

            err = mPKCS11->GetTokenInfo(slotID, *tokenInfo);
            if (!err.IsNone()) {
                return {0, AOS_ERROR_WRAP(err)};
            }

            if (tokenInfo->mLabel == mTokenLabel) {
                return {slotID, ErrorEnum::eNone};
            }

            if ((tokenInfo->mFlags & CKF_TOKEN_INITIALIZED) == 0 && !freeSlotID.HasValue()) {
                freeSlotID.SetValue(slotID);
            }
        }
    }

    if (freeSlotID.HasValue()) {
        return {freeSlotID.GetValue(), ErrorEnum::eNone};
    }

    LOG_ERR() << "No suitable slot found";

    return {0, AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
}

RetWithError<bool> PKCS11Module::IsOwned() const
{
    auto tokenInfo = MakeUnique<pkcs11::TokenInfo>(&mTmpObjAllocator);

    auto err = mPKCS11->GetTokenInfo(mSlotID, *tokenInfo);
    if (!err.IsNone()) {
        return {false, AOS_ERROR_WRAP(err)};
    }

    const bool isOwned = (tokenInfo->mFlags & CKF_TOKEN_INITIALIZED) != 0;

    return {isOwned, ErrorEnum::eNone};
}

Error PKCS11Module::PrintInfo(pkcs11::SlotID slotID) const
{
    pkcs11::LibInfo   libInfo;
    pkcs11::SlotInfo  slotInfo;
    pkcs11::TokenInfo tokenInfo;

    auto err = mPKCS11->GetLibInfo(libInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Library=" << mConfig.mLibrary << ", info=" << libInfo;

    err = mPKCS11->GetSlotInfo(slotID, slotInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "SlotID=" << slotID << ", slotInfo=" << slotInfo;

    err = mPKCS11->GetTokenInfo(slotID, tokenInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "SlotID=" << slotID << ", tokenInfo=" << tokenInfo;

    return ErrorEnum::eNone;
}

Error PKCS11Module::GetTeeUserPIN(const String& loginType, uint32_t uid, uint32_t gid, String& userPIN)
{
    if (loginType == cLoginTypePublic) {
        userPIN = loginType;
        return ErrorEnum::eNone;
    }

    if (loginType == cLoginTypeUser) {
        return GenTeeUserPIN(cLoginTypeUser, "uid", uid, userPIN);
    }

    if (loginType == cLoginTypeGroup) {
        return GenTeeUserPIN(cLoginTypeGroup, "gid", gid, userPIN);
    }

    LOG_ERR() << "Wrong TEE login: type=" << loginType;

    return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
}

Error PKCS11Module::GenTeeUserPIN(const String& loginType, const String& idType, uint32_t id, String& userPIN)
{
    StaticString<pkcs11::cPINLen> userID;
    uuid::UUID                    teeSpace;
    uuid::UUID                    userSHA1;
    Error                         err = ErrorEnum::eNone;

    Tie(teeSpace, err) = uuid::StringToUUID(cTeeClientUUIDNs);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = userID.Format("%s=%d", idType.CStr(), id);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Tie(userSHA1, err) = mX509Provider->CreateUUIDv5(teeSpace, userID.AsByteArray());
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = userPIN.Format("%s:%s", loginType.CStr(), uuid::UUIDToString(userSHA1).CStr());
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

RetWithError<SharedPtr<pkcs11::SessionContext>> PKCS11Module::CreateSession(bool userLogin, const String& pin)
{
    Error err = ErrorEnum::eNone;

    if (!mSession) {
        Tie(mSession, err) = mPKCS11->OpenSession(mSlotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
        if (!err.IsNone()) {
            return {nullptr, AOS_ERROR_WRAP(err)};
        }
    }

    LOG_DBG() << "Create session: session=" << mSession->GetHandle() << ", slotID=" << mSlotID;

    auto sessionInfo = MakeShared<pkcs11::SessionInfo>(&mTmpObjAllocator);

    err = mSession->GetSessionInfo(*sessionInfo);
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    const bool isUserLoggedIn
        = sessionInfo->state == CKS_RO_USER_FUNCTIONS || sessionInfo->state == CKS_RW_USER_FUNCTIONS;
    const bool isSOLoggedIn = sessionInfo->state == CKS_RW_SO_FUNCTIONS;

    if ((userLogin && isSOLoggedIn) || (!userLogin && isUserLoggedIn)) {
        err = mSession->Logout();
        if (!err.IsNone()) {
            return {nullptr, AOS_ERROR_WRAP(err)};
        }
    }

    if (userLogin && !isUserLoggedIn) {
        LOG_DBG() << "User login: session=" << mSession->GetHandle() << ", slotID=" << mSlotID;

        return {mSession, AOS_ERROR_WRAP(mSession->Login(CKU_USER, mUserPIN))};
    }

    if (!userLogin && !isSOLoggedIn) {
        LOG_DBG() << "SO login: session=" << mSession->GetHandle() << ", slotID=" << mSlotID;

        return {mSession, AOS_ERROR_WRAP(mSession->Login(CKU_SO, pin))};
    }

    return {mSession, ErrorEnum::eNone};
}

void PKCS11Module::CloseSession()
{
    mSession.Reset();
    mPKCS11->ClearSessions();
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

    if (filter.mType.HasValue()) {
        const auto classPtr = reinterpret_cast<const uint8_t*>(&filter.mType.GetValue());

        templ.EmplaceBack(CKA_CLASS, Array<uint8_t>(classPtr, sizeof(pkcs11::ObjectClass)));
    }

    // search object handles
    StaticArray<pkcs11::ObjectHandle, cCertsPerModule * 3> objects; // certs, privKeys, pubKeys

    auto err = session.FindObjects(templ, objects);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
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

        auto& searchObject = dst.Back();

        searchObject.mType   = filter.mType;
        searchObject.mHandle = object;
        searchObject.mID.Resize(searchObject.mID.MaxSize());

        StaticArray<Array<uint8_t>, cSearchObjAttrCount> searchAttrValues;
        StaticArray<uint8_t, pkcs11::cLabelLen>          label;

        searchAttrValues.PushBack(searchObject.mID);
        searchAttrValues.PushBack(label);

        err = session.GetAttributeValues(object, searchAttrTypes, searchAttrValues);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        searchObject.mID.Resize(searchAttrValues[0].Size());

        err = pkcs11::Utils::ConvertPKCS11String(searchAttrValues[1], searchObject.mLabel);
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

    LOG_DBG() << "Token mem info: publicMemory=" << info.mTotalPublicMemory - info.mFreePublicMemory << "/"
              << info.mTotalPublicMemory << ", privateMemory=" << info.mTotalPrivateMemory - info.mFreePrivateMemory
              << "/" << info.mTotalPrivateMemory;

    return ErrorEnum::eNone;
}

bool PKCS11Module::CheckCertificate(const crypto::x509::Certificate& cert, const crypto::PrivateKeyItf& key) const
{
    return GetBase<crypto::PublicKeyItf>(cert.mPublicKey).IsEqual(key.GetPublic());
}

Error PKCS11Module::CreateCertificateChain(const SharedPtr<pkcs11::SessionContext>& session, const Array<uint8_t>& id,
    const String& label, const Array<crypto::x509::Certificate>& chain)
{
    auto utils = pkcs11::Utils(session, *mX509Provider, mLocalCacheAllocator);

    LOG_DBG() << "Import certificate with id: " << aos::uuid::UUIDToString(id);
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

        auto uuid = uuid::CreateUUID();

        LOG_DBG() << "Import root certificate with id: " << aos::uuid::UUIDToString(uuid);

        err = utils.ImportCertificate(uuid, "", chain[i]);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::CreateURL(const String& label, const Array<uint8_t>& id, String& url)
{
    const auto AddParam = [](const aos::String& name, const aos::String& param, bool opaque, String& paramList) {
        if (!paramList.IsEmpty()) {
            paramList.Append(opaque ? ";" : "&");
        }

        paramList.Append(name).Append("=").Append(param);
    };

    StaticString<cURLLen> opaque, query;

    // create opaque part of url
    AddParam("token", mTokenLabel.CStr(), true, opaque);

    if (!label.IsEmpty()) {
        AddParam("object", label.CStr(), true, opaque);
    }

    if (!id.IsEmpty()) {
        StaticString<pkcs11::cIDStrLen> idStr;

        auto err = crypto::EncodePKCS11ID(id, idStr);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        AddParam("id", idStr, true, opaque);
    }

    // create query part of url
    if (mConfig.mModulePathInURL) {
        AddParam("module-path", mConfig.mLibrary.CStr(), false, query);
    }

    if (!mUserPIN.IsEmpty()) {
        AddParam("pin-source", mConfig.mUserPINPath, false, query);
    }

    // combine opaque & query parts of url
    auto err = url.Format("%s:%s?%s", cPKCS11Scheme, opaque.CStr(), query.CStr());
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::ParseURL(const String& url, String& label, Array<uint8_t>& id)
{
    StaticString<cFilePathLen>      library;
    StaticString<pkcs11::cLabelLen> token;
    StaticString<pkcs11::cPINLen>   userPIN;

    auto err = crypto::ParsePKCS11URL(url, library, token, label, id, userPIN);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error PKCS11Module::GetValidInfo(const pkcs11::SessionContext& session, Array<SearchObject>& certs,
    Array<SearchObject>& privKeys, Array<SearchObject>& pubKeys, Array<CertInfo>& resCerts)
{
    for (auto privKey = privKeys.begin(); privKey != privKeys.end();) {
        LOG_DBG() << "Private key found: ID=" << uuid::UUIDToString(privKey->mID);

        auto pubKey = FindObjectByID(pubKeys, privKey->mID);
        if (pubKey == pubKeys.end()) {
            privKey++;
            continue;
        }

        LOG_DBG() << "Public key found: ID=" << uuid::UUIDToString(pubKey->mID);

        auto cert = FindObjectByID(certs, privKey->mID);
        if (cert == certs.end()) {
            privKey++;
            continue;
        }

        LOG_DBG() << "Certificate found: ID=" << uuid::UUIDToString(cert->mID);

        // create certInfo
        auto x509Cert  = MakeUnique<crypto::x509::Certificate>(&mTmpObjAllocator);
        auto validCert = MakeUnique<CertInfo>(&mTmpObjAllocator);

        auto err = GetX509Cert(session, cert->mHandle, *x509Cert);
        if (!err.IsNone()) {
            LOG_ERR() << "Can't get x509 certificate: ID=" << uuid::UUIDToString(cert->mID);
            return err;
        }

        err = CreateCertInfo(*x509Cert, privKey->mID, cert->mID, *validCert);
        if (!err.IsNone()) {
            return err;
        }

        // update containers
        err = resCerts.PushBack(*validCert);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        certs.Erase(cert);
        pubKeys.Erase(pubKey);
        privKey = privKeys.Erase(privKey);
    }

    return ErrorEnum::eNone;
}

PKCS11Module::SearchObject* PKCS11Module::FindObjectByID(Array<SearchObject>& array, const Array<uint8_t>& id)
{
    for (SearchObject* cur = array.begin(); cur != array.end(); cur++) {
        if (cur->mID == id) {
            return cur;
        }
    }

    return array.end();
}

Error PKCS11Module::GetX509Cert(
    const pkcs11::SessionContext& session, pkcs11::ObjectHandle object, crypto::x509::Certificate& cert)
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

    err = mX509Provider->DERToX509Cert(values[0], cert);
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

void PKCS11Module::PrintInvalidObjects(const String& objectType, const Array<SearchObject>& objects)
{
    for (const auto& object : objects) {
        LOG_WRN() << "Invalid " << objectType << " found: certType=" << mCertType
                  << ", id=" << uuid::UUIDToString(object.mID);
    }
}

} // namespace aos::iam::certhandler
