/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <deque>
#include <dlfcn.h>
#include <fstream>
#include <map>
#include <vector>

#include <gmock/gmock.h>

#include <core/common/pkcs11/privatekey.hpp>
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/crypto/softhsmenv.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/common/tools/uuid.hpp>

using namespace testing;

namespace aos::pkcs11 {
namespace {
/***********************************************************************************************************************
 * Fake cryptoki
 **********************************************************************************************************************/

class FakeCryptoki {
public:
    struct FindResult {
        CK_RV                         mRV = CKR_OK;
        std::vector<CK_OBJECT_HANDLE> mHandles;
    };

    using Attributes = std::map<CK_ATTRIBUTE_TYPE, std::vector<uint8_t>>;

    static void Reset()
    {
        sRV              = CKR_GENERAL_ERROR;
        sCloseSessionRV  = CKR_OK;
        sSignInitRV      = CKR_GENERAL_ERROR;
        sDecryptInitRV   = CKR_GENERAL_ERROR;
        sFindObjectsRV   = CKR_OK;
        sFindFinalRV     = CKR_OK;
        sGenerateKeyRV   = CKR_GENERAL_ERROR;
        sGetAttributeRV  = CKR_GENERAL_ERROR;
        sDestroyObjectRV = CKR_GENERAL_ERROR;
        sFindResults     = {};
        sCurrentFind     = {};
        sObjects         = {};
        sFunctionList    = {};

        sFunctionList.C_CloseSession      = CloseSession;
        sFunctionList.C_GetSessionInfo    = GetSessionInfo;
        sFunctionList.C_Logout            = Logout;
        sFunctionList.C_InitPIN           = InitPIN;
        sFunctionList.C_GetAttributeValue = GetAttributeValue;
        sFunctionList.C_CreateObject      = CreateObject;
        sFunctionList.C_DestroyObject     = DestroyObject;
        sFunctionList.C_SignInit          = SignInit;
        sFunctionList.C_Sign              = Sign;
        sFunctionList.C_DecryptInit       = DecryptInit;
        sFunctionList.C_Decrypt           = Decrypt;
        sFunctionList.C_FindObjectsInit   = FindObjectsInit;
        sFunctionList.C_FindObjects       = FindObjects;
        sFunctionList.C_FindObjectsFinal  = FindObjectsFinal;
        sFunctionList.C_GenerateKeyPair   = GenerateKeyPair;
    }

    static CK_FUNCTION_LIST_PTR GetFunctionList() { return &sFunctionList; }

    static CK_RV                                  sRV;
    static CK_RV                                  sCloseSessionRV;
    static CK_RV                                  sSignInitRV;
    static CK_RV                                  sDecryptInitRV;
    static CK_RV                                  sFindObjectsRV;
    static CK_RV                                  sFindFinalRV;
    static CK_RV                                  sGenerateKeyRV;
    static CK_RV                                  sGetAttributeRV;
    static CK_RV                                  sDestroyObjectRV;
    static std::deque<FindResult>                 sFindResults;
    static std::map<CK_OBJECT_HANDLE, Attributes> sObjects;

private:
    static CK_RV CloseSession(CK_SESSION_HANDLE) { return sCloseSessionRV; }
    static CK_RV GetSessionInfo(CK_SESSION_HANDLE, CK_SESSION_INFO_PTR) { return sRV; }
    static CK_RV Logout(CK_SESSION_HANDLE) { return sRV; }
    static CK_RV InitPIN(CK_SESSION_HANDLE, CK_UTF8CHAR_PTR, CK_ULONG) { return sRV; }
    static CK_RV CreateObject(CK_SESSION_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR) { return sRV; }
    static CK_RV DestroyObject(CK_SESSION_HANDLE, CK_OBJECT_HANDLE) { return sDestroyObjectRV; }
    static CK_RV SignInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE) { return sSignInitRV; }
    static CK_RV Sign(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR) { return sRV; }
    static CK_RV DecryptInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE) { return sDecryptInitRV; }
    static CK_RV Decrypt(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR) { return sRV; }

    static CK_RV GetAttributeValue(CK_SESSION_HANDLE, CK_OBJECT_HANDLE object, CK_ATTRIBUTE_PTR templ, CK_ULONG count)
    {
        if (sGetAttributeRV != CKR_OK) {
            return sGetAttributeRV;
        }

        const auto& attributes = sObjects[object];

        for (CK_ULONG i = 0; i < count; i++) {
            auto it = attributes.find(templ[i].type);
            if (it == attributes.end()) {
                templ[i].ulValueLen = CK_UNAVAILABLE_INFORMATION;

                continue;
            }

            if (it->second.size() > templ[i].ulValueLen) {
                return CKR_BUFFER_TOO_SMALL;
            }

            std::copy(it->second.begin(), it->second.end(), static_cast<uint8_t*>(templ[i].pValue));
            templ[i].ulValueLen = it->second.size();
        }

        return CKR_OK;
    }

    static CK_RV FindObjectsInit(CK_SESSION_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG)
    {
        if (sFindResults.empty()) {
            return sRV;
        }

        sCurrentFind = sFindResults.front();
        sFindResults.pop_front();

        return sCurrentFind.mRV;
    }

    static CK_RV FindObjects(CK_SESSION_HANDLE, CK_OBJECT_HANDLE_PTR objects, CK_ULONG maxCount, CK_ULONG_PTR count)
    {
        if (sFindObjectsRV != CKR_OK) {
            return sFindObjectsRV;
        }

        *count = 0;

        while (*count < maxCount && !sCurrentFind.mHandles.empty()) {
            objects[(*count)++] = sCurrentFind.mHandles.front();
            sCurrentFind.mHandles.erase(sCurrentFind.mHandles.begin());
        }

        return CKR_OK;
    }

    static CK_RV FindObjectsFinal(CK_SESSION_HANDLE) { return sFindFinalRV; }

    static CK_RV GenerateKeyPair(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_ATTRIBUTE_PTR, CK_ULONG, CK_ATTRIBUTE_PTR,
        CK_ULONG, CK_OBJECT_HANDLE_PTR pubKey, CK_OBJECT_HANDLE_PTR privKey)
    {
        *pubKey  = cFakePubKeyHandle;
        *privKey = cFakePrivKeyHandle;

        return sGenerateKeyRV;
    }

    static FindResult       sCurrentFind;
    static CK_FUNCTION_LIST sFunctionList;

public:
    static constexpr CK_OBJECT_HANDLE cFakePrivKeyHandle = 100;
    static constexpr CK_OBJECT_HANDLE cFakePubKeyHandle  = 101;
};

CK_RV                                                FakeCryptoki::sRV              = CKR_GENERAL_ERROR;
CK_RV                                                FakeCryptoki::sCloseSessionRV  = CKR_OK;
CK_RV                                                FakeCryptoki::sSignInitRV      = CKR_GENERAL_ERROR;
CK_RV                                                FakeCryptoki::sDecryptInitRV   = CKR_GENERAL_ERROR;
CK_RV                                                FakeCryptoki::sFindObjectsRV   = CKR_OK;
CK_RV                                                FakeCryptoki::sFindFinalRV     = CKR_OK;
CK_RV                                                FakeCryptoki::sGenerateKeyRV   = CKR_GENERAL_ERROR;
CK_RV                                                FakeCryptoki::sGetAttributeRV  = CKR_GENERAL_ERROR;
CK_RV                                                FakeCryptoki::sDestroyObjectRV = CKR_GENERAL_ERROR;
std::deque<FakeCryptoki::FindResult>                 FakeCryptoki::sFindResults;
std::map<CK_OBJECT_HANDLE, FakeCryptoki::Attributes> FakeCryptoki::sObjects;
FakeCryptoki::FindResult                             FakeCryptoki::sCurrentFind;
CK_FUNCTION_LIST                                     FakeCryptoki::sFunctionList {};

template <typename T>
std::vector<uint8_t> ToBytes(T value)
{
    const auto* ptr = reinterpret_cast<const uint8_t*>(&value);

    return std::vector<uint8_t>(ptr, ptr + sizeof(value));
}

std::vector<uint8_t> ReadDER(const char* path)
{
    std::ifstream file(path, std::ios::binary);

    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

FakeCryptoki::Attributes CertObject(const std::vector<uint8_t>& der)
{
    return {{CKA_CLASS, ToBytes<CK_OBJECT_CLASS>(CKO_CERTIFICATE)}, {CKA_VALUE, der}};
}

class RandomStub : public crypto::RandomItf {
public:
    RetWithError<uint64_t> RandInt(uint64_t maxValue) override { return {maxValue, ErrorEnum::eFailed}; }
    Error                  RandBuffer(Array<uint8_t>& buffer, size_t size) override
    {
        (void)buffer;
        (void)size;

        return ErrorEnum::eFailed;
    }
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class PKCS11Test : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        ASSERT_TRUE(mCryptoFactory.Init(mAllocator).IsNone());
        mCryptoProvider = &mCryptoFactory.GetCryptoProvider();
        mHashProvider   = &mCryptoFactory.GetHashProvider();

        ASSERT_TRUE(mSoftHSMEnv.Init(mAllocator, mPIN, mLabel).IsNone());

        mLibrary = mSoftHSMEnv.GetLibrary();
        mSlotID  = mSoftHSMEnv.GetSlotID();
    }

    static constexpr auto mLabel = "iam pkcs11 test slot";
    static constexpr auto mPIN   = "admin";

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

    crypto::DefaultCryptoFactory mCryptoFactory;
    crypto::CryptoProviderItf*   mCryptoProvider = nullptr;
    crypto::HasherItf*           mHashProvider   = nullptr;
    test::SoftHSMEnv             mSoftHSMEnv;

    SlotID                    mSlotID = 0;
    SharedPtr<LibraryContext> mLibrary;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(PKCS11Test, PrintTestTokenInfo)
{
    LibInfo libInfo;
    ASSERT_TRUE(mSoftHSMEnv.GetLibrary()->GetLibInfo(libInfo).IsNone());
    LOG_INF() << "Lib Info: " << libInfo;

    SlotInfo slotInfo;

    ASSERT_TRUE(mSoftHSMEnv.GetLibrary()->GetSlotInfo(mSlotID, slotInfo).IsNone());
    LOG_INF() << "Test Slot Info: " << slotInfo;

    TokenInfo tokenInfo;

    ASSERT_TRUE(mSoftHSMEnv.GetLibrary()->GetTokenInfo(mSlotID, tokenInfo).IsNone());
    LOG_INF() << "Test Token Info: " << tokenInfo;
}

TEST_F(PKCS11Test, Login)
{
    constexpr auto cBadPIN = "user";

    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.GetLibrary()->OpenSession(mSlotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
    ASSERT_TRUE(err.IsNone() && session);

    // Login OK
    ASSERT_TRUE(session->Login(CKU_USER, mPIN).IsNone());
    ASSERT_TRUE(session->Logout().IsNone());

    // Login NOK
    ASSERT_FALSE(session->Login(CKU_USER, cBadPIN).IsNone());
}

TEST_F(PKCS11Test, SessionInfo)
{
    Error                     err;
    SharedPtr<SessionContext> session;
    SessionInfo               sessionInfo;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN);
    ASSERT_TRUE(err.IsNone());

    ASSERT_TRUE(session->GetSessionInfo(sessionInfo).IsNone());

    EXPECT_EQ(sessionInfo.slotID, mSlotID);
    EXPECT_EQ(sessionInfo.state & CKS_RW_USER_FUNCTIONS, CKS_RW_USER_FUNCTIONS);
    EXPECT_EQ(sessionInfo.flags & (CKF_RW_SESSION | CKF_SERIAL_SESSION), CKF_RW_SESSION | CKF_SERIAL_SESSION);
    EXPECT_EQ(sessionInfo.ulDeviceError, CKR_OK);
}

TEST_F(PKCS11Test, CreateMultipleSessions)
{
    Error                     err;
    SharedPtr<SessionContext> session1, session2, session3;

    Tie(session1, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // double login failed
    Tie(session2, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_FALSE(err.IsNone());

    // open remaining sessions, but dont log in
    Tie(session2, err) = mSoftHSMEnv.OpenUserSession(mPIN, false);
    ASSERT_TRUE(err.IsNone());

    Tie(session3, err) = mSoftHSMEnv.OpenUserSession(mPIN, false);
    ASSERT_TRUE(err.IsNone());
}

TEST_F(PKCS11Test, GenerateRSAKeyPairWithLabel)
{
    Error                     err;
    SharedPtr<SessionContext> session1, session2;

    Tie(session1, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err) = Utils(mAllocator, session1, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // check key exists in a new session
    Tie(session2, err) = mSoftHSMEnv.OpenUserSession(mPIN, false);
    ASSERT_TRUE(err.IsNone());

    StaticArray<ObjectAttribute, cObjectAttributesCount> templ;
    StaticArray<ObjectHandle, cKeysPerToken>             objects;
    CK_BBOOL                                             cTrue = CK_TRUE;

    templ.EmplaceBack(CKA_TOKEN, Array<uint8_t>(&cTrue, sizeof(cTrue)));

    ASSERT_TRUE(session2->FindObjects(templ, objects).IsNone());
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Contains(key.GetPrivHandle()));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Contains(key.GetPubHandle()));

    // remove key
    err = Utils(mAllocator, session1, *mCryptoProvider).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    ASSERT_TRUE(session2->FindObjects(templ, objects).Is(ErrorEnum::eNotFound));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPrivHandle())));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPubHandle())));
}

TEST_F(PKCS11Test, GenerateECDSAKeyPairWithLabel)
{
    Error                     err;
    SharedPtr<SessionContext> session1, session2;

    Tie(session1, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err)
        = Utils(mAllocator, session1, *mCryptoProvider).GenerateECDSAKeyPairWithLabel(id, mLabel, EllipticCurve::eP384);
    ASSERT_TRUE(err.IsNone());

    // check ECDSA public key params
    const auto&          pubKey           = static_cast<const crypto::ECDSAPublicKey&>(key.GetPrivKey()->GetPublic());
    const auto           actualECParams   = pubKey.GetECParamsOID();
    std::vector<uint8_t> expectedECParams = {0x2b, 0x81, 0x04, 0x00, 0x22};

    EXPECT_THAT(std::vector<uint8_t>(actualECParams.begin(), actualECParams.end()), ElementsAreArray(expectedECParams));

    // check key exists in a new session
    Tie(session2, err) = mSoftHSMEnv.OpenUserSession(mPIN, false);
    ASSERT_TRUE(err.IsNone());

    StaticArray<ObjectAttribute, cObjectAttributesCount> templ;
    StaticArray<ObjectHandle, cKeysPerToken>             objects;
    CK_BBOOL                                             cTrue = CK_TRUE;

    templ.EmplaceBack(CKA_TOKEN, Array<uint8_t>(&cTrue, sizeof(cTrue)));

    ASSERT_TRUE(session2->FindObjects(templ, objects).IsNone());
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Contains(key.GetPrivHandle()));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Contains(key.GetPubHandle()));

    // remove key
    err = Utils(mAllocator, session1, *mCryptoProvider).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    ASSERT_TRUE(session2->FindObjects(templ, objects).Is(ErrorEnum::eNotFound));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPrivHandle())));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPubHandle())));
}

TEST_F(PKCS11Test, FindPrivateKey)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err) = Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // find PrivateKey
    PrivateKey foundKey;
    Tie(foundKey, err) = Utils(mAllocator, session, *mCryptoProvider).FindPrivateKey(id, mLabel);
    ASSERT_TRUE(err.IsNone());

    ASSERT_EQ(key.GetPrivHandle(), foundKey.GetPrivHandle());
    ASSERT_EQ(key.GetPubHandle(), foundKey.GetPubHandle());
    ASSERT_TRUE(key.GetPrivKey()->GetPublic().IsEqual(foundKey.GetPrivKey()->GetPublic()));

    // remove key
    err = Utils(mAllocator, session, *mCryptoProvider).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    Tie(foundKey, err) = Utils(mAllocator, session, *mCryptoProvider).FindPrivateKey(id, mLabel);
    ASSERT_EQ(err, ErrorEnum::eNotFound);
}

TEST_F(PKCS11Test, ImportCertificate)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // import certificate
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, crypto::cCertDERSize> derBlob;
    crypto::x509::Certificate                  caCert;

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/ca.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, caCert).IsNone());

    ASSERT_TRUE(Utils(mAllocator, session, *mCryptoProvider).ImportCertificate(id, mLabel, caCert).IsNone());

    // check certificate exist
    bool hasCertificate = false;

    Tie(hasCertificate, err)
        = Utils(mAllocator, session, *mCryptoProvider).HasCertificate(caCert.mIssuer, caCert.mSerial);
    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(hasCertificate);

    // delete certificate
    err = Utils(mAllocator, session, *mCryptoProvider).DeleteCertificate(id, mLabel);
    ASSERT_TRUE(err.IsNone());

    // check certificate doesn't exist
    Tie(hasCertificate, err)
        = Utils(mAllocator, session, *mCryptoProvider).HasCertificate(caCert.mIssuer, caCert.mSerial);
    ASSERT_TRUE(err.IsNone());
    ASSERT_FALSE(hasCertificate);
}

TEST_F(PKCS11Test, GenPIN)
{
    static constexpr auto cTestPINsNum = 1000;
    static constexpr auto cPINSize     = 20;

    std::vector<StaticString<cPINSize>> pins;

    for (int i = 0; i < cTestPINsNum; i++) {
        StaticString<cPINSize> pin;

        ASSERT_TRUE(GenPIN(*mCryptoProvider, pin).IsNone());

        pins.push_back(pin);
    }

    // check there is no equal PINs
    std::sort(pins.begin(), pins.end());

    StaticString<cPINSize> prevPIN;

    for (const auto& pin : pins) {
        ASSERT_NE(pin, prevPIN);
    }
}

TEST_F(PKCS11Test, FindCertificateChain)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // create ids
    uuid::UUID caId, clientId;

    Tie(caId, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    Tie(clientId, err) = uuid::StringToUUID("00000000-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    // read certificates
    StaticArray<uint8_t, crypto::cCertDERSize> derBlob;
    crypto::x509::Certificate                  caCert, clientCert;

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/ca.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, caCert).IsNone());

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/client.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, clientCert).IsNone());

    // import certificates
    ASSERT_TRUE(Utils(mAllocator, session, *mCryptoProvider).ImportCertificate(caId, mLabel, caCert).IsNone());
    ASSERT_TRUE(Utils(mAllocator, session, *mCryptoProvider).ImportCertificate(clientId, mLabel, clientCert).IsNone());

    // find two certificate chain
    SharedPtr<crypto::x509::CertificateChain> chain;

    Tie(chain, err) = Utils(mAllocator, session, *mCryptoProvider).FindCertificateChain(clientId, mLabel);

    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(chain);

    ASSERT_EQ(chain->Size(), 2);
    ASSERT_EQ((*chain)[0].mSubject, clientCert.mSubject);
    ASSERT_EQ((*chain)[0].mIssuer, clientCert.mIssuer);
    ASSERT_EQ((*chain)[1].mSubject, caCert.mSubject);
    ASSERT_EQ((*chain)[1].mIssuer, caCert.mIssuer);
}

TEST_F(PKCS11Test, FindCertificateChainSelectsIssuerByAKI)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    uuid::UUID caOldId, caNewId, clientId;

    Tie(caOldId, err) = uuid::StringToUUID("11111111-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    Tie(caNewId, err) = uuid::StringToUUID("22222222-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    Tie(clientId, err) = uuid::StringToUUID("33333333-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, crypto::cCertDERSize> derBlob;
    crypto::x509::Certificate                  caOldCert, caNewCert, clientCert;

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/ca_old.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, caOldCert).IsNone());

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/ca.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, caNewCert).IsNone());

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/client.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, clientCert).IsNone());

    ASSERT_EQ(caOldCert.mSubject, caNewCert.mSubject);
    ASSERT_NE(caOldCert.mSubjectKeyId, caNewCert.mSubjectKeyId);
    ASSERT_FALSE(clientCert.mAuthorityKeyId.IsEmpty());
    ASSERT_EQ(clientCert.mAuthorityKeyId, caNewCert.mSubjectKeyId);
    ASSERT_NE(clientCert.mAuthorityKeyId, caOldCert.mSubjectKeyId);

    // Import old CA first so subject search would prefer it without AKI matching.
    ASSERT_TRUE(Utils(mAllocator, session, *mCryptoProvider).ImportCertificate(caOldId, mLabel, caOldCert).IsNone());
    ASSERT_TRUE(Utils(mAllocator, session, *mCryptoProvider).ImportCertificate(caNewId, mLabel, caNewCert).IsNone());
    ASSERT_TRUE(Utils(mAllocator, session, *mCryptoProvider).ImportCertificate(clientId, mLabel, clientCert).IsNone());

    SharedPtr<crypto::x509::CertificateChain> chain;

    Tie(chain, err) = Utils(mAllocator, session, *mCryptoProvider).FindCertificateChain(clientId, mLabel);

    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(chain);
    ASSERT_EQ(chain->Size(), 2);
    ASSERT_EQ((*chain)[0].mSubjectKeyId, clientCert.mSubjectKeyId);
    ASSERT_EQ((*chain)[1].mSubjectKeyId, caNewCert.mSubjectKeyId);
    ASSERT_EQ((*chain)[0].mAuthorityKeyId, (*chain)[1].mSubjectKeyId);
}

TEST_F(PKCS11Test, PKCS11RSAPrivateKeySign)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err) = Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // generate signature
    const std::string         msg = "Hello World";
    StaticArray<uint8_t, 32>  digest;
    StaticArray<uint8_t, 256> signature;

    auto [hash, hashErr] = mHashProvider->CreateHash(crypto::HashEnum::eSHA256);
    ASSERT_TRUE(hashErr.IsNone());

    ASSERT_TRUE(hash->Update(Array<uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.length())).IsNone());
    ASSERT_TRUE(hash->Finalize(digest).IsNone());

    auto privKey = pkcs11key.GetPrivKey();
    ASSERT_TRUE(privKey->Sign(digest, {crypto::HashEnum::eSHA256}, signature).IsNone());

    // verify signature valid
    const auto& pubKey = static_cast<const crypto::RSAPublicKey&>(privKey->GetPublic());

    ASSERT_TRUE(mCryptoFactory.VerifySignature(pubKey, signature, digest));
}

TEST_F(PKCS11Test, PKCS11ECDSAPrivateKeySign)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err)
        = Utils(mAllocator, session, *mCryptoProvider).GenerateECDSAKeyPairWithLabel(id, mLabel, EllipticCurve::eP384);
    ASSERT_TRUE(err.IsNone());

    // generate signature
    auto privKey = pkcs11key.GetPrivKey();

    const std::string msg = "Hello World";

    StaticArray<uint8_t, 32>  digest;
    StaticArray<uint8_t, 256> signature;

    digest.Insert(digest.begin(), reinterpret_cast<const uint8_t*>(&msg.front()),
        reinterpret_cast<const uint8_t*>(&msg.back() + 1));
    ASSERT_TRUE(privKey->Sign(digest, {crypto::HashEnum::eNone}, signature).IsNone());

    // verify signature valid
    const auto& pubKey = static_cast<const crypto::ECDSAPublicKey&>(privKey->GetPublic());

    mCryptoFactory.VerifySignature(pubKey, signature, digest);
}

TEST_F(PKCS11Test, PKCS11RSAPrivateKeyDecrypt)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err) = Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // encrypt message
    const auto& privKey = pkcs11key.GetPrivKey();
    const auto& pubKey  = static_cast<const crypto::RSAPublicKey&>(privKey->GetPublic());

    const std::string sample = "Hello World";

    StaticArray<uint8_t, 32>  msg;
    StaticArray<uint8_t, 256> cipher;

    msg.Insert(msg.begin(), reinterpret_cast<const uint8_t*>(&sample.front()),
        reinterpret_cast<const uint8_t*>(&sample.back() + 1));

    ASSERT_TRUE(mCryptoFactory.Encrypt(pubKey, msg, cipher).IsNone());

    // decrypt message
    StaticArray<uint8_t, 256> result;

    ASSERT_TRUE(
        privKey->Decrypt(cipher, crypto::DecryptionOptions {crypto::PKCS1v15DecryptionOptions {}}, result).IsNone());

    const auto actual   = std::vector<uint8_t>(result.begin(), result.end());
    const auto expected = std::vector<uint8_t>(msg.begin(), msg.end());
    EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST_F(PKCS11Test, PKCS11RSAPrivateKeySignWithDifferentHashes)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err) = Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    auto privKey = pkcs11key.GetPrivKey();

    const std::vector<std::pair<crypto::HashEnum, size_t>> hashes = {{crypto::HashEnum::eSHA1, 20},
        {crypto::HashEnum::eSHA224, 28}, {crypto::HashEnum::eSHA384, 48}, {crypto::HashEnum::eSHA512, 64}};

    for (const auto& [hash, digestSize] : hashes) {
        StaticArray<uint8_t, crypto::cSHA2DigestSize> digest;
        StaticArray<uint8_t, 256>                     signature;

        ASSERT_TRUE(digest.Resize(digestSize, 0xAB).IsNone());

        EXPECT_TRUE(privKey->Sign(digest, {hash}, signature).IsNone());
        EXPECT_EQ(signature.Size(), 256U);
    }

    StaticArray<uint8_t, 32> digest;
    StaticArray<uint8_t, 16> smallSignature;

    ASSERT_TRUE(digest.Resize(32, 0xAB).IsNone());
    EXPECT_FALSE(privKey->Sign(digest, {crypto::HashEnum::eSHA256}, smallSignature).IsNone());
}

TEST_F(PKCS11Test, PKCS11RSAPrivateKeyDecryptOptions)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err) = Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    const auto& privKey = pkcs11key.GetPrivKey();
    const auto& pubKey  = static_cast<const crypto::RSAPublicKey&>(privKey->GetPublic());

    StaticArray<uint8_t, 32>  msg;
    StaticArray<uint8_t, 256> cipher;
    StaticArray<uint8_t, 256> result;

    ASSERT_TRUE(msg.Resize(16, 0x11).IsNone());
    ASSERT_TRUE(mCryptoFactory.Encrypt(pubKey, msg, cipher).IsNone());

    crypto::PKCS1v15DecryptionOptions pkcs1Options;
    pkcs1Options.mKeySize = 16;

    EXPECT_TRUE(
        privKey->Decrypt(cipher, crypto::DecryptionOptions {pkcs1Options}, result).Is(ErrorEnum::eNotSupported));

    for (const auto hash : {crypto::HashEnum::eSHA224, crypto::HashEnum::eSHA512_224, crypto::HashEnum::eSHA512_256}) {
        EXPECT_TRUE(privKey->Decrypt(cipher, crypto::DecryptionOptions {crypto::OAEPDecryptionOptions {hash}}, result)
                        .Is(ErrorEnum::eNotSupported));
    }

    // supported OAEP hashes: cipher is PKCS1v15 encoded, so decryption itself fails in the library
    for (const auto hash : {crypto::HashEnum::eSHA1, crypto::HashEnum::eSHA256, crypto::HashEnum::eSHA384,
             crypto::HashEnum::eSHA512, crypto::HashEnum::eSHA3_224, crypto::HashEnum::eSHA3_256}) {
        auto decryptErr
            = privKey->Decrypt(cipher, crypto::DecryptionOptions {crypto::OAEPDecryptionOptions {hash}}, result);

        EXPECT_FALSE(decryptErr.IsNone());
        EXPECT_FALSE(decryptErr.Is(ErrorEnum::eNotSupported));
    }

    StaticArray<uint8_t, 4> smallResult;

    EXPECT_FALSE(privKey->Decrypt(cipher, crypto::DecryptionOptions {crypto::PKCS1v15DecryptionOptions {}}, smallResult)
                     .IsNone());
}

TEST_F(PKCS11Test, PKCS11ECDSAPrivateKeyDecryptNotSupported)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err)
        = Utils(mAllocator, session, *mCryptoProvider).GenerateECDSAKeyPairWithLabel(id, mLabel, EllipticCurve::eP384);
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, 32> cipher;
    StaticArray<uint8_t, 32> result;

    ASSERT_TRUE(cipher.Resize(32, 0x01).IsNone());

    EXPECT_TRUE(pkcs11key.GetPrivKey()
                    ->Decrypt(cipher, crypto::DecryptionOptions {crypto::PKCS1v15DecryptionOptions {}}, result)
                    .Is(ErrorEnum::eFailed));
}

TEST_F(PKCS11Test, GenerateECDSAKeyPairUnsupportedCurve)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err) = Utils(mAllocator, session, *mCryptoProvider)
                        .GenerateECDSAKeyPairWithLabel(id, mLabel, static_cast<EllipticCurve>(-1));
    EXPECT_TRUE(err.Is(ErrorEnum::eNotSupported));
}

TEST_F(PKCS11Test, SessionErrorsOnInvalidObjects)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    constexpr ObjectHandle cInvalidHandle = 0xFFFF;

    StaticArray<uint8_t, 32>  data;
    StaticArray<uint8_t, 256> output;
    CK_MECHANISM              mechanism = {CKM_RSA_PKCS, nullptr, 0};

    ASSERT_TRUE(data.Resize(32, 0x01).IsNone());

    EXPECT_FALSE(session->Sign(&mechanism, cInvalidHandle, data, output).IsNone());
    EXPECT_FALSE(session->Decrypt(&mechanism, cInvalidHandle, data, output).IsNone());
    EXPECT_FALSE(session->DestroyObject(cInvalidHandle).IsNone());

    EXPECT_FALSE(Utils(mAllocator, session, *mCryptoProvider)
                     .DeletePrivateKey(PrivateKey(cInvalidHandle, cInvalidHandle, nullptr))
                     .IsNone());

    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    EXPECT_TRUE(Utils(mAllocator, session, *mCryptoProvider).DeleteCertificate(id, mLabel).Is(ErrorEnum::eNotFound));

    SharedPtr<crypto::x509::CertificateChain> chain;

    Tie(chain, err) = Utils(mAllocator, session, *mCryptoProvider).FindCertificateChain(id, mLabel);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));
    EXPECT_FALSE(chain);
}

TEST_F(PKCS11Test, FindPrivateKeyWithoutPublicKey)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err) = Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    ASSERT_TRUE(session->DestroyObject(key.GetPubHandle()).IsNone());

    PrivateKey foundKey;

    Tie(foundKey, err) = Utils(mAllocator, session, *mCryptoProvider).FindPrivateKey(id, mLabel);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

    StaticArray<uint8_t, cIDStrLen> longID;

    ASSERT_TRUE(longID.Resize(cIDStrLen, 0x01).IsNone());

    Tie(foundKey, err) = Utils(mAllocator, session, *mCryptoProvider).FindPrivateKey(longID, mLabel);
    EXPECT_FALSE(err.IsNone());

    EXPECT_TRUE(session->DestroyObject(key.GetPrivHandle()).IsNone());
}

TEST_F(PKCS11Test, FindCertificateChainWithoutIssuer)
{
    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    uuid::UUID caOldId, clientId;

    Tie(caOldId, err) = uuid::StringToUUID("11111111-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    Tie(clientId, err) = uuid::StringToUUID("33333333-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, crypto::cCertDERSize> derBlob;
    crypto::x509::Certificate                  caOldCert, clientCert;

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/ca_old.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, caOldCert).IsNone());

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/client.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, clientCert).IsNone());

    Utils utils(mAllocator, session, *mCryptoProvider);

    // only client certificate: issuer is not found neither by subject nor by key id
    ASSERT_TRUE(utils.ImportCertificate(clientId, mLabel, clientCert).IsNone());

    SharedPtr<crypto::x509::CertificateChain> chain;

    Tie(chain, err) = utils.FindCertificateChain(clientId, mLabel);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

    // issuer with the same subject but another key id
    ASSERT_TRUE(utils.ImportCertificate(caOldId, mLabel, caOldCert).IsNone());

    Tie(chain, err) = utils.FindCertificateChain(clientId, mLabel);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

    // the same certificate imported twice: first one is used
    ASSERT_TRUE(utils.ImportCertificate(caOldId, mLabel, caOldCert).IsNone());

    Tie(chain, err) = utils.FindCertificateChain(caOldId, mLabel);
    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(chain);
    EXPECT_EQ(chain->Size(), 1U);
}

TEST_F(PKCS11Test, LibraryContextErrors)
{
    constexpr SlotID cInvalidSlotID = 0xFFFF;

    auto library = mSoftHSMEnv.GetLibrary();

    SlotInfo  slotInfo;
    TokenInfo tokenInfo;

    EXPECT_FALSE(library->GetSlotInfo(cInvalidSlotID, slotInfo).IsNone());
    EXPECT_FALSE(library->GetTokenInfo(cInvalidSlotID, tokenInfo).IsNone());
    EXPECT_FALSE(library->InitToken(cInvalidSlotID, mPIN, mLabel).IsNone());
    EXPECT_FALSE(library->CloseAllSessions(cInvalidSlotID).IsNone());

    StaticString<cLabelLen + 1> longLabel;

    ASSERT_TRUE(longLabel.Resize(cLabelLen).IsNone());
    EXPECT_TRUE(library->InitToken(mSlotID, mPIN, longLabel).Is(ErrorEnum::eNoMemory));

    // SoftHSM always provides an additional empty slot, so one element list is too small
    StaticArray<SlotID, 1> smallSlotList;

    EXPECT_TRUE(library->GetSlotList(false, smallSlotList).Is(ErrorEnum::eNoMemory));

    Error                     err;
    SharedPtr<SessionContext> session;

    Tie(session, err) = library->OpenSession(cInvalidSlotID, CKF_SERIAL_SESSION);
    EXPECT_FALSE(err.IsNone());
    EXPECT_FALSE(session);
}

TEST_F(PKCS11Test, LibraryContextSessionPool)
{
    auto library = mSoftHSMEnv.GetLibrary();

    library->ClearSessions();

    const std::vector<Flags> flagsList = {CKF_SERIAL_SESSION, CKF_SERIAL_SESSION | CKF_RW_SESSION,
        CKF_SERIAL_SESSION | 0x10, CKF_SERIAL_SESSION | CKF_RW_SESSION | 0x10, CKF_SERIAL_SESSION | 0x20};

    std::vector<SharedPtr<SessionContext>> sessions;

    for (const auto flags : flagsList) {
        auto [session, err] = library->OpenSession(mSlotID, flags);
        ASSERT_TRUE(err.IsNone());
        ASSERT_TRUE(session);

        sessions.push_back(session);
    }

    // the last session is in the pool, the first one is evicted
    auto [lastSession, lastErr] = library->OpenSession(mSlotID, flagsList.back());
    ASSERT_TRUE(lastErr.IsNone());
    EXPECT_EQ(lastSession.Get(), sessions.back().Get());

    auto [firstSession, firstErr] = library->OpenSession(mSlotID, flagsList.front());
    ASSERT_TRUE(firstErr.IsNone());
    EXPECT_NE(firstSession.Get(), sessions.front().Get());

    EXPECT_TRUE(library->CloseAllSessions(mSlotID).IsNone());

    library->ClearSessions();
}

TEST_F(PKCS11Test, NotInitializedLibraryContext)
{
    LibraryContext library;

    SlotInfo                slotInfo;
    TokenInfo               tokenInfo;
    LibInfo                 libInfo;
    StaticArray<SlotID, 10> slotList;

    EXPECT_TRUE(library.InitToken(mSlotID, mPIN, mLabel).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(library.GetSlotList(true, slotList).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(library.GetSlotInfo(mSlotID, slotInfo).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(library.GetTokenInfo(mSlotID, tokenInfo).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(library.GetLibInfo(libInfo).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(library.CloseAllSessions(mSlotID).Is(ErrorEnum::eWrongState));

    auto [session, err] = library.OpenSession(mSlotID, CKF_SERIAL_SESSION);
    EXPECT_TRUE(err.Is(ErrorEnum::eWrongState));
    EXPECT_FALSE(session);
}

TEST_F(PKCS11Test, PKCS11ManagerOpenLibraryErrors)
{
    EXPECT_FALSE(mSoftHSMEnv.GetManager().OpenLibrary("/not/existing/library.so"));

    EXPECT_EQ(mSoftHSMEnv.GetManager().OpenLibrary(SOFTHSM2_LIB).Get(), mLibrary.Get());

    PKCS11Manager manager;

    ASSERT_TRUE(manager.Init(mAllocator).IsNone());

    EXPECT_FALSE(manager.OpenLibrary("/not/existing/library.so"));

    EXPECT_FALSE(manager.OpenLibrary("libc.so.6"));
}

TEST_F(PKCS11Test, GenPINRandomError)
{
    RandomStub            random;
    StaticString<cPINLen> pin;

    EXPECT_TRUE(GenPIN(random, pin).Is(ErrorEnum::eFailed));
}

/***********************************************************************************************************************
 * Fake cryptoki tests
 **********************************************************************************************************************/

class PKCS11FakeTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        ASSERT_TRUE(mCryptoFactory.Init(mAllocator).IsNone());
        mCryptoProvider = &mCryptoFactory.GetCryptoProvider();

        FakeCryptoki::Reset();

        mSession = MakeShared<SessionContext>(&mAllocator, cSessionHandle, FakeCryptoki::GetFunctionList());
        ASSERT_TRUE(mSession);
    }

    void TearDown() override
    {
        mSession.Reset();
        FakeCryptoki::Reset();
    }

    Utils GetUtils() { return Utils(mAllocator, mSession, *mCryptoProvider); }

    static constexpr SessionHandle cSessionHandle = 1;

    HeapAllocator                mAllocator;
    crypto::DefaultCryptoFactory mCryptoFactory;
    crypto::CryptoProviderItf*   mCryptoProvider = nullptr;
    SharedPtr<SessionContext>    mSession;
};

TEST_F(PKCS11FakeTest, NoFunctionList)
{
    auto session = MakeShared<SessionContext>(&mAllocator, cSessionHandle, nullptr);
    ASSERT_TRUE(session);

    SessionInfo                                          info;
    StaticArray<AttributeType, 1>                        types;
    StaticArray<Array<uint8_t>, 1>                       values;
    StaticArray<ObjectAttribute, cObjectAttributesCount> templ;
    StaticArray<ObjectHandle, cKeysPerToken>             objects;
    StaticArray<uint8_t, 32>                             data;
    StaticArray<uint8_t, 256>                            output;
    CK_MECHANISM                                         mechanism = {CKM_RSA_PKCS, nullptr, 0};

    EXPECT_TRUE(session->GetSessionInfo(info).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->Login(CKU_USER, "pin").Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->Logout().Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->InitPIN("pin").Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->GetAttributeValues(1, types, values).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->FindObjects(templ, objects).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->CreateObject(templ).mError.Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->DestroyObject(1).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->Sign(&mechanism, 1, data, output).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(session->Decrypt(&mechanism, 1, data, output).Is(ErrorEnum::eWrongState));

    Utils utils(mAllocator, session, *mCryptoProvider);

    StaticArray<uint8_t, 16> id;

    ASSERT_TRUE(id.Resize(16, 0x01).IsNone());

    EXPECT_TRUE(utils.GenerateRSAKeyPairWithLabel(id, "label", 2048).mError.Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(
        utils.GenerateECDSAKeyPairWithLabel(id, "label", EllipticCurve::eP384).mError.Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(utils.DeletePrivateKey(PrivateKey(1, 2, nullptr)).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(utils.DeleteCertificate(id, "label").Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(utils.HasCertificate(id, id).mError.Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(utils.FindCertificateChain(id, "label").mError.Is(ErrorEnum::eWrongState));

    crypto::x509::Certificate cert;

    EXPECT_TRUE(utils.ImportCertificate(id, "label", cert).Is(ErrorEnum::eWrongState));
}

TEST_F(PKCS11FakeTest, LibraryFailures)
{
    SessionInfo                                          info;
    StaticArray<AttributeType, 1>                        types;
    StaticArray<Array<uint8_t>, 1>                       values;
    StaticArray<ObjectAttribute, cObjectAttributesCount> templ;
    StaticArray<ObjectHandle, cKeysPerToken>             objects;
    StaticArray<uint8_t, 32>                             data;
    StaticArray<uint8_t, 256>                            output;
    CK_MECHANISM                                         mechanism = {CKM_RSA_PKCS, nullptr, 0};
    uint8_t                                              value     = 0;

    ASSERT_TRUE(types.PushBack(CKA_CLASS).IsNone());
    ASSERT_TRUE(values.PushBack(Array<uint8_t>(&value, sizeof(value))).IsNone());

    const Error cLibraryError = static_cast<int32_t>(CKR_GENERAL_ERROR);

    EXPECT_EQ(mSession->GetSessionInfo(info), cLibraryError);
    EXPECT_EQ(mSession->Logout(), cLibraryError);
    EXPECT_EQ(mSession->InitPIN("pin"), cLibraryError);
    EXPECT_EQ(mSession->GetAttributeValues(1, types, values), cLibraryError);
    EXPECT_EQ(mSession->FindObjects(templ, objects), cLibraryError);
    EXPECT_EQ(mSession->CreateObject(templ).mError, cLibraryError);
    EXPECT_EQ(mSession->DestroyObject(1), cLibraryError);
    EXPECT_EQ(mSession->Sign(&mechanism, 1, data, output), cLibraryError);
    EXPECT_EQ(mSession->Decrypt(&mechanism, 1, data, output), cLibraryError);

    FakeCryptoki::sSignInitRV    = CKR_OK;
    FakeCryptoki::sDecryptInitRV = CKR_OK;

    EXPECT_EQ(mSession->Sign(&mechanism, 1, data, output), cLibraryError);
    EXPECT_EQ(mSession->Decrypt(&mechanism, 1, data, output), cLibraryError);

    FakeCryptoki::sFindResults   = {{CKR_OK, {1}}};
    FakeCryptoki::sFindObjectsRV = CKR_GENERAL_ERROR;

    EXPECT_EQ(mSession->FindObjects(templ, objects), cLibraryError);

    FakeCryptoki::sFindResults   = {{CKR_OK, {1}}};
    FakeCryptoki::sFindObjectsRV = CKR_OK;
    FakeCryptoki::sFindFinalRV   = CKR_GENERAL_ERROR;

    EXPECT_EQ(mSession->FindObjects(templ, objects), cLibraryError);

    FakeCryptoki::sCloseSessionRV = CKR_GENERAL_ERROR;

    mSession.Reset();
}

TEST_F(PKCS11FakeTest, InvalidArguments)
{
    StaticArray<ObjectAttribute, cObjectAttributesCount + 1> bigTempl;
    StaticArray<ObjectHandle, cKeysPerToken>                 objects;
    uint8_t                                                  value = 0;

    ASSERT_TRUE(bigTempl.Resize(cObjectAttributesCount + 1, ObjectAttribute {CKA_CLASS, {&value, 1}}).IsNone());

    EXPECT_TRUE(mSession->CreateObject(bigTempl).mError.Is(ErrorEnum::eNoMemory));

    FakeCryptoki::sFindResults = {{CKR_OK, {1}}};

    EXPECT_TRUE(mSession->FindObjects(bigTempl, objects).Is(ErrorEnum::eNoMemory));

    StaticArray<AttributeType, cObjectAttributesCount + 1>  types;
    StaticArray<Array<uint8_t>, cObjectAttributesCount + 1> values;

    ASSERT_TRUE(types.PushBack(CKA_CLASS).IsNone());

    EXPECT_TRUE(mSession->GetAttributeValues(1, types, values).Is(ErrorEnum::eInvalidArgument));

    ASSERT_TRUE(types.Resize(cObjectAttributesCount + 1, CKA_CLASS).IsNone());
    ASSERT_TRUE(values.Resize(cObjectAttributesCount + 1, Array<uint8_t>(&value, 1)).IsNone());

    EXPECT_TRUE(mSession->GetAttributeValues(1, types, values).Is(ErrorEnum::eNoMemory));

    ASSERT_TRUE(types.Resize(1).IsNone());
    ASSERT_TRUE(values.Resize(1).IsNone());

    FakeCryptoki::sGetAttributeRV = CKR_OK;
    FakeCryptoki::sObjects[1]     = {};

    EXPECT_TRUE(mSession->GetAttributeValues(1, types, values).Is(ErrorEnum::eInvalidArgument));

    StaticArray<ObjectHandle, 1>                         oneObject;
    StaticArray<ObjectAttribute, cObjectAttributesCount> templ;

    FakeCryptoki::sFindResults = {{CKR_OK, {1, 2}}};

    EXPECT_TRUE(mSession->FindObjects(templ, oneObject).Is(ErrorEnum::eNoMemory));
}

TEST_F(PKCS11FakeTest, GenerateKeyPairErrors)
{
    StaticArray<uint8_t, 16> id;

    ASSERT_TRUE(id.Resize(16, 0x01).IsNone());

    const Error cLibraryError = static_cast<int32_t>(CKR_GENERAL_ERROR);

    EXPECT_EQ(GetUtils().GenerateRSAKeyPairWithLabel(id, "label", 2048).mError, cLibraryError);
    EXPECT_EQ(GetUtils().GenerateECDSAKeyPairWithLabel(id, "label", EllipticCurve::eP384).mError, cLibraryError);

    FakeCryptoki::sGenerateKeyRV = CKR_OK;

    EXPECT_EQ(GetUtils().GenerateRSAKeyPairWithLabel(id, "label", 2048).mError, cLibraryError);
    EXPECT_EQ(GetUtils().GenerateECDSAKeyPairWithLabel(id, "label", EllipticCurve::eP384).mError, cLibraryError);

    FakeCryptoki::sGetAttributeRV = CKR_OK;
    FakeCryptoki::sObjects[FakeCryptoki::cFakePubKeyHandle]
        = {{CKA_ECDSA_PARAMS, {0xFF, 0x00}}, {CKA_EC_POINT, {0x04, 0x02, 0x01, 0x02}}};

    EXPECT_FALSE(GetUtils().GenerateECDSAKeyPairWithLabel(id, "label", EllipticCurve::eP384).mError.IsNone());

    FakeCryptoki::sObjects[FakeCryptoki::cFakePubKeyHandle]
        = {{CKA_ECDSA_PARAMS, {0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x22}}, {CKA_EC_POINT, {0xFF}}};

    EXPECT_FALSE(GetUtils().GenerateECDSAKeyPairWithLabel(id, "label", EllipticCurve::eP384).mError.IsNone());

    // unsupported curve (P256) returned by library
    FakeCryptoki::sObjects[FakeCryptoki::cFakePubKeyHandle]
        = {{CKA_ECDSA_PARAMS, {0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07}},
            {CKA_EC_POINT, {0x04, 0x03, 0x01, 0x02, 0x03}}};

    EXPECT_TRUE(GetUtils()
                    .GenerateECDSAKeyPairWithLabel(id, "label", EllipticCurve::eP384)
                    .mError.Is(ErrorEnum::eNotSupported));
}

TEST_F(PKCS11FakeTest, FindPrivateKeyErrors)
{
    constexpr ObjectHandle cPrivKey = 1;
    constexpr ObjectHandle cPubKey  = 2;

    StaticArray<uint8_t, 16> id;

    ASSERT_TRUE(id.Resize(16, 0x01).IsNone());

    FakeCryptoki::sFindResults = {{CKR_OK, {cPrivKey}}};

    EXPECT_EQ(GetUtils().FindPrivateKey(id, "label").mError, Error(static_cast<int32_t>(CKR_GENERAL_ERROR)));

    FakeCryptoki::sGetAttributeRV    = CKR_OK;
    FakeCryptoki::sObjects[cPrivKey] = {{CKA_KEY_TYPE, ToBytes<CK_KEY_TYPE>(CKK_RSA)}};
    FakeCryptoki::sFindResults       = {{CKR_OK, {cPrivKey}}, {CKR_GENERAL_ERROR, {}}};

    EXPECT_EQ(GetUtils().FindPrivateKey(id, "label").mError, Error(static_cast<int32_t>(CKR_GENERAL_ERROR)));

    FakeCryptoki::sObjects[cPrivKey] = {{CKA_KEY_TYPE, ToBytes<CK_KEY_TYPE>(CKK_DSA)}};
    FakeCryptoki::sFindResults       = {{CKR_OK, {cPrivKey}}, {CKR_OK, {cPubKey}}};

    EXPECT_TRUE(GetUtils().FindPrivateKey(id, "label").mError.Is(ErrorEnum::eNotSupported));
}

TEST_F(PKCS11FakeTest, MissingFunctions)
{
    StaticArray<ObjectAttribute, cObjectAttributesCount> templ;
    StaticArray<ObjectHandle, cKeysPerToken>             objects;
    StaticArray<uint8_t, 32>                             data;
    StaticArray<uint8_t, 256>                            output;
    CK_MECHANISM                                         mechanism = {CKM_RSA_PKCS, nullptr, 0};

    auto functionList = FakeCryptoki::GetFunctionList();

    FakeCryptoki::sRV            = CKR_OK;
    FakeCryptoki::sSignInitRV    = CKR_OK;
    FakeCryptoki::sDecryptInitRV = CKR_OK;

    auto [handle, createErr] = mSession->CreateObject(templ);
    EXPECT_TRUE(createErr.IsNone());
    EXPECT_EQ(handle, CK_INVALID_HANDLE);

    functionList->C_Sign             = nullptr;
    functionList->C_Decrypt          = nullptr;
    functionList->C_FindObjectsFinal = nullptr;

    EXPECT_TRUE(mSession->Sign(&mechanism, 1, data, output).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(mSession->Decrypt(&mechanism, 1, data, output).Is(ErrorEnum::eWrongState));

    FakeCryptoki::sFindResults = {{CKR_OK, {1}}};

    EXPECT_TRUE(mSession->FindObjects(templ, objects).Is(ErrorEnum::eWrongState));

    functionList->C_FindObjects = nullptr;

    FakeCryptoki::sFindResults = {{CKR_OK, {1}}};

    EXPECT_TRUE(mSession->FindObjects(templ, objects).Is(ErrorEnum::eWrongState));
}

TEST_F(PKCS11FakeTest, CertificateErrors)
{
    StaticArray<uint8_t, 16> id;

    ASSERT_TRUE(id.Resize(16, 0x01).IsNone());

    const Error cLibraryError = static_cast<int32_t>(CKR_GENERAL_ERROR);

    StaticArray<uint8_t, crypto::cCertDERSize> derBlob;
    crypto::x509::Certificate                  caCert;

    ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/ca.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, caCert).IsNone());

    EXPECT_EQ(GetUtils().ImportCertificate(id, "", caCert), cLibraryError);

    FakeCryptoki::sFindResults = {{CKR_GENERAL_ERROR, {}}};

    auto [hasCert, hasErr] = GetUtils().HasCertificate(caCert.mIssuer, caCert.mSerial);
    EXPECT_EQ(hasErr, cLibraryError);
    EXPECT_FALSE(hasCert);

    FakeCryptoki::sFindResults = {{CKR_OK, {1}}};

    EXPECT_EQ(GetUtils().DeleteCertificate(id, "label"), cLibraryError);

    FakeCryptoki::sFindResults = {{CKR_OK, {1, 2}}};

    EXPECT_EQ(GetUtils().FindCertificateChain(id, "label").mError, cLibraryError);

    FakeCryptoki::sGetAttributeRV = CKR_OK;

    FakeCryptoki::sObjects[1]  = {{CKA_CLASS, ToBytes<CK_OBJECT_CLASS>(CKO_DATA)}, {CKA_VALUE, {0x01}}};
    FakeCryptoki::sFindResults = {{CKR_OK, {1}}};

    EXPECT_TRUE(GetUtils().FindCertificateChain(id, "label").mError.Is(ErrorEnum::eFailed));

    FakeCryptoki::sObjects[1]  = CertObject({});
    FakeCryptoki::sFindResults = {{CKR_OK, {1}}};

    EXPECT_TRUE(GetUtils().FindCertificateChain(id, "label").mError.Is(ErrorEnum::eFailed));

    FakeCryptoki::sObjects[1]  = CertObject({0x01, 0x02, 0x03});
    FakeCryptoki::sFindResults = {{CKR_OK, {1}}};

    EXPECT_FALSE(GetUtils().FindCertificateChain(id, "label").mError.IsNone());
}

TEST_F(PKCS11FakeTest, FindCertificateChainSkipsInvalidCertificates)
{
    constexpr ObjectHandle cClient  = 1;
    constexpr ObjectHandle cInvalid = 2;
    constexpr ObjectHandle cCA      = 3;

    StaticArray<uint8_t, 16> id;

    ASSERT_TRUE(id.Resize(16, 0x01).IsNone());

    FakeCryptoki::sGetAttributeRV    = CKR_OK;
    FakeCryptoki::sObjects[cClient]  = CertObject(ReadDER(CERTIFICATES_DIR "/client.cer.der"));
    FakeCryptoki::sObjects[cInvalid] = CertObject({0x01, 0x02, 0x03});
    FakeCryptoki::sObjects[cCA]      = CertObject(ReadDER(CERTIFICATES_DIR "/ca.cer.der"));

    // issuer found by subject, invalid candidate is skipped
    FakeCryptoki::sFindResults = {{CKR_OK, {cClient}}, {CKR_OK, {cInvalid, cCA}}};

    auto [chain, err] = GetUtils().FindCertificateChain(id, "label");
    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(chain);
    EXPECT_EQ(chain->Size(), 2U);

    // issuer not found by subject, found by key id, invalid candidate is skipped
    FakeCryptoki::sFindResults = {{CKR_OK, {cClient}}, {CKR_OK, {}}, {CKR_OK, {cInvalid, cClient, cCA}}};

    Tie(chain, err) = GetUtils().FindCertificateChain(id, "label");
    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(chain);
    EXPECT_EQ(chain->Size(), 2U);

    FakeCryptoki::sFindResults = {{CKR_OK, {cClient}}, {CKR_GENERAL_ERROR, {}}};

    Tie(chain, err) = GetUtils().FindCertificateChain(id, "label");
    EXPECT_EQ(err, Error(static_cast<int32_t>(CKR_GENERAL_ERROR)));

    FakeCryptoki::sFindResults = {{CKR_OK, {cClient}}, {CKR_OK, {}}, {CKR_GENERAL_ERROR, {}}};

    Tie(chain, err) = GetUtils().FindCertificateChain(id, "label");
    EXPECT_EQ(err, Error(static_cast<int32_t>(CKR_GENERAL_ERROR)));
}

} // namespace aos::pkcs11
