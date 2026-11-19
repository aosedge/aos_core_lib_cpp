/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

#include <core/common/crypto/certloader.hpp>
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/crypto/softhsmenv.hpp>
#include <core/common/tests/stubs/testallocator.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
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

TEST_F(CertloaderTest, LoadPrivKeyFromFile)
{
    SharedPtr<PrivateKeyItf> privKey;
    Error                    error;

    Tie(privKey, error) = mCertLoader.LoadPrivKeyByURL("file:" TEST_CERTIFICATES_DIR "/client.key");
    ASSERT_TRUE(error.IsNone());
    EXPECT_TRUE(privKey);

    Tie(privKey, error) = mCertLoader.LoadPrivKeyByURL("file:" TEST_CERTIFICATES_DIR "/not-existing.key");
    EXPECT_FALSE(error.IsNone());
    EXPECT_FALSE(privKey);
}

TEST_F(CertloaderTest, LoadByInvalidURL)
{
    const std::string tooLongPath = "file:/" + std::string(cFilePathLen, 'a');
    const std::string badPIN
        = "pkcs11:token=cryptoutils;object=cryptoutils;id=%AA%BB%CC?module-path=" SOFTHSM2_LIB "&pin-value=wrong";

    const std::string testCases[] = {
        "no scheme",
        "http://localhost/cert.pem",
        "file:" CERTIFICATES_DIR "/not-existing.pem",
        tooLongPath,
        "pkcs11:token=cryptoutils;id=%AA%BB%CC",
        "pkcs11:token=unknown;object=cryptoutils;id=%AA%BB%CC?module-path=" SOFTHSM2_LIB,
        "pkcs11:token=cryptoutils;object=cryptoutils;id=%AA%BB%CC?module-path=/not-existing.so",
        badPIN,
    };

    for (const auto& url : testCases) {
        SCOPED_TRACE(url);

        auto [chain, certsErr] = mCertLoader.LoadCertsChainByURL(url.c_str());
        EXPECT_FALSE(certsErr.IsNone());
        EXPECT_FALSE(chain);

        auto [key, keyErr] = mCertLoader.LoadPrivKeyByURL(url.c_str());
        EXPECT_FALSE(keyErr.IsNone());
        EXPECT_FALSE(key);
    }

    EXPECT_TRUE(mCertLoader.LoadCertsChainByURL("http://localhost/cert.pem").mError.Is(ErrorEnum::eInvalidArgument));
    EXPECT_TRUE(mCertLoader.LoadPrivKeyByURL("http://localhost/key.pem").mError.Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CertloaderTest, ParsePKCS11URLErrors)
{
    const std::string tooLong(cFilePathLen + 1, 'a');

    const char* cInvalidURLs[] = {
        "file:/usr/share/.ssh/rsa.pub",
        "pkcs11:token=aoscore;id=%00%01",
        "pkcs11:token=aoscore;object=diskencryption",
        "pkcs11:object=diskencryption;id=%00%0",
        "pkcs11:object=diskencryption;id=%00x01",
        "pkcs11:object=diskencryption;id=%00%zz",
    };

    StaticString<cFilePathLen>            library;
    StaticString<pkcs11::cLabelLen>       token;
    StaticString<pkcs11::cLabelLen>       label;
    StaticArray<uint8_t, pkcs11::cIDSize> id;
    StaticString<pkcs11::cPINLen>         userPIN;

    for (const auto* url : cInvalidURLs) {
        EXPECT_FALSE(ParsePKCS11URL(url, library, token, label, id, userPIN).IsNone()) << url;
    }

    const std::string cInvalidStrURLs[] = {
        "pkcs11:object=diskencryption;id=%00?module-path=" + tooLong,
        "pkcs11:token=" + tooLong + ";object=diskencryption;id=%00",
        "pkcs11:object=diskencryption;id=%00?pin-value=" + tooLong,
        "pkcs11:object=diskencryption;id=%00?pin-source=" + tooLong,
        "pkcs11:object=diskencryption;id=%00?pin-source=/not-existing-pin-file",
    };

    for (const auto& url : cInvalidStrURLs) {
        EXPECT_FALSE(ParsePKCS11URL(url.c_str(), library, token, label, id, userPIN).IsNone()) << url;
    }

    StaticArray<uint8_t, 2> smallID;

    EXPECT_FALSE(
        ParsePKCS11URL("pkcs11:object=diskencryption;id=%00%01%02", library, token, label, smallID, userPIN).IsNone());
}

TEST_F(CertloaderTest, EncodePKCS11ID)
{
    const uint8_t id[] = {0x00, 0xAB, 0x10};

    StaticString<pkcs11::cIDStrLen> idStr;

    ASSERT_TRUE(EncodePKCS11ID(Array<uint8_t>(id, sizeof(id)), idStr).IsNone());
    EXPECT_EQ(idStr, "%00%AB%10");

    StaticArray<uint8_t, pkcs11::cIDSize> decoded;

    ASSERT_TRUE(DecodeToPKCS11ID(idStr, decoded).IsNone());
    EXPECT_EQ(decoded, Array<uint8_t>(id, sizeof(id)));

    StaticString<4> small;

    EXPECT_FALSE(EncodePKCS11ID(Array<uint8_t>(id, sizeof(id)), small).IsNone());

    StaticString<3> tiny = "%00";

    EXPECT_FALSE(EncodePKCS11ID(Array<uint8_t>(id, sizeof(id)), tiny).IsNone());
}

TEST_F(CertloaderTest, LoadFailsOnAnyAllocationFailure)
{
    constexpr uint8_t caID[]     = {0x00, 0x01, 0x02};
    constexpr uint8_t clientID[] = {0x00, 0x01, 0x03};

    ImportCertificateChainToPKCS11(Array(caID, ArraySize(caID)), Array(clientID, ArraySize(clientID)));

    const auto pkcs11URL = "pkcs11:token=cryptoutils;object=cryptoutils;id=%00%01%03?module-"
                           "path=" SOFTHSM2_LIB "&pin-source="
        + std::string(mPINSource);

    tests::TestAllocator allocator;
    CertLoader           certLoader;

    ASSERT_TRUE(certLoader.Init(allocator, *mCryptoProvider, mSoftHSMEnv.GetManager()).IsNone());

    const std::vector<std::function<Error()>> loaders = {
        [&] { return certLoader.LoadCertsChainByURL("file:" CERTIFICATES_DIR "/client-ca-chain.pem").mError; },
        [&] { return certLoader.LoadPrivKeyByURL("file:" TEST_CERTIFICATES_DIR "/client.key").mError; },
        [&] { return certLoader.LoadCertsChainByURL(pkcs11URL.c_str()).mError; },
    };

    for (size_t i = 0; i < loaders.size(); i++) {
        for (size_t allocation = 0;; allocation++) {
            SCOPED_TRACE("loader " + std::to_string(i) + " allocation " + std::to_string(allocation));

            allocator.FailAfter(allocation);

            const auto err = loaders[i]();

            allocator.Reset();

            if (err.IsNone()) {
                break;
            }

            // PKCS11 search skips certificates it can't process and reports them as not found
            EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory) || (i == 2 && err.Is(ErrorEnum::eNotFound)))
                << tests::utils::ErrorToStr(err);
            ASSERT_LT(allocation, 100);
        }
    }
}

} // namespace aos::crypto
