/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/crypto/certloader.hpp>
#include <core/common/pkcs11/pkcs11.hpp>
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/crypto/softhsmenv.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>

namespace aos::crypto {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

using namespace testing;

class CertloaderTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        ASSERT_TRUE(fs::WriteStringToFile(mPINSource, mPIN, 0664).IsNone());

        ASSERT_TRUE(mCryptoFactory.Init(mAllocator).IsNone());
        mCryptoProvider = &mCryptoFactory.GetCryptoProvider();

        ASSERT_TRUE(mSoftHSMEnv.Init(mAllocator, mPIN, mLabel).IsNone());

        ASSERT_TRUE(
            mCertLoader.Init(mAllocator, mCryptoFactory.GetCryptoProvider(), mSoftHSMEnv.GetManager()).IsNone());

        mLibrary = mSoftHSMEnv.GetLibrary();
        mSlotID  = mSoftHSMEnv.GetSlotID();
    }

    void TearDown() override { ASSERT_TRUE(fs::Remove(mPINSource).IsNone()); }

    void ImportCertificateChainToPKCS11(const Array<uint8_t>& caID, const Array<uint8_t>& clientID)
    {
        Error                             err;
        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
        ASSERT_TRUE(err.IsNone());

        // read certificates
        StaticArray<uint8_t, cCertDERSize> derBlob;
        x509::Certificate                  caCert, clientCert;

        ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/ca.cer.der", derBlob).IsNone());
        ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, caCert).IsNone());

        ASSERT_TRUE(fs::ReadFile(CERTIFICATES_DIR "/client.cer.der", derBlob).IsNone());
        ASSERT_TRUE(mCryptoProvider->DERToX509Cert(derBlob, clientCert).IsNone());

        // import certificates
        ASSERT_TRUE(
            pkcs11::Utils(mAllocator, session, *mCryptoProvider).ImportCertificate(caID, mLabel, caCert).IsNone());
        ASSERT_TRUE(pkcs11::Utils(mAllocator, session, *mCryptoProvider)
                        .ImportCertificate(clientID, mLabel, clientCert)
                        .IsNone());
    }

    // Provisions a non-extractable CKO_SECRET_KEY (AES) object: the posture LoadPrivKeyByURL/FindPrivateKey
    // actually search for (alongside CKO_PRIVATE_KEY), and the only one production provisioning is expected
    // to create. The key's own value is never read back by anything under test here (crypto::PrivateKeyItf
    // never exposes it).
    void WriteSecretKeyObject(const Array<uint8_t>& id, const String& label, const Array<uint8_t>& value)
    {
        Error                             err = ErrorEnum::eNone;
        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
        ASSERT_TRUE(err.IsNone());

        CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
        CK_KEY_TYPE     keyType  = CKK_AES;
        CK_BBOOL        trueVal  = CK_TRUE;
        CK_BBOOL        falseVal = CK_FALSE;

        StaticArray<pkcs11::ObjectAttribute, 10> templ;

        auto pushBytes = [&](pkcs11::AttributeType type, const void* data, size_t size) {
            ASSERT_TRUE(
                templ.PushBack({type, Array<uint8_t>(reinterpret_cast<uint8_t*>(const_cast<void*>(data)), size)})
                    .IsNone());
        };

        pushBytes(CKA_CLASS, &keyClass, sizeof(keyClass));
        pushBytes(CKA_KEY_TYPE, &keyType, sizeof(keyType));
        pushBytes(CKA_TOKEN, &trueVal, sizeof(trueVal));
        pushBytes(CKA_PRIVATE, &trueVal, sizeof(trueVal));
        pushBytes(CKA_EXTRACTABLE, &falseVal, sizeof(falseVal));
        pushBytes(CKA_SENSITIVE, &trueVal, sizeof(trueVal));
        pushBytes(CKA_ID, id.Get(), id.Size());
        pushBytes(CKA_LABEL, label.Get(), label.Size());
        ASSERT_TRUE(
            templ.PushBack({CKA_VALUE, Array<uint8_t>(const_cast<uint8_t*>(value.Get()), value.Size())}).IsNone());

        pkcs11::ObjectHandle handle    = 0;
        Error                createErr = ErrorEnum::eNone;

        Tie(handle, createErr) = session->CreateObject(templ);
        ASSERT_TRUE(createErr.IsNone());
    }

    void GeneratePrivateKey(const Array<uint8_t>& id)
    {
        Error                             err;
        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
        ASSERT_TRUE(err.IsNone());

        pkcs11::PrivateKey key;

        Tie(key, err)
            = pkcs11::Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
        ASSERT_TRUE(err.IsNone());
    }

    static constexpr auto mLabel     = "cryptoutils";
    static constexpr auto mPIN       = "admin";
    static constexpr auto mPINSource = "pin.txt";

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

    DefaultCryptoFactory mCryptoFactory;
    CryptoProviderItf*   mCryptoProvider = nullptr;
    test::SoftHSMEnv     mSoftHSMEnv;

    pkcs11::SlotID                    mSlotID = 0;
    SharedPtr<pkcs11::LibraryContext> mLibrary;
    CertLoader                        mCertLoader;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CertloaderTest, ParseScheme)
{
    const char* url1 = "pkcs11:token=aoscore;object=diskencryption;id=2e2769b6-be2c-43ff-b16d-25985a04e6b2?module-path="
                       "/usr/lib/softhsm/libsofthsm2.so";
    const char* url2 = "file:/usr/share/.ssh/rsa.pub";
    const char* url3 = "file/usr/share/.ssh/rsa.pub";

    StaticString<30> scheme;

    ASSERT_EQ(ParseURLScheme(url1, scheme), ErrorEnum::eNone);
    EXPECT_EQ(scheme, "pkcs11");

    ASSERT_EQ(ParseURLScheme(url2, scheme), ErrorEnum::eNone);
    EXPECT_EQ(scheme, "file");

    ASSERT_EQ(ParseURLScheme(url3, scheme), ErrorEnum::eNotFound);
}

TEST_F(CertloaderTest, ParseFileURL)
{
    const char* url1 = "file:/usr/share/.ssh/rsa.pub";
    const char* url2 = "pkcs11:token=aoscore";

    StaticString<cFilePathLen> path;

    ASSERT_EQ(ParseFileURL(url1, path), ErrorEnum::eNone);
    EXPECT_EQ(path, "/usr/share/.ssh/rsa.pub");

    ASSERT_NE(ParseFileURL(url2, path), ErrorEnum::eNone);
}

TEST_F(CertloaderTest, ParsePKCS11URLAllValues)
{
    const auto url1 = "pkcs11:token=aoscore;object=diskencryption;id=%00%01%02%03%04%05%06%07?module-path="
                      "/usr/lib/softhsm/libsofthsm2.so&pin-source="
        + std::string(mPINSource);

    StaticString<cFilePathLen>            library;
    StaticString<pkcs11::cLabelLen>       token;
    StaticString<pkcs11::cLabelLen>       label;
    StaticArray<uint8_t, pkcs11::cIDSize> id;
    StaticString<pkcs11::cPINLen>         userPIN;
    uint8_t                               expectedID[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

    ASSERT_EQ(ParsePKCS11URL(url1.c_str(), library, token, label, id, userPIN), ErrorEnum::eNone);

    EXPECT_EQ(library, "/usr/lib/softhsm/libsofthsm2.so");
    EXPECT_EQ(token, "aoscore");
    EXPECT_EQ(label, "diskencryption");
    EXPECT_EQ(userPIN, mPIN);
    EXPECT_EQ(id, Array(expectedID, ArraySize(expectedID)));
}

TEST_F(CertloaderTest, ParsePKCS11URLRequiredValuesOnly)
{
    const char* url1 = "pkcs11:object=diskencryption;id=%AA%BB%CC";

    StaticString<cFilePathLen>      library;
    StaticString<pkcs11::cLabelLen> token;
    StaticString<pkcs11::cLabelLen> label;
    uuid::UUID                      id;
    StaticString<pkcs11::cPINLen>   userPIN;
    uint8_t                         expectedID[] = {0xAA, 0xBB, 0xCC};

    ASSERT_EQ(ParsePKCS11URL(url1, library, token, label, id, userPIN), ErrorEnum::eNone);

    EXPECT_EQ(library, "");
    EXPECT_EQ(token, "");
    EXPECT_EQ(label, "diskencryption");
    EXPECT_EQ(userPIN, "");
    EXPECT_EQ(id, Array(expectedID, ArraySize(expectedID)));
}

TEST_F(CertloaderTest, ParsePKCS11URLPinValue)
{
    const auto url = "pkcs11:token=aoscore;object=diskencryption;id=%00%01%02%03%04%05%06%07?module-path="
                     "/usr/lib/softhsm/libsofthsm2.so&pin-value="
        + std::string(mPIN);

    StaticString<cFilePathLen>      library;
    StaticString<pkcs11::cLabelLen> token;
    StaticString<pkcs11::cLabelLen> label;
    uuid::UUID                      id;
    StaticString<pkcs11::cPINLen>   userPIN;

    ASSERT_EQ(ParsePKCS11URL(url.c_str(), library, token, label, id, userPIN), ErrorEnum::eNone);

    EXPECT_EQ(userPIN, mPIN);
}

TEST_F(CertloaderTest, ParsePKCS11URLPinSource)
{
    const auto url = "pkcs11:token=aoscore;object=diskencryption;id=%00%01%02%03%04%05%06%07?module-path="
                     "/usr/lib/softhsm/libsofthsm2.so&pin-source="
        + std::string(mPINSource);

    StaticString<cFilePathLen>      library;
    StaticString<pkcs11::cLabelLen> token;
    StaticString<pkcs11::cLabelLen> label;
    uuid::UUID                      id;
    StaticString<pkcs11::cPINLen>   userPIN;

    ASSERT_EQ(ParsePKCS11URL(url.c_str(), library, token, label, id, userPIN), ErrorEnum::eNone);

    EXPECT_EQ(userPIN, mPIN);
}

TEST_F(CertloaderTest, ParsePKCS11URLPinValueAndPinSource)
{
    const auto url = "pkcs11:token=aoscore;object=diskencryption;id=%00%01%02%03%04%05%06%07?module-path="
                     "/usr/lib/softhsm/libsofthsm2.so&pin-source="
        + std::string(mPINSource) + "&pin-value=" + std::string(mPIN);

    StaticString<cFilePathLen>      library;
    StaticString<pkcs11::cLabelLen> token;
    StaticString<pkcs11::cLabelLen> label;
    uuid::UUID                      id;
    StaticString<pkcs11::cPINLen>   userPIN;

    ASSERT_NE(ParsePKCS11URL(url.c_str(), library, token, label, id, userPIN), ErrorEnum::eNone);
}

TEST_F(CertloaderTest, FindPKCS11CertificateChain)
{
    constexpr uint8_t caID[]     = {0x00, 0x01, 0x02};
    constexpr uint8_t clientID[] = {0x00, 0x01, 0x03};

    ImportCertificateChainToPKCS11(Array(caID, ArraySize(caID)), Array(clientID, ArraySize(clientID)));

    const auto url = "pkcs11:token=cryptoutils;object=cryptoutils;id=%00%01%03?module-"
                     "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    SharedPtr<x509::CertificateChain> chain;
    Error                             error;
    Tie(chain, error) = mCertLoader.LoadCertsChainByURL(url.c_str());
    ASSERT_TRUE(error.IsNone());
    ASSERT_TRUE(chain);
    ASSERT_EQ(chain->Size(), 2);

    // check client certificate
    StaticString<cCertSubjSize> subject;
    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN((*chain)[0].mSubject, subject).IsNone());
    EXPECT_EQ(std::string(subject.CStr()), std::string("CN=Aos Core"));

    StaticString<cCertIssuerSize> issuer;
    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN((*chain)[0].mIssuer, issuer).IsNone());
    EXPECT_EQ(std::string(issuer.CStr()), std::string("CN=Aos Cloud"));

    // check CA certificate
    EXPECT_EQ((*chain)[1].mSubject, (*chain)[1].mIssuer);

    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN((*chain)[1].mIssuer, issuer).IsNone());
    EXPECT_EQ(std::string(issuer.CStr()), std::string("CN=Aos Cloud"));
}

TEST_F(CertloaderTest, FindPKCS11CertificateChainBadURL)
{
    constexpr uint8_t caID[]     = {0x00, 0x01, 0x02};
    constexpr uint8_t clientID[] = {0x00, 0x01, 0x03};

    ImportCertificateChainToPKCS11(Array(caID, ArraySize(caID)), Array(clientID, ArraySize(clientID)));

    const auto url = "pkcs11:token=cryptoutils;object=cryptoutils;id=%00%01%04?module-"
                     "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    SharedPtr<x509::CertificateChain> chain;
    Error                             error;
    Tie(chain, error) = mCertLoader.LoadCertsChainByURL(url.c_str());
    ASSERT_TRUE(error.Is(ErrorEnum::eNotFound));
}

TEST_F(CertloaderTest, FindPKCS11PrivateKey)
{
    constexpr uint8_t id[] = {0xAA, 0xBB, 0xCC};

    GeneratePrivateKey(Array(id, ArraySize(id)));

    const auto url = "pkcs11:token=cryptoutils;object=cryptoutils;id=%AA%BB%CC?module-"
                     "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    SharedPtr<PrivateKeyItf> privKey;
    Error                    error;
    Tie(privKey, error) = mCertLoader.LoadPrivKeyByURL(url.c_str());
    ASSERT_TRUE(error.IsNone());
    ASSERT_TRUE(privKey);
}

TEST_F(CertloaderTest, FindPKCS11PrivateKeyBadURL)
{
    constexpr uint8_t id[] = {0xAA, 0xBB, 0xCC};

    GeneratePrivateKey(Array(id, ArraySize(id)));

    const auto url = "pkcs11:token=cryptoutils;object=cryptoutils;id=%AA%BB%FF?module-"
                     "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    SharedPtr<PrivateKeyItf> privKey;
    Error                    error;
    Tie(privKey, error) = mCertLoader.LoadPrivKeyByURL(url.c_str());
    ASSERT_TRUE(error.Is(ErrorEnum::eNotFound));
}

TEST_F(CertloaderTest, FindCertificatesFromFile)
{
    const char* url = "file:" CERTIFICATES_DIR "/client-ca-chain.pem";

    SharedPtr<x509::CertificateChain> chain;
    Error                             error;
    Tie(chain, error) = mCertLoader.LoadCertsChainByURL(url);
    ASSERT_TRUE(error.IsNone());
    ASSERT_TRUE(chain);
    ASSERT_EQ(chain->Size(), 2);

    // check client certificate
    StaticString<cCertSubjSize> subject;
    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN((*chain)[0].mSubject, subject).IsNone());
    EXPECT_EQ(std::string(subject.CStr()), std::string("CN=Aos Core"));

    StaticString<cCertIssuerSize> issuer;
    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN((*chain)[0].mIssuer, issuer).IsNone());
    EXPECT_EQ(std::string(issuer.CStr()), std::string("CN=Aos Cloud"));

    // check CA certificate
    EXPECT_EQ((*chain)[1].mSubject, (*chain)[1].mIssuer);

    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN((*chain)[1].mIssuer, issuer).IsNone());
    EXPECT_EQ(std::string(issuer.CStr()), std::string("CN=Aos Cloud"));
}

TEST_F(CertloaderTest, FindPKCS11SecretKey)
{
    constexpr uint8_t id[]    = {0xDD, 0xEE, 0xFF};
    constexpr uint8_t value[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};

    WriteSecretKeyObject(Array(id, ArraySize(id)), "aos-layer-key", Array(value, ArraySize(value)));

    const auto url = "pkcs11:token=cryptoutils;object=aos-layer-key;id=%DD%EE%FF?module-path=" SOFTHSM2_LIB
                     "&pin-source="
        + std::string(mPINSource);

    auto [key, err] = mCertLoader.LoadPrivKeyByURL(url.c_str());

    // the key's own value is never exposed: success and a non-null handle is all there is to check here.
    // Actually decrypting with it is covered by the pkcs11/sm/imagemanager round-trip tests.
    ASSERT_TRUE(err.IsNone());
    EXPECT_TRUE(key);
}

TEST_F(CertloaderTest, FindPKCS11SecretKeyNotFound)
{
    constexpr uint8_t id[]    = {0xDD, 0xEE, 0xFF};
    constexpr uint8_t value[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};

    WriteSecretKeyObject(Array(id, ArraySize(id)), "aos-layer-key", Array(value, ArraySize(value)));

    // matches neither the secret key above (wrong label) nor any RSA/ECDSA key pair.
    const auto url = "pkcs11:token=cryptoutils;object=no-such-label;id=%DD%EE%FF?module-path=" SOFTHSM2_LIB
                     "&pin-source="
        + std::string(mPINSource);

    auto [key, err] = mCertLoader.LoadPrivKeyByURL(url.c_str());

    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));
    EXPECT_FALSE(key);
}

} // namespace aos::crypto
