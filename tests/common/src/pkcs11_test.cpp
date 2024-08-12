/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>
#include <gmock/gmock.h>
#include <mbedtls/sha256.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"
#include "aos/common/pkcs11/privatekey.hpp"
#include "aos/common/tools/allocator.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/uuid.hpp"
#include "softhsmenv.hpp"

#include "log.hpp"

using namespace testing;

namespace aos {
namespace pkcs11 {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class PKCS11Test : public Test {
protected:
    void SetUp() override
    {
        InitLog();

        ASSERT_TRUE(mCryptoProvider.Init().IsNone());
        ASSERT_TRUE(mSoftHSMEnv.Init(mPIN, mLabel).IsNone());

        mLibrary = mSoftHSMEnv.GetLibrary();
        mSlotID  = mSoftHSMEnv.GetSlotID();
    }

    static constexpr auto mLabel = "iam pkcs11 test slot";
    static constexpr auto mPIN   = "admin";

    crypto::MbedTLSCryptoProvider mCryptoProvider;
    test::SoftHSMEnv              mSoftHSMEnv;

    SlotID                    mSlotID = 0;
    SharedPtr<LibraryContext> mLibrary;

    StaticAllocator<Max(2 * sizeof(pkcs11::PKCS11RSAPrivateKey), sizeof(pkcs11::PKCS11ECDSAPrivateKey),
                        2 * sizeof(crypto::x509::Certificate) + sizeof(crypto::x509::CertificateChain)
                            + 2 * sizeof(pkcs11::PKCS11RSAPrivateKey))
        + pkcs11::Utils::cLocalObjectsMaxSize>
        mAllocator;
};

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static void ImportRSAPublicKey(const crypto::RSAPublicKey& rsaKey, mbedtls_pk_context& ctx)
{
    const auto& n = rsaKey.GetN();
    const auto& e = rsaKey.GetE();

    ASSERT_EQ(mbedtls_pk_setup(&ctx, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)), 0);

    mbedtls_rsa_context* rsaCtx = mbedtls_pk_rsa(ctx);

    ASSERT_EQ(mbedtls_rsa_import_raw(rsaCtx, n.Get(), n.Size(), nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0), 0);
    ASSERT_EQ(mbedtls_rsa_import_raw(rsaCtx, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, e.Get(), e.Size()), 0);
    ASSERT_EQ(mbedtls_rsa_complete(rsaCtx), 0);
    ASSERT_EQ(mbedtls_rsa_check_pubkey(rsaCtx), 0);
}

static void ImportECDSAPublicKey(const crypto::ECDSAPublicKey& ecdsaKey, mbedtls_pk_context& ctx)
{
    // We can ignore ECPARAMS as we support SECP384R1 curve only
    const auto& ecParams = ecdsaKey.GetECParamsOID();
    (void)ecParams;

    const auto& ecPoint = ecdsaKey.GetECPoint();

    ASSERT_EQ(mbedtls_pk_setup(&ctx, mbedtls_pk_info_from_type(MBEDTLS_PK_ECDSA)), 0);

    mbedtls_ecdsa_context* ecdsaCtx = mbedtls_pk_ec(ctx);

    mbedtls_ecdsa_init(ecdsaCtx);
    ASSERT_EQ(mbedtls_ecp_group_load(&ecdsaCtx->private_grp, MBEDTLS_ECP_DP_SECP384R1), 0);

    // read EC point
    ASSERT_EQ(
        mbedtls_ecp_point_read_binary(&ecdsaCtx->private_grp, &ecdsaCtx->private_Q, ecPoint.Get(), ecPoint.Size()), 0);
}

static bool VerifySHA256RSASignature(
    const crypto::RSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
{
    mbedtls_pk_context pubKeyCtx;

    mbedtls_pk_init(&pubKeyCtx);

    ImportRSAPublicKey(pubKey, pubKeyCtx);

    int ret = mbedtls_pk_verify(
        &pubKeyCtx, MBEDTLS_MD_SHA256, digest.Get(), digest.Size(), signature.Get(), signature.Size());

    mbedtls_pk_free(&pubKeyCtx);

    return ret == 0;
}

static bool Encrypt(const crypto::RSAPublicKey& pubKey, const Array<uint8_t>& msg, Array<uint8_t>& cipher)
{
    mbedtls_pk_context pubKeyCtx;
    mbedtls_pk_init(&pubKeyCtx);

    ImportRSAPublicKey(pubKey, pubKeyCtx);

    // setup entropy
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    const char*              pers = "mbedtls_pk_encrypt";

    mbedtls_ctr_drbg_init(&ctrDrbg);
    mbedtls_entropy_init(&entropy);

    int ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));

    if (ret != 0) {
        mbedtls_pk_free(&pubKeyCtx);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctrDrbg);

        return false;
    }

    // encrypt
    size_t len = 0;

    cipher.Resize(cipher.MaxSize());

    ret = mbedtls_pk_encrypt(
        &pubKeyCtx, msg.Get(), msg.Size(), cipher.Get(), &len, cipher.Size(), mbedtls_ctr_drbg_random, &ctrDrbg);

    cipher.Resize(len);

    mbedtls_pk_free(&pubKeyCtx);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctrDrbg);

    return ret == 0;
}

static void VerifyECDSASignature(
    const crypto::ECDSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
{
    mbedtls_pk_context pubKeyCtx;

    mbedtls_pk_init(&pubKeyCtx);

    ImportECDSAPublicKey(pubKey, pubKeyCtx);

    mbedtls_ecdsa_context* ecdsaCtx = mbedtls_pk_ec(pubKeyCtx);

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    size_t rsLen = signature.Size() / 2;

    ASSERT_EQ(mbedtls_mpi_read_binary(&r, signature.Get(), rsLen), 0);
    ASSERT_EQ(mbedtls_mpi_read_binary(&s, signature.Get() + rsLen, rsLen), 0);

    ASSERT_EQ(
        mbedtls_ecdsa_verify(&ecdsaCtx->private_grp, digest.Get(), digest.Size(), &ecdsaCtx->private_Q, &r, &s), 0);

    mbedtls_pk_free(&pubKeyCtx);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
}

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

    Error                     err = ErrorEnum::eNone;
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
    Error                     err = ErrorEnum::eNone;
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
    Error                     err = ErrorEnum::eNone;
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
    Error                     err = ErrorEnum::eNone;
    SharedPtr<SessionContext> session1, session2;

    Tie(session1, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err) = Utils(session1, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
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
    err = Utils(session1, mCryptoProvider, mAllocator).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    ASSERT_TRUE(session2->FindObjects(templ, objects).Is(ErrorEnum::eNotFound));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPrivHandle())));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPubHandle())));
}

TEST_F(PKCS11Test, GenerateECDSAKeyPairWithLabel)
{
    Error                     err = ErrorEnum::eNone;
    SharedPtr<SessionContext> session1, session2;

    Tie(session1, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err)
        = Utils(session1, mCryptoProvider, mAllocator).GenerateECDSAKeyPairWithLabel(id, mLabel, EllipticCurve::eP384);
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
    err = Utils(session1, mCryptoProvider, mAllocator).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    ASSERT_TRUE(session2->FindObjects(templ, objects).Is(ErrorEnum::eNotFound));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPrivHandle())));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPubHandle())));
}

TEST_F(PKCS11Test, FindPrivateKey)
{
    Error                     err = ErrorEnum::eNone;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err) = Utils(session, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // find PrivateKey
    PrivateKey foundKey;
    Tie(foundKey, err) = Utils(session, mCryptoProvider, mAllocator).FindPrivateKey(id, mLabel);
    ASSERT_TRUE(err.IsNone());

    ASSERT_EQ(key.GetPrivHandle(), foundKey.GetPrivHandle());
    ASSERT_EQ(key.GetPubHandle(), foundKey.GetPubHandle());
    ASSERT_TRUE(key.GetPrivKey()->GetPublic().IsEqual(foundKey.GetPrivKey()->GetPublic()));

    // remove key
    err = Utils(session, mCryptoProvider, mAllocator).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    Tie(foundKey, err) = Utils(session, mCryptoProvider, mAllocator).FindPrivateKey(id, mLabel);
    ASSERT_EQ(err, ErrorEnum::eNotFound);
}

TEST_F(PKCS11Test, ImportCertificate)
{
    Error                     err = ErrorEnum::eNone;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // import certificate
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, crypto::cCertDERSize> derBlob;
    crypto::x509::Certificate                  caCert;

    ASSERT_TRUE(FS::ReadFile(CERTIFICATES_DIR "/ca.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider.DERToX509Cert(derBlob, caCert).IsNone());

    ASSERT_TRUE(Utils(session, mCryptoProvider, mAllocator).ImportCertificate(id, mLabel, caCert).IsNone());

    // check certificate exist
    bool hasCertificate = false;

    Tie(hasCertificate, err)
        = Utils(session, mCryptoProvider, mAllocator).HasCertificate(caCert.mIssuer, caCert.mSerial);
    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(hasCertificate);

    // delete certificate
    err = Utils(session, mCryptoProvider, mAllocator).DeleteCertificate(id, mLabel);
    ASSERT_TRUE(err.IsNone());

    // check certificate doesn't exist
    Tie(hasCertificate, err)
        = Utils(session, mCryptoProvider, mAllocator).HasCertificate(caCert.mIssuer, caCert.mSerial);
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

        ASSERT_TRUE(GenPIN(pin).IsNone());

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
    Error                     err = ErrorEnum::eNone;
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

    ASSERT_TRUE(FS::ReadFile(CERTIFICATES_DIR "/ca.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider.DERToX509Cert(derBlob, caCert).IsNone());

    ASSERT_TRUE(FS::ReadFile(CERTIFICATES_DIR "/client.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider.DERToX509Cert(derBlob, clientCert).IsNone());

    // import certificates
    ASSERT_TRUE(Utils(session, mCryptoProvider, mAllocator).ImportCertificate(caId, mLabel, caCert).IsNone());
    ASSERT_TRUE(Utils(session, mCryptoProvider, mAllocator).ImportCertificate(clientId, mLabel, clientCert).IsNone());

    // find two certificate chain
    SharedPtr<crypto::x509::CertificateChain> chain;

    Tie(chain, err) = Utils(session, mCryptoProvider, mAllocator).FindCertificateChain(clientId, mLabel);

    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(chain);

    ASSERT_EQ(chain->Size(), 2);
    ASSERT_EQ((*chain)[0].mSubject, clientCert.mSubject);
    ASSERT_EQ((*chain)[0].mIssuer, clientCert.mIssuer);
    ASSERT_EQ((*chain)[1].mSubject, caCert.mSubject);
    ASSERT_EQ((*chain)[1].mIssuer, caCert.mIssuer);
}

TEST_F(PKCS11Test, PKCS11RSAPrivateKeySign)
{
    Error                     err = ErrorEnum::eNone;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err) = Utils(session, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // generate signature
    auto privKey = pkcs11key.GetPrivKey();

    const std::string msg = "Hello World";

    StaticArray<uint8_t, 32> digest;

    digest.Resize(digest.MaxSize());
    mbedtls_sha256(reinterpret_cast<const uint8_t*>(msg.data()), msg.length(), digest.Get(), 0);

    StaticArray<uint8_t, 256> signature;

    ASSERT_TRUE(privKey->Sign(digest, {crypto::HashEnum::eSHA256}, signature).IsNone());

    // verify signature valid
    const auto& pubKey = static_cast<const crypto::RSAPublicKey&>(privKey->GetPublic());

    ASSERT_TRUE(VerifySHA256RSASignature(pubKey, signature, digest));
}

TEST_F(PKCS11Test, PKCS11ECDSAPrivateKeySign)
{
    Error                     err = ErrorEnum::eNone;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err)
        = Utils(session, mCryptoProvider, mAllocator).GenerateECDSAKeyPairWithLabel(id, mLabel, EllipticCurve::eP384);
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

    VerifyECDSASignature(pubKey, signature, digest);
}

TEST_F(PKCS11Test, PKCS11RSAPrivateKeyDecrypt)
{
    Error                     err = ErrorEnum::eNone;
    SharedPtr<SessionContext> session;

    Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey pkcs11key;

    Tie(pkcs11key, err) = Utils(session, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // encrypt message
    const auto& privKey = pkcs11key.GetPrivKey();
    const auto& pubKey  = static_cast<const crypto::RSAPublicKey&>(privKey->GetPublic());

    const std::string sample = "Hello World";

    StaticArray<uint8_t, 32>  msg;
    StaticArray<uint8_t, 256> cipher;

    msg.Insert(msg.begin(), reinterpret_cast<const uint8_t*>(&sample.front()),
        reinterpret_cast<const uint8_t*>(&sample.back() + 1));

    ASSERT_TRUE(Encrypt(pubKey, msg, cipher));

    // decrypt message
    StaticArray<uint8_t, 256> result;

    ASSERT_TRUE(privKey->Decrypt(cipher, result).IsNone());

    const auto actual   = std::vector<uint8_t>(result.begin(), result.end());
    const auto expected = std::vector<uint8_t>(msg.begin(), msg.end());
    EXPECT_THAT(actual, ElementsAreArray(expected));
}

} // namespace pkcs11
} // namespace aos
