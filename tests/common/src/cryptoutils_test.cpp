/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"
#include "aos/common/cryptoutils.hpp"
#include "log.hpp"
#include "softhsmenv.hpp"

namespace aos {
namespace cryptoutils {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

using namespace testing;

class CryptoutilsTest : public Test {
protected:
    void SetUp() override
    {
        InitLogs();

        ASSERT_TRUE(mCryptoProvider.Init().IsNone());
        ASSERT_TRUE(mSoftHSMEnv.Init(mPIN, mLabel).IsNone());

        ASSERT_TRUE(mCertLoader.Init(mCryptoProvider, mSoftHSMEnv.GetManager()).IsNone());

        mLibrary = mSoftHSMEnv.GetLibrary();
        mSlotID  = mSoftHSMEnv.GetSlotID();
    }

    void ImportCertificateChainToPKCS11(const String& caId, const String& clientId)
    {
        Error                             err = ErrorEnum::eNone;
        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
        ASSERT_TRUE(err.IsNone());

        // create ids
        uuid::UUID caUUID, clientUUID;

        Tie(caUUID, err) = uuid::StringToUUID(caId);
        ASSERT_TRUE(err.IsNone());

        Tie(clientUUID, err) = uuid::StringToUUID(clientId);
        ASSERT_TRUE(err.IsNone());

        // read certificates
        StaticArray<uint8_t, crypto::cCertDERSize> derBlob;
        crypto::x509::Certificate                  caCert, clientCert;

        ASSERT_TRUE(FS::ReadFile(CERTIFICATES_DIR "/ca.cer.der", derBlob).IsNone());
        ASSERT_TRUE(mCryptoProvider.DERToX509Cert(derBlob, caCert).IsNone());

        ASSERT_TRUE(FS::ReadFile(CERTIFICATES_DIR "/client.cer.der", derBlob).IsNone());
        ASSERT_TRUE(mCryptoProvider.DERToX509Cert(derBlob, clientCert).IsNone());

        // import certificates
        ASSERT_TRUE(
            pkcs11::Utils(session, mCryptoProvider, mAllocator).ImportCertificate(caUUID, mLabel, caCert).IsNone());
        ASSERT_TRUE(pkcs11::Utils(session, mCryptoProvider, mAllocator)
                        .ImportCertificate(clientUUID, mLabel, clientCert)
                        .IsNone());
    }

    void GeneratePrivateKey(const String& id)
    {
        Error                             err = ErrorEnum::eNone;
        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
        ASSERT_TRUE(err.IsNone());

        // generate key
        uuid::UUID keyUUID;

        Tie(keyUUID, err) = uuid::StringToUUID(id);
        ASSERT_TRUE(err.IsNone());

        pkcs11::PrivateKey key;

        Tie(key, err)
            = pkcs11::Utils(session, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(keyUUID, mLabel, 2048);
        ASSERT_TRUE(err.IsNone());
    }

    static constexpr auto mLabel = "cryptoutils";
    static constexpr auto mPIN   = "admin";

    crypto::MbedTLSCryptoProvider mCryptoProvider;
    test::SoftHSMEnv              mSoftHSMEnv;

    pkcs11::SlotID                    mSlotID = 0;
    SharedPtr<pkcs11::LibraryContext> mLibrary;
    cryptoutils::CertLoader           mCertLoader;

    StaticAllocator<Max(2 * sizeof(pkcs11::PKCS11RSAPrivateKey), sizeof(pkcs11::PKCS11ECDSAPrivateKey),
        2 * sizeof(crypto::x509::Certificate) + sizeof(crypto::x509::CertificateChain)
            + 2 * sizeof(pkcs11::PKCS11RSAPrivateKey))>
        mAllocator;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CryptoutilsTest, ParseScheme)
{
    const char* url1 = "pkcs11:token=aoscore;object=diskencryption;id=2e2769b6-be2c-43ff-b16d-25985a04e6b2?module-path="
                       "/usr/lib/softhsm/libsofthsm2.so&pin-value=42hAGWdIvQr47T8X";
    const char* url2 = "file:/usr/share/.ssh/rsa.pub";
    const char* url3 = "file/usr/share/.ssh/rsa.pub";

    StaticString<30> scheme;

    ASSERT_EQ(ParseURLScheme(url1, scheme), ErrorEnum::eNone);
    EXPECT_EQ(scheme, "pkcs11");

    ASSERT_EQ(ParseURLScheme(url2, scheme), ErrorEnum::eNone);
    EXPECT_EQ(scheme, "file");

    ASSERT_EQ(ParseURLScheme(url3, scheme), ErrorEnum::eNotFound);
}

TEST_F(CryptoutilsTest, ParseFileURL)
{
    const char* url1 = "file:/usr/share/.ssh/rsa.pub";
    const char* url2 = "pkcs11:token=aoscore";

    StaticString<cFilePathLen> path;

    ASSERT_EQ(ParseFileURL(url1, path), ErrorEnum::eNone);
    EXPECT_EQ(path, "/usr/share/.ssh/rsa.pub");

    ASSERT_NE(ParseFileURL(url2, path), ErrorEnum::eNone);
}

TEST_F(CryptoutilsTest, ParsePKCS11URLAllValues)
{
    const char* url1 = "pkcs11:token=aoscore;object=diskencryption;id=2e2769b6-be2c-43ff-b16d-25985a04e6b2?module-path="
                       "/usr/lib/softhsm/libsofthsm2.so&pin-value=42hAGWdIvQr47T8X";

    StaticString<cFilePathLen>      library;
    StaticString<pkcs11::cLabelLen> token;
    StaticString<pkcs11::cLabelLen> label;
    uuid::UUID                      id;
    StaticString<pkcs11::cPINLen>   userPIN;

    ASSERT_EQ(ParsePKCS11URL(url1, library, token, label, id, userPIN), ErrorEnum::eNone);

    EXPECT_EQ(library, "/usr/lib/softhsm/libsofthsm2.so");
    EXPECT_EQ(token, "aoscore");
    EXPECT_EQ(label, "diskencryption");
    EXPECT_EQ(userPIN, "42hAGWdIvQr47T8X");

    auto strID = uuid::UUIDToString(id);

    EXPECT_EQ(strID.CStr(), std::string("2e2769b6-be2c-43ff-b16d-25985a04e6b2"));
}

TEST_F(CryptoutilsTest, ParsePKCS11URLRequiredValuesOnly)
{
    const char* url1 = "pkcs11:object=diskencryption;id=2e2769b6-be2c-43ff-b16d-25985a04e6b2";

    StaticString<cFilePathLen>      library;
    StaticString<pkcs11::cLabelLen> token;
    StaticString<pkcs11::cLabelLen> label;
    uuid::UUID                      id;
    StaticString<pkcs11::cPINLen>   userPIN;

    ASSERT_EQ(ParsePKCS11URL(url1, library, token, label, id, userPIN), ErrorEnum::eNone);

    EXPECT_EQ(library, "");
    EXPECT_EQ(token, "");
    EXPECT_EQ(label, "diskencryption");
    EXPECT_EQ(userPIN, "");

    auto strID = uuid::UUIDToString(id);

    EXPECT_EQ(strID.CStr(), std::string("2e2769b6-be2c-43ff-b16d-25985a04e6b2"));
}

TEST_F(CryptoutilsTest, FindPKCS11CertificateChain)
{
    constexpr auto caId     = "08080808-0404-0404-0404-121212121212";
    constexpr auto clientId = "00000000-0404-0404-0404-121212121212";

    ImportCertificateChainToPKCS11(caId, clientId);

    const char* url = "pkcs11:token=cryptoutils;object=cryptoutils;id=00000000-0404-0404-0404-121212121212?module-"
                      "path=" SOFTHSM2_LIB "&pin-value=admin";

    SharedPtr<crypto::x509::CertificateChain> chain;
    Error                                     error = ErrorEnum::eNone;

    Tie(chain, error) = mCertLoader.LoadCertsChainByURL(url);
    ASSERT_TRUE(error.IsNone());
    ASSERT_TRUE(chain);
    ASSERT_EQ(chain->Size(), 2);

    // check client certificate
    aos::StaticString<aos::crypto::cCertSubjSize> subject;
    ASSERT_TRUE(mCryptoProvider.ASN1DecodeDN((*chain)[0].mSubject, subject).IsNone());
    EXPECT_EQ(std::string(subject.CStr()), std::string("CN=Aos Core"));

    aos::StaticString<aos::crypto::cCertIssuerSize> issuer;
    ASSERT_TRUE(mCryptoProvider.ASN1DecodeDN((*chain)[0].mIssuer, issuer).IsNone());
    EXPECT_EQ(std::string(issuer.CStr()), std::string("CN=Aos Cloud"));

    // check CA certificate
    EXPECT_EQ((*chain)[1].mSubject, (*chain)[1].mIssuer);

    ASSERT_TRUE(mCryptoProvider.ASN1DecodeDN((*chain)[1].mIssuer, issuer).IsNone());
    EXPECT_EQ(std::string(issuer.CStr()), std::string("CN=Aos Cloud"));
}

TEST_F(CryptoutilsTest, FindPKCS11CertificateChainBadURL)
{
    constexpr auto caId     = "08080808-0404-0404-0404-121212121212";
    constexpr auto clientId = "00000000-0404-0404-0404-121212121212";

    ImportCertificateChainToPKCS11(caId, clientId);

    const char* url = "pkcs11:token=cryptoutils;object=cryptoutils;id=00000000-0404-0404-0404-121212121211?module-"
                      "path=" SOFTHSM2_LIB "&pin-value=admin";

    SharedPtr<crypto::x509::CertificateChain> chain;
    Error                                     error = ErrorEnum::eNone;

    Tie(chain, error) = mCertLoader.LoadCertsChainByURL(url);
    ASSERT_TRUE(error.Is(ErrorEnum::eNotFound));
}

TEST_F(CryptoutilsTest, FindPKCS11PrivateKey)
{
    constexpr auto id = "08080808-0404-0404-0404-121212121212";

    GeneratePrivateKey(id);

    const char* url = "pkcs11:token=cryptoutils;object=cryptoutils;id=08080808-0404-0404-0404-121212121212?module-"
                      "path=" SOFTHSM2_LIB "&pin-value=admin";

    SharedPtr<crypto::PrivateKeyItf> privKey;
    Error                            error = ErrorEnum::eNone;

    Tie(privKey, error) = mCertLoader.LoadPrivKeyByURL(url);
    ASSERT_TRUE(error.IsNone());
    ASSERT_TRUE(privKey);
}

TEST_F(CryptoutilsTest, FindPKCS11PrivateKeyBadURL)
{
    constexpr auto id = "08080808-0404-0404-0404-121212121212";

    GeneratePrivateKey(id);

    const char* url = "pkcs11:token=cryptoutils;object=cryptoutils;id=08080808-0404-0404-0404-121212121211?module-"
                      "path=" SOFTHSM2_LIB "&pin-value=admin";

    SharedPtr<crypto::PrivateKeyItf> privKey;
    Error                            error = ErrorEnum::eNone;

    Tie(privKey, error) = mCertLoader.LoadPrivKeyByURL(url);
    ASSERT_TRUE(error.Is(ErrorEnum::eNotFound));
}

TEST_F(CryptoutilsTest, FindCertificatesFromFile)
{
    const char* url = "file:" CERTIFICATES_DIR "/client-ca-chain.pem";

    SharedPtr<crypto::x509::CertificateChain> chain;
    Error                                     error = ErrorEnum::eNone;

    Tie(chain, error) = mCertLoader.LoadCertsChainByURL(url);
    ASSERT_TRUE(error.IsNone());
    ASSERT_TRUE(chain);
    ASSERT_EQ(chain->Size(), 2);

    // check client certificate
    aos::StaticString<aos::crypto::cCertSubjSize> subject;
    ASSERT_TRUE(mCryptoProvider.ASN1DecodeDN((*chain)[0].mSubject, subject).IsNone());
    EXPECT_EQ(std::string(subject.CStr()), std::string("CN=Aos Core"));

    aos::StaticString<aos::crypto::cCertIssuerSize> issuer;
    ASSERT_TRUE(mCryptoProvider.ASN1DecodeDN((*chain)[0].mIssuer, issuer).IsNone());
    EXPECT_EQ(std::string(issuer.CStr()), std::string("CN=Aos Cloud"));

    // check CA certificate
    EXPECT_EQ((*chain)[1].mSubject, (*chain)[1].mIssuer);

    ASSERT_TRUE(mCryptoProvider.ASN1DecodeDN((*chain)[1].mIssuer, issuer).IsNone());
    EXPECT_EQ(std::string(issuer.CStr()), std::string("CN=Aos Cloud"));
}

} // namespace cryptoutils
} // namespace aos
