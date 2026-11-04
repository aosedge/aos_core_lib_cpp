/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <fstream>
#include <gmock/gmock.h>
#include <memory>
#include <vector>

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
 * Helpers
 **********************************************************************************************************************/

using Bytes = std::vector<uint8_t>;

constexpr auto cBlockSize = crypto::AESCipherItf::cBlockSize;
constexpr auto cIVSize    = crypto::AESCipherItf::cGCMIVSize;
constexpr auto cTagSize   = crypto::AESCipherItf::cGCMTagSize;

Bytes Pattern(size_t size, uint8_t seed)
{
    Bytes result(size);

    for (size_t i = 0; i < size; i++) {
        result[i] = static_cast<uint8_t>(seed + i * 31);
    }

    return result;
}

// Supplies data in fixed-size chunks, optionally failing on the given (1-based) chunk.
class VectorChunkProvider : public crypto::ChunkProviderItf {
public:
    VectorChunkProvider(const Bytes& data, size_t chunkSize, size_t failOnChunk = 0)
        : mData(data)
        , mChunkSize(chunkSize)
        , mFailOnChunk(failOnChunk)
    {
    }

    RetWithError<Array<uint8_t>> NextChunk() override
    {
        if (mOffset == mData.size()) {
            return {Array<uint8_t>(), ErrorEnum::eEOF};
        }

        if (++mChunks == mFailOnChunk) {
            return {Array<uint8_t>(), ErrorEnum::eFailed};
        }

        auto size = std::min(mChunkSize, mData.size() - mOffset);
        auto data = Array<uint8_t>(mData.data() + mOffset, size);

        mOffset += size;

        return {data, ErrorEnum::eNone};
    }

private:
    const Bytes& mData;
    size_t       mChunkSize;
    size_t       mFailOnChunk;
    size_t       mOffset = 0;
    size_t       mChunks = 0;
};

// Collects decrypted chunks, optionally failing on the given (1-based) chunk.
class VectorChunkReceiver : public crypto::ChunkReceiverItf {
public:
    explicit VectorChunkReceiver(size_t bufferSize, size_t failOnChunk = 0)
        : mStorage(bufferSize)
        , mBuffer(mStorage.data(), bufferSize)
        , mFailOnChunk(failOnChunk)
    {
    }

    Array<uint8_t>& GetBuffer() override { return mBuffer; }

    Error OnChunk(const Array<uint8_t>& chunk) override
    {
        if (++mChunks == mFailOnChunk) {
            return ErrorEnum::eFailed;
        }

        mResult.insert(mResult.end(), chunk.begin(), chunk.end());

        return ErrorEnum::eNone;
    }

    const Bytes& GetResult() const { return mResult; }

private:
    Bytes          mStorage;
    Array<uint8_t> mBuffer;
    size_t         mFailOnChunk;
    size_t         mChunks = 0;
    Bytes          mResult;
};

// Emulates OP-TEE's PKCS11 TA on top of SoftHSM: CKM_AES_CTR is rejected at C_DecryptInit unless ulCounterBits is
// 1, which then behaves like a whole-block counter. With sCorruptCounterCheck, the second block of every two-block
// C_Decrypt is corrupted, as a spec-compliant token that honors a 1-bit counter would produce.
CK_FUNCTION_LIST_PTR sRealFunctions       = nullptr;
bool                 sCorruptCounterCheck = false;
size_t               sCounterChecks       = 0;

CK_RV OPTEEDecryptInit(CK_SESSION_HANDLE session, CK_MECHANISM_PTR mechanism, CK_OBJECT_HANDLE key)
{
    if (mechanism == nullptr || mechanism->mechanism != CKM_AES_CTR || mechanism->pParameter == nullptr) {
        return sRealFunctions->C_DecryptInit(session, mechanism, key);
    }

    auto params = *static_cast<CK_AES_CTR_PARAMS*>(mechanism->pParameter);

    if (params.ulCounterBits != 1) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    params.ulCounterBits = cBlockSize * 8;

    CK_MECHANISM realMechanism {CKM_AES_CTR, &params, sizeof(params)};

    return sRealFunctions->C_DecryptInit(session, &realMechanism, key);
}

CK_RV OPTEEDecrypt(
    CK_SESSION_HANDLE session, CK_BYTE_PTR encrypted, CK_ULONG encryptedLen, CK_BYTE_PTR data, CK_ULONG_PTR dataLen)
{
    auto rv = sRealFunctions->C_Decrypt(session, encrypted, encryptedLen, data, dataLen);

    if (rv == CKR_OK && data != nullptr && *dataLen == cBlockSize * 2) {
        sCounterChecks++;

        if (sCorruptCounterCheck) {
            data[cBlockSize] ^= 0x01;
        }
    }

    return rv;
}

// Fault injection for object lookup: when set, C_FindObjectsInit/C_GetAttributeValue fail with the given CK_RV.
// With sTruncateAttributeValue, C_GetAttributeValue succeeds but reports a value shorter than the attribute's type.
CK_RV sFindObjectsInitRV      = CKR_OK;
CK_RV sGetAttributeValueRV    = CKR_OK;
bool  sTruncateAttributeValue = false;

CK_RV FaultyFindObjectsInit(CK_SESSION_HANDLE session, CK_ATTRIBUTE_PTR templ, CK_ULONG count)
{
    if (sFindObjectsInitRV != CKR_OK) {
        return sFindObjectsInitRV;
    }

    return sRealFunctions->C_FindObjectsInit(session, templ, count);
}

CK_RV FaultyGetAttributeValue(
    CK_SESSION_HANDLE session, CK_OBJECT_HANDLE object, CK_ATTRIBUTE_PTR templ, CK_ULONG count)
{
    if (sGetAttributeValueRV != CKR_OK) {
        return sGetAttributeValueRV;
    }

    auto rv = sRealFunctions->C_GetAttributeValue(session, object, templ, count);

    if (rv == CKR_OK && sTruncateAttributeValue && count != 0) {
        templ[0].ulValueLen /= 2;
    }

    return rv;
}

// A token refusing CKM_AES_CTR with this key altogether.
CK_RV RefusingDecryptInit(CK_SESSION_HANDLE session, CK_MECHANISM_PTR mechanism, CK_OBJECT_HANDLE key)
{
    if (mechanism != nullptr && mechanism->mechanism == CKM_AES_CTR) {
        return CKR_KEY_FUNCTION_NOT_PERMITTED;
    }

    return sRealFunctions->C_DecryptInit(session, mechanism, key);
}

// Fault injection on top of SoftHSM: when set, C_DecryptUpdate/C_DecryptFinal fail with the given CK_RV.
CK_RV sDecryptUpdateRV = CKR_OK;
CK_RV sDecryptFinalRV  = CKR_OK;

CK_RV FaultyDecryptUpdate(
    CK_SESSION_HANDLE session, CK_BYTE_PTR encrypted, CK_ULONG encryptedLen, CK_BYTE_PTR data, CK_ULONG_PTR dataLen)
{
    if (sDecryptUpdateRV != CKR_OK) {
        return sDecryptUpdateRV;
    }

    return sRealFunctions->C_DecryptUpdate(session, encrypted, encryptedLen, data, dataLen);
}

// Emulates PKCS#11 3.0 on top of SoftHSM (2.40): C_DecryptInit with a NULL mechanism terminates the active
// decryption operation.
// When set, C_DecryptInit with a mechanism fails with sDecryptInitRV.
size_t sDecryptCancels = 0;
CK_RV  sDecryptInitRV  = CKR_OK;

CK_RV CancellingDecryptInit(CK_SESSION_HANDLE session, CK_MECHANISM_PTR mechanism, CK_OBJECT_HANDLE key)
{
    if (mechanism != nullptr) {
        if (sDecryptInitRV != CKR_OK) {
            return sDecryptInitRV;
        }

        return sRealFunctions->C_DecryptInit(session, mechanism, key);
    }

    sDecryptCancels++;

    Bytes    discard(4096);
    CK_ULONG size = discard.size();

    (void)sRealFunctions->C_DecryptFinal(session, discard.data(), &size);

    return CKR_OK;
}

CK_RV FaultyDecryptFinal(CK_SESSION_HANDLE session, CK_BYTE_PTR data, CK_ULONG_PTR dataLen)
{
    if (sDecryptFinalRV != CKR_OK) {
        return sDecryptFinalRV;
    }

    return sRealFunctions->C_DecryptFinal(session, data, dataLen);
}

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

    // Encrypts plain with AES-GCM in software under the key ImportSecretKey creates: ciphertext followed by the tag.
    Bytes GCMEncrypt(const Bytes& key, const Bytes& iv, const Bytes& plain)
    {
        auto [cipher, err] = mCryptoProvider->CreateAESEncoder(
            "GCM", Array<uint8_t>(key.data(), key.size()), Array<uint8_t>(iv.data(), iv.size()));
        EXPECT_TRUE(err.IsNone());

        if (!err.IsNone()) {
            return {};
        }

        auto  out = std::make_unique<StaticArray<uint8_t, 4096>>();
        Bytes result;

        for (size_t offset = 0; offset < plain.size(); offset += out->MaxSize()) {
            auto size = std::min(out->MaxSize(), plain.size() - offset);

            EXPECT_TRUE(cipher->EncryptBlock(Array<uint8_t>(plain.data() + offset, size), *out).IsNone());
            result.insert(result.end(), out->begin(), out->end());
        }

        EXPECT_TRUE(cipher->Finalize(*out).IsNone());
        result.insert(result.end(), out->begin(), out->end());

        StaticArray<uint8_t, cTagSize> tag;

        EXPECT_TRUE(cipher->GetTag(tag).IsNone());
        result.insert(result.end(), tag.begin(), tag.end());

        return result;
    }

    // Value of the key ImportSecretKey creates.
    static Bytes SecretKeyValue(size_t keySize)
    {
        Bytes value(keySize);

        for (size_t i = 0; i < keySize; i++) {
            value[i] = static_cast<uint8_t>(i + 1);
        }

        return value;
    }

    // CTR options for decrypting the payload of a GCM ciphertext: counter block IV || 0x00000002.
    static crypto::DecryptionOptions PayloadCTROptions(const Bytes& iv)
    {
        crypto::CTRDecryptionOptions ctr;

        EXPECT_TRUE(ctr.mCounter.Assign(Array<uint8_t>(iv.data(), iv.size())).IsNone());

        for (auto byte : {0x00, 0x00, 0x00, 0x02}) {
            EXPECT_TRUE(ctr.mCounter.PushBack(static_cast<uint8_t>(byte)).IsNone());
        }

        return crypto::DecryptionOptions {ctr};
    }

    // Imports a 32-byte secret key and returns it, wrapped as AESPrivateKey, along with the GCM ciphertext (without
    // tag) of plain under it.
    void PrepareAESKey(const SharedPtr<SessionContext>& session, const String& label, const Bytes& iv,
        const Bytes& plain, PrivateKey& key, Bytes& cipher)
    {
        constexpr uint8_t cID[] = {0x01, 0x02, 0x03};
        const auto        id    = Array<uint8_t>(cID, ArraySize(cID));

        ASSERT_TRUE(ImportSecretKey(session, id, label, AESPrivateKey::cKeySize).mError.IsNone());

        Error err;

        Tie(key, err) = Utils(mAllocator, session, *mCryptoProvider).FindPrivateKey(id, label);
        ASSERT_TRUE(err.IsNone());

        cipher = GCMEncrypt(SecretKeyValue(AESPrivateKey::cKeySize), iv, plain);
        ASSERT_EQ(cipher.size(), plain.size() + cTagSize);

        cipher.resize(plain.size());
    }

    // Imports a non-extractable AES CKO_SECRET_KEY object of the given size, the way the layer key is provisioned.
    RetWithError<ObjectHandle> ImportSecretKey(
        const SharedPtr<SessionContext>& session, const Array<uint8_t>& id, const String& label, size_t keySize)
    {
        CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
        CK_KEY_TYPE     keyType  = CKK_AES;
        CK_BBOOL        trueVal  = CK_TRUE;
        CK_BBOOL        falseVal = CK_FALSE;

        auto value = SecretKeyValue(keySize);

        StaticArray<ObjectAttribute, cObjectAttributesCount> templ;

        auto push = [&](AttributeType type, const void* data, size_t size) {
            (void)templ.PushBack({type, Array<uint8_t>(reinterpret_cast<uint8_t*>(const_cast<void*>(data)), size)});
        };

        push(CKA_CLASS, &keyClass, sizeof(keyClass));
        push(CKA_KEY_TYPE, &keyType, sizeof(keyType));
        push(CKA_TOKEN, &trueVal, sizeof(trueVal));
        push(CKA_PRIVATE, &trueVal, sizeof(trueVal));
        push(CKA_EXTRACTABLE, &falseVal, sizeof(falseVal));
        push(CKA_SENSITIVE, &trueVal, sizeof(trueVal));
        push(CKA_DECRYPT, &trueVal, sizeof(trueVal));
        push(CKA_ID, id.Get(), id.Size());
        push(CKA_LABEL, label.Get(), label.Size());
        push(CKA_VALUE, value.data(), value.size());

        return session->CreateObject(templ);
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

TEST_F(PKCS11Test, FindAndDeleteSecretKey)
{
    auto [session, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    constexpr uint8_t cID[] = {0x0A, 0x0B, 0x0C};
    const auto        id    = Array<uint8_t>(cID, ArraySize(cID));

    ASSERT_TRUE(ImportSecretKey(session, id, "secret key", AESPrivateKey::cKeySize).mError.IsNone());

    Utils utils(mAllocator, session, *mCryptoProvider);

    auto [key, findErr] = utils.FindPrivateKey(id, "secret key");
    ASSERT_TRUE(findErr.IsNone());
    EXPECT_EQ(key.GetPubHandle(), CK_INVALID_HANDLE);

    // a secret key has no public object: deleting it must not try to destroy one.
    ASSERT_TRUE(utils.DeletePrivateKey(key).IsNone());

    EXPECT_TRUE(utils.FindPrivateKey(id, "secret key").mError.Is(ErrorEnum::eNotFound));
}

TEST_F(PKCS11Test, FindSecretKeyRejectsNon256BitKey)
{
    auto [session, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    constexpr uint8_t cID[] = {0x0D, 0x0E, 0x0F};
    const auto        id    = Array<uint8_t>(cID, ArraySize(cID));

    ASSERT_TRUE(ImportSecretKey(session, id, "aes128 key", 16).mError.IsNone());

    EXPECT_TRUE(Utils(mAllocator, session, *mCryptoProvider)
                    .FindPrivateKey(id, "aes128 key")
                    .mError.Is(ErrorEnum::eInvalidArgument));
}

TEST_F(PKCS11Test, FindSecretKeyRejectsDuplicates)
{
    auto [session, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    constexpr uint8_t cID[] = {0x0A, 0x0A, 0x0A};
    const auto        id    = Array<uint8_t>(cID, ArraySize(cID));

    ASSERT_TRUE(ImportSecretKey(session, id, "duplicate key", AESPrivateKey::cKeySize).mError.IsNone());
    ASSERT_TRUE(ImportSecretKey(session, id, "duplicate key", AESPrivateKey::cKeySize).mError.IsNone());

    EXPECT_TRUE(Utils(mAllocator, session, *mCryptoProvider)
                    .FindPrivateKey(id, "duplicate key")
                    .mError.Is(ErrorEnum::eInvalidArgument));
}

TEST_F(PKCS11Test, FindSecretKeyReturnsTokenErrors)
{
    auto [userSession, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    constexpr uint8_t cID[] = {0x0B, 0x0B, 0x0B};
    const auto        id    = Array<uint8_t>(cID, ArraySize(cID));

    ASSERT_TRUE(ImportSecretKey(userSession, id, "lookup key", AESPrivateKey::cKeySize).mError.IsNone());

    sRealFunctions = userSession->GetFunctionList();

    CK_FUNCTION_LIST faultyFunctions = *sRealFunctions;

    faultyFunctions.C_FindObjectsInit   = FaultyFindObjectsInit;
    faultyFunctions.C_GetAttributeValue = FaultyGetAttributeValue;

    CK_SESSION_HANDLE handle = CK_INVALID_HANDLE;

    ASSERT_EQ(
        sRealFunctions->C_OpenSession(mSlotID, CKF_SERIAL_SESSION | CKF_RW_SESSION, nullptr, nullptr, &handle), CKR_OK);

    auto session = MakeShared<SessionContext>(&mAllocator, handle, &faultyFunctions);
    ASSERT_TRUE(session);

    Utils utils(mAllocator, session, *mCryptoProvider);

    // the secret key search itself fails: that's returned, not mistaken for "no secret key, try a key pair".
    sFindObjectsInitRV = CKR_DEVICE_ERROR;

    EXPECT_EQ(utils.FindPrivateKey(id, "lookup key").mError.Errno(), static_cast<int32_t>(CKR_DEVICE_ERROR));

    // the key is found, but its size can't be read.
    sFindObjectsInitRV   = CKR_OK;
    sGetAttributeValueRV = CKR_DEVICE_ERROR;

    EXPECT_EQ(utils.FindPrivateKey(id, "lookup key").mError.Errno(), static_cast<int32_t>(CKR_DEVICE_ERROR));

    // the size is read, but isn't a CK_ULONG.
    sGetAttributeValueRV    = CKR_OK;
    sTruncateAttributeValue = true;

    EXPECT_TRUE(utils.FindPrivateKey(id, "lookup key").mError.Is(ErrorEnum::eFailed));

    sTruncateAttributeValue = false;

    EXPECT_TRUE(utils.FindPrivateKey(id, "lookup key").mError.IsNone());
}

TEST_F(PKCS11Test, AESMechConverterRejectsUnsupportedOptions)
{
    PKCS11AESMechConverter aesConverter(cBlockSize * 8);

    EXPECT_TRUE(crypto::DecryptionOptions {crypto::PKCS1v15DecryptionOptions {}}
                    .ApplyVisitor(aesConverter)
                    .mError.Is(ErrorEnum::eNotSupported));
    EXPECT_TRUE(crypto::DecryptionOptions {crypto::OAEPDecryptionOptions {}}
                    .ApplyVisitor(aesConverter)
                    .mError.Is(ErrorEnum::eNotSupported));

    crypto::GCMDecryptionOptions shortIV;

    ASSERT_TRUE(shortIV.mIV.Resize(cIVSize - 1).IsNone());
    EXPECT_TRUE(crypto::DecryptionOptions {shortIV}.ApplyVisitor(aesConverter).mError.Is(ErrorEnum::eInvalidArgument));

    crypto::CTRDecryptionOptions shortCounter;

    ASSERT_TRUE(shortCounter.mCounter.Resize(cBlockSize - 1).IsNone());
    EXPECT_TRUE(
        crypto::DecryptionOptions {shortCounter}.ApplyVisitor(aesConverter).mError.Is(ErrorEnum::eInvalidArgument));

    PCKS11RSAMechConverter rsaConverter;

    EXPECT_TRUE(crypto::DecryptionOptions {crypto::GCMDecryptionOptions {}}
                    .ApplyVisitor(rsaConverter)
                    .mError.Is(ErrorEnum::eNotSupported));
    EXPECT_TRUE(crypto::DecryptionOptions {crypto::CTRDecryptionOptions {}}
                    .ApplyVisitor(rsaConverter)
                    .mError.Is(ErrorEnum::eNotSupported));
}

TEST_F(PKCS11Test, AESPrivateKeyHasNoPublicPartAndDoesNotSign)
{
    auto [session, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    const auto iv = Pattern(cIVSize, 1);
    PrivateKey key;
    Bytes      cipher;

    PrepareAESKey(session, "no public part", iv, Pattern(16, 2), key, cipher);

    const auto& privKey = *key.GetPrivKey();

    EXPECT_EQ(privKey.GetPublic().GetKeyType(), crypto::KeyType {});
    EXPECT_FALSE(privKey.GetPublic().IsEqual(privKey.GetPublic()));

    StaticArray<uint8_t, 32>  digest;
    StaticArray<uint8_t, 512> signature;

    EXPECT_TRUE(privKey.Sign(digest, crypto::SignOptions {}, signature).Is(ErrorEnum::eNotSupported));

    // RSA decryption options don't apply to it either, single-shot or streamed.
    const auto rsaOptions = crypto::DecryptionOptions {crypto::PKCS1v15DecryptionOptions {}};

    VectorChunkProvider provider(cipher, 16, 1);
    VectorChunkReceiver receiver(16);

    EXPECT_TRUE(privKey.Decrypt(Array<uint8_t>(cipher.data(), cipher.size()), rsaOptions, signature)
                    .Is(ErrorEnum::eNotSupported));
    EXPECT_TRUE(privKey.StreamDecrypt(provider, rsaOptions, receiver).Is(ErrorEnum::eNotSupported));
}

TEST_F(PKCS11Test, AESPrivateKeyDecryptsGCM)
{
    auto [session, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    const auto iv    = Pattern(cIVSize, 3);
    const auto plain = Pattern(100, 4);
    PrivateKey key;
    Bytes      cipher;

    PrepareAESKey(session, "gcm key", iv, plain, key, cipher);

    auto sealed = GCMEncrypt(SecretKeyValue(AESPrivateKey::cKeySize), iv, plain);

    crypto::GCMDecryptionOptions gcm;

    ASSERT_TRUE(gcm.mIV.Assign(Array<uint8_t>(iv.data(), iv.size())).IsNone());

    StaticArray<uint8_t, 256> result;

    ASSERT_TRUE(key.GetPrivKey()
                    ->Decrypt(Array<uint8_t>(sealed.data(), sealed.size()), crypto::DecryptionOptions {gcm}, result)
                    .IsNone());
    EXPECT_EQ(Bytes(result.begin(), result.end()), plain);

    // a modified tag fails authentication.
    sealed.back() ^= 0x01;

    EXPECT_FALSE(key.GetPrivKey()
                     ->Decrypt(Array<uint8_t>(sealed.data(), sealed.size()), crypto::DecryptionOptions {gcm}, result)
                     .IsNone());
}

TEST_F(PKCS11Test, AESPrivateKeyStreamDecryptsCTR)
{
    auto [session, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    const auto iv    = Pattern(cIVSize, 5);
    const auto plain = Pattern(5000, 6);
    PrivateKey key;
    Bytes      cipher;

    PrepareAESKey(session, "ctr key", iv, plain, key, cipher);

    VectorChunkProvider provider(cipher, 1024);
    VectorChunkReceiver receiver(1024);

    ASSERT_TRUE(key.GetPrivKey()->StreamDecrypt(provider, PayloadCTROptions(iv), receiver).IsNone());
    EXPECT_EQ(receiver.GetResult(), plain);
}

TEST_F(PKCS11Test, AESPrivateKeyStreamDecryptAbortsOperationOnError)
{
    auto [session, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    const auto iv    = Pattern(cIVSize, 7);
    const auto plain = Pattern(3000, 8);
    PrivateKey key;
    Bytes      cipher;

    PrepareAESKey(session, "abort key", iv, plain, key, cipher);

    const auto& privKey = *key.GetPrivKey();

    // after every failure the operation must have been terminated: the next one on the same session succeeds.
    auto decryptsAgain = [&]() {
        VectorChunkProvider provider(cipher, 1024);
        VectorChunkReceiver receiver(1024);

        EXPECT_TRUE(privKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).IsNone());
        EXPECT_EQ(receiver.GetResult(), plain);
    };

    {
        VectorChunkProvider provider(cipher, 1024, 2);
        VectorChunkReceiver receiver(1024);

        EXPECT_TRUE(privKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).Is(ErrorEnum::eFailed));
    }

    decryptsAgain();

    {
        VectorChunkProvider provider(cipher, 1024);
        VectorChunkReceiver receiver(1024, 2);

        EXPECT_TRUE(privKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).Is(ErrorEnum::eFailed));
    }

    decryptsAgain();

    {
        // a token error other than "not supported" is returned as is.
        VectorChunkProvider provider(cipher, 1024);
        VectorChunkReceiver receiver(cBlockSize);

        auto streamErr = privKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver);

        EXPECT_EQ(streamErr.Errno(), static_cast<int32_t>(CKR_BUFFER_TOO_SMALL));
    }

    decryptsAgain();
}

TEST_F(PKCS11Test, StreamDecryptIsNotSupportedByRSAKey)
{
    auto [session, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-343434343434");
    ASSERT_TRUE(err.IsNone());

    auto [key, keyErr] = Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(keyErr.IsNone());

    const Bytes         data(16);
    VectorChunkProvider provider(data, 16);
    VectorChunkReceiver receiver(16);

    EXPECT_TRUE(
        key.GetPrivKey()
            ->StreamDecrypt(provider, crypto::DecryptionOptions {crypto::PKCS1v15DecryptionOptions {}}, receiver)
            .Is(ErrorEnum::eNotSupported));
}

TEST_F(PKCS11Test, AESPrivateKeyFallsBackToOPTEECounterBits)
{
    auto [userSession, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    const auto iv    = Pattern(cIVSize, 9);
    const auto plain = Pattern(2000, 10);
    PrivateKey key;
    Bytes      cipher;

    PrepareAESKey(userSession, "optee key", iv, plain, key, cipher);

    sRealFunctions       = userSession->GetFunctionList();
    sCorruptCounterCheck = false;
    sCounterChecks       = 0;

    CK_FUNCTION_LIST opteeFunctions = *sRealFunctions;

    opteeFunctions.C_DecryptInit = OPTEEDecryptInit;
    opteeFunctions.C_Decrypt     = OPTEEDecrypt;

    CK_SESSION_HANDLE handle = CK_INVALID_HANDLE;

    ASSERT_EQ(
        sRealFunctions->C_OpenSession(mSlotID, CKF_SERIAL_SESSION | CKF_RW_SESSION, nullptr, nullptr, &handle), CKR_OK);

    auto session = MakeShared<SessionContext>(&mAllocator, handle, &opteeFunctions);
    ASSERT_TRUE(session);

    AESPrivateKey opteeKey(session, key.GetPrivHandle());

    for (int i = 0; i < 2; i++) {
        VectorChunkProvider provider(cipher, 512);
        VectorChunkReceiver receiver(512);

        ASSERT_TRUE(opteeKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).IsNone());
        EXPECT_EQ(receiver.GetResult(), plain);
    }

    // a single-shot decrypt uses the same, cached, counter width.
    StaticArray<uint8_t, 64> result;

    ASSERT_TRUE(
        opteeKey.Decrypt(Array<uint8_t>(cipher.data(), result.MaxSize()), PayloadCTROptions(iv), result).IsNone());
    EXPECT_EQ(Bytes(result.begin(), result.end()), Bytes(plain.begin(), plain.begin() + result.MaxSize()));

    // the whole-block counter check runs once and is cached.
    EXPECT_EQ(sCounterChecks, 1U);

    // a token that really treats ulCounterBits = 1 as a 1-bit counter is rejected rather than decrypting garbage.
    sCorruptCounterCheck = true;

    AESPrivateKey       compliantKey(session, key.GetPrivHandle());
    VectorChunkProvider provider(cipher, 512);
    VectorChunkReceiver receiver(512);

    EXPECT_TRUE(compliantKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).Is(ErrorEnum::eNotSupported));
    EXPECT_TRUE(receiver.GetResult().empty());
}

TEST_F(PKCS11Test, AESPrivateKeyReturnsCTRProbeErrorBeforeConsumingData)
{
    auto [userSession, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    const auto iv    = Pattern(cIVSize, 13);
    const auto plain = Pattern(500, 14);
    PrivateKey key;
    Bytes      cipher;

    PrepareAESKey(userSession, "refused key", iv, plain, key, cipher);

    sRealFunctions = userSession->GetFunctionList();

    CK_FUNCTION_LIST refusingFunctions = *sRealFunctions;

    refusingFunctions.C_DecryptInit = RefusingDecryptInit;

    CK_SESSION_HANDLE handle = CK_INVALID_HANDLE;

    ASSERT_EQ(
        sRealFunctions->C_OpenSession(mSlotID, CKF_SERIAL_SESSION | CKF_RW_SESSION, nullptr, nullptr, &handle), CKR_OK);

    auto session = MakeShared<SessionContext>(&mAllocator, handle, &refusingFunctions);
    ASSERT_TRUE(session);

    AESPrivateKey       aesKey(session, key.GetPrivHandle());
    VectorChunkProvider provider(cipher, 256, 1);
    VectorChunkReceiver receiver(256);

    // the counter width probe fails with an unrelated error: it is returned as is, with no fallback and before the
    // provider is asked for anything (it would fail on its first chunk).
    auto streamErr = aesKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver);

    EXPECT_EQ(streamErr.Errno(), static_cast<int32_t>(CKR_KEY_FUNCTION_NOT_PERMITTED));
    EXPECT_TRUE(receiver.GetResult().empty());
}

TEST_F(PKCS11Test, DecryptMultiPartReportsNotSupportedAndAbortsOperation)
{
    auto [userSession, err] = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    const auto iv    = Pattern(cIVSize, 11);
    const auto plain = Pattern(1000, 12);
    PrivateKey key;
    Bytes      cipher;

    PrepareAESKey(userSession, "faulty key", iv, plain, key, cipher);

    sRealFunctions = userSession->GetFunctionList();

    CK_FUNCTION_LIST faultyFunctions = *sRealFunctions;

    faultyFunctions.C_DecryptInit   = CancellingDecryptInit;
    faultyFunctions.C_DecryptUpdate = FaultyDecryptUpdate;
    faultyFunctions.C_DecryptFinal  = FaultyDecryptFinal;

    sDecryptCancels = 0;

    CK_SESSION_HANDLE handle = CK_INVALID_HANDLE;

    ASSERT_EQ(
        sRealFunctions->C_OpenSession(mSlotID, CKF_SERIAL_SESSION | CKF_RW_SESSION, nullptr, nullptr, &handle), CKR_OK);

    auto session = MakeShared<SessionContext>(&mAllocator, handle, &faultyFunctions);
    ASSERT_TRUE(session);

    AESPrivateKey aesKey(session, key.GetPrivHandle());

    // a token that can't do multi-part decryption for the mechanism is reported as such; the abort, where
    // C_DecryptFinal keeps failing with CKR_BUFFER_TOO_SMALL, falls back to C_DecryptInit with no mechanism.
    sDecryptUpdateRV = CKR_FUNCTION_NOT_SUPPORTED;
    sDecryptFinalRV  = CKR_BUFFER_TOO_SMALL;

    {
        VectorChunkProvider provider(cipher, 256);
        VectorChunkReceiver receiver(256);

        EXPECT_TRUE(aesKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).Is(ErrorEnum::eNotSupported));
    }

    EXPECT_EQ(sDecryptCancels, 1U);

    sDecryptUpdateRV = CKR_OK;
    sDecryptFinalRV  = CKR_OK;

    {
        VectorChunkProvider provider(cipher, 256);
        VectorChunkReceiver receiver(256);

        EXPECT_TRUE(aesKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).IsNone());
        EXPECT_EQ(receiver.GetResult(), plain);
    }

    // the operation can't even be started: nothing is read from the provider.
    sDecryptInitRV = CKR_DEVICE_ERROR;

    {
        VectorChunkProvider provider(cipher, 256, 1);
        VectorChunkReceiver receiver(256);

        EXPECT_EQ(aesKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).Errno(),
            static_cast<int32_t>(CKR_DEVICE_ERROR));
    }

    sDecryptInitRV = CKR_OK;

    // a C_DecryptFinal failure other than "not supported" is returned as is.
    sDecryptFinalRV = CKR_DEVICE_ERROR;

    {
        VectorChunkProvider provider(cipher, 256);
        VectorChunkReceiver receiver(256);

        EXPECT_EQ(aesKey.StreamDecrypt(provider, PayloadCTROptions(iv), receiver).Errno(),
            static_cast<int32_t>(CKR_DEVICE_ERROR));
    }

    sDecryptFinalRV = CKR_OK;

    // the faked failure left SoftHSM's operation active: terminate it before the session is closed.
    EXPECT_EQ(faultyFunctions.C_DecryptInit(handle, nullptr, CK_INVALID_HANDLE), CKR_OK);
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

} // namespace aos::pkcs11
