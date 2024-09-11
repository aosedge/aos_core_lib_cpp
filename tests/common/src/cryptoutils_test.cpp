/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"
#include "aos/common/crypto/utils.hpp"
#include "aos/test/log.hpp"
#include "aos/test/softhsmenv.hpp"

namespace aos::crypto {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

using namespace testing;

class CryptoutilsTest : public Test {
protected:
    void SetUp() override
    {
        test::InitLog();

        ASSERT_TRUE(FS::WriteStringToFile(mPINSource, mPIN, 0664).IsNone());

        ASSERT_TRUE(mCryptoProvider.Init().IsNone());
        ASSERT_TRUE(mSoftHSMEnv.Init(mPIN, mLabel).IsNone());

        ASSERT_TRUE(mCertLoader.Init(mCryptoProvider, mSoftHSMEnv.GetManager()).IsNone());

        mLibrary = mSoftHSMEnv.GetLibrary();
        mSlotID  = mSoftHSMEnv.GetSlotID();
    }

    void TearDown() override { ASSERT_TRUE(FS::Remove(mPINSource).IsNone()); }

    void ImportCertificateChainToPKCS11(const Array<uint8_t>& caID, const Array<uint8_t>& clientID)
    {
        Error                             err = ErrorEnum::eNone;
        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
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
            pkcs11::Utils(session, mCryptoProvider, mAllocator).ImportCertificate(caID, mLabel, caCert).IsNone());
        ASSERT_TRUE(pkcs11::Utils(session, mCryptoProvider, mAllocator)
                        .ImportCertificate(clientID, mLabel, clientCert)
                        .IsNone());
    }

    void GeneratePrivateKey(const Array<uint8_t>& id)
    {
        Error                             err = ErrorEnum::eNone;
        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
        ASSERT_TRUE(err.IsNone());

        pkcs11::PrivateKey key;

        Tie(key, err)
            = pkcs11::Utils(session, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
        ASSERT_TRUE(err.IsNone());
    }

    static constexpr auto mLabel     = "cryptoutils";
    static constexpr auto mPIN       = "admin";
    static constexpr auto mPINSource = "pin.txt";

    crypto::MbedTLSCryptoProvider mCryptoProvider;
    test::SoftHSMEnv              mSoftHSMEnv;

    pkcs11::SlotID                    mSlotID = 0;
    SharedPtr<pkcs11::LibraryContext> mLibrary;
    crypto::CertLoader                mCertLoader;

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

TEST_F(CryptoutilsTest, ParsePKCS11URLRequiredValuesOnly)
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

TEST_F(CryptoutilsTest, ParsePKCS11URLPinValue)
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

TEST_F(CryptoutilsTest, ParsePKCS11URLPinSource)
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

TEST_F(CryptoutilsTest, ParsePKCS11URLPinValueAndPinSource)
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

TEST_F(CryptoutilsTest, FindPKCS11CertificateChain)
{
    constexpr uint8_t caID[]     = {0x00, 0x01, 0x02};
    constexpr uint8_t clientID[] = {0x00, 0x01, 0x03};

    ImportCertificateChainToPKCS11(Array(caID, ArraySize(caID)), Array(clientID, ArraySize(clientID)));

    const auto url = "pkcs11:token=cryptoutils;object=cryptoutils;id=%00%01%03?module-"
                     "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    SharedPtr<crypto::x509::CertificateChain> chain;
    Error                                     error = ErrorEnum::eNone;

    Tie(chain, error) = mCertLoader.LoadCertsChainByURL(url.c_str());
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
    constexpr uint8_t caID[]     = {0x00, 0x01, 0x02};
    constexpr uint8_t clientID[] = {0x00, 0x01, 0x03};

    ImportCertificateChainToPKCS11(Array(caID, ArraySize(caID)), Array(clientID, ArraySize(clientID)));

    const auto url = "pkcs11:token=cryptoutils;object=cryptoutils;id=%00%01%04?module-"
                     "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    SharedPtr<crypto::x509::CertificateChain> chain;
    Error                                     error = ErrorEnum::eNone;

    Tie(chain, error) = mCertLoader.LoadCertsChainByURL(url.c_str());
    ASSERT_TRUE(error.Is(ErrorEnum::eNotFound));
}

TEST_F(CryptoutilsTest, FindPKCS11PrivateKey)
{
    constexpr uint8_t id[] = {0xAA, 0xBB, 0xCC};

    GeneratePrivateKey(Array(id, ArraySize(id)));

    const auto url = "pkcs11:token=cryptoutils;object=cryptoutils;id=%AA%BB%CC?module-"
                     "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    SharedPtr<crypto::PrivateKeyItf> privKey;
    Error                            error = ErrorEnum::eNone;

    Tie(privKey, error) = mCertLoader.LoadPrivKeyByURL(url.c_str());
    ASSERT_TRUE(error.IsNone());
    ASSERT_TRUE(privKey);
}

TEST_F(CryptoutilsTest, FindPKCS11PrivateKeyBadURL)
{
    constexpr uint8_t id[] = {0xAA, 0xBB, 0xCC};

    GeneratePrivateKey(Array(id, ArraySize(id)));

    const auto url = "pkcs11:token=cryptoutils;object=cryptoutils;id=%AA%BB%FF?module-"
                     "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    SharedPtr<crypto::PrivateKeyItf> privKey;
    Error                            error = ErrorEnum::eNone;

    Tie(privKey, error) = mCertLoader.LoadPrivKeyByURL(url.c_str());
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

} // namespace aos::crypto
