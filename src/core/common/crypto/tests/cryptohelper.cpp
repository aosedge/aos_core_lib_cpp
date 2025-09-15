/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <vector>

#include <core/common/cloudprotocol/servicediscovery.hpp>
#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/crypto/softhsmenv.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/utils.hpp>

#include "stubs/certprovider.hpp"

using namespace testing;

namespace aos::crypto {

/***********************************************************************************************************************
 * Test stubs
 **********************************************************************************************************************/

void AssertOK(const Error& err)
{
    if (!err.IsNone()) {
        throw err;
    }
}

std::vector<uint8_t> ReadFileFromAESDir(const String& fileName)
{
    StaticArray<uint8_t, 2048> content;

    auto fullPath = CRYPTOHELPER_AES_DIR "/" + std::string(fileName.CStr());

    AssertOK(fs::ReadFile(fullPath.c_str(), content));

    return {content.begin(), content.end()};
}

std::vector<uint8_t> ReadFileFromCrtDir(const String& fileName)
{
    StaticArray<uint8_t, 2048> content;

    auto fullPath = CRYPTOHELPER_CERTS_DIR "/" + std::string(fileName.CStr());

    AssertOK(fs::ReadFile(fullPath.c_str(), content));

    return {content.begin(), content.end()};
}

cloudprotocol::DecryptInfo CreateDecryptionInfo(
    const char* blockAlg, const std::vector<uint8_t>& blockIV, const std::vector<uint8_t>& blockKey)
{
    cloudprotocol::DecryptInfo decryptInfo;

    decryptInfo.mBlockAlg = blockAlg;
    decryptInfo.mBlockIV  = Array<uint8_t>(blockIV.data(), blockIV.size());
    decryptInfo.mBlockIV.Resize(16);

    decryptInfo.mBlockKey = Array<uint8_t>(blockKey.data(), blockKey.size());

    return decryptInfo;
}

cloudprotocol::CertificateInfo CreateCert(CryptoProviderItf& provider, const char* name)
{
    auto fullPath = CRYPTOHELPER_CERTS_DIR "/" + std::string(name) + ".pem";

    StaticString<cCertPEMLen> pem;
    x509::CertificateChain    chain;

    AssertOK(fs::ReadFileToString(fullPath.c_str(), pem));
    AssertOK(provider.PEMToX509Certs(pem, chain));

    cloudprotocol::CertificateInfo cert;

    cert.mFingerprint = name;
    cert.mCertificate = chain[0].mRaw;

    return cert;
}

cloudprotocol::CertificateChainInfo CreateCertChain(const char* name, const std::vector<std::string> fingerprints)
{
    cloudprotocol::CertificateChainInfo chain;

    chain.mName = name;

    for (const auto& item : fingerprints) {
        AssertOK(chain.mFingerprints.PushBack(item.c_str()));
    }

    return chain;
}

cloudprotocol::SignInfo CreateSigns(const char* chainName, const char* algName)
{
    cloudprotocol::SignInfo signs;

    signs.mChainName = chainName;
    signs.mAlg       = algName;

    auto testFilePath = std::string(CRYPTOHELPER_CERTS_DIR "/hello-world.txt.") + chainName + ".sig";
    AssertOK(fs::ReadFile(testFilePath.c_str(), signs.mValue));
    signs.mTrustedTimestamp = Time::Now();

    return signs;
}

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CryptoHelperTest : public Test {
public:
    void SetUp() override
    {
        tests::utils::InitLog();

        ASSERT_TRUE(mCryptoFactory.Init().IsNone());

        mCryptoProvider = &mCryptoFactory.GetCryptoProvider();

        ASSERT_TRUE(mSoftHSMEnv.Init(cPIN, cLabel).IsNone());
        ASSERT_TRUE(mCertLoader.Init(*mCryptoProvider, mSoftHSMEnv.GetManager()).IsNone());

        ASSERT_TRUE(
            mCryptoHelper.Init(mCertProvider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL, cCACert)
                .IsNone());
    }

protected:
    static constexpr auto cDefaultServiceDiscoveryURL = "http://service-discovery-url.html";
    static constexpr auto cCACert                     = CRYPTOHELPER_CERTS_DIR "/rootCA.pem";

    static constexpr auto cLabel = "iam pkcs11 test slot";
    static constexpr auto cPIN   = "admin";

    DefaultCryptoFactory mCryptoFactory;

    CryptoProviderItf* mCryptoProvider;

    CertProviderStub mCertProvider;
    CertLoader       mCertLoader;
    CryptoHelper     mCryptoHelper;
    test::SoftHSMEnv mSoftHSMEnv;
};

TEST_F(CryptoHelperTest, ServiceDiscoveryURLs)
{
    struct TestData {
        std::string mCert;
        std::string mServiceDiscoveryURL;
    };

    TestData testData[] = {{"online", "https://www.mytest.com"}, {"onlineTest1", "https://Test1:9000"},
        {"onlineTest2", cDefaultServiceDiscoveryURL}};

    for (const auto& [certName, url] : testData) {
        mCertProvider.AddCert("online", certName);

        StaticArray<StaticString<cURLLen>, cMaxNumURLs> discoveryURLs;

        ASSERT_TRUE(mCryptoHelper.GetServiceDiscoveryURLs(discoveryURLs).IsNone());
        ASSERT_EQ(discoveryURLs.Size(), 1);
        EXPECT_EQ(url, discoveryURLs[0].CStr());
    }
}

TEST_F(CryptoHelperTest, Decrypt)
{
    struct TestData {
        const char*                mEncryptedFile;
        cloudprotocol::DecryptInfo mDecryptInfo;
        std::vector<uint8_t>       mDecryptedContent;
    };

    std::vector<TestData> testData = {

        {CRYPTOHELPER_AES_DIR "/hello-world.txt.enc",
            CreateDecryptionInfo("AES256/CBC/PKCS7PADDING", {1, 2, 3, 4, 5}, ReadFileFromAESDir("aes.key")),
            ReadFileFromAESDir("hello-world.txt")},
    };

    mCertProvider.AddCert("offline", "offline1");

    for (const auto& test : testData) {
        LOG_INF() << "Decode encrypted file: " << test.mEncryptedFile;

        constexpr auto cDecryptedFile = CRYPTOHELPER_AES_DIR "/decrypted.raw";

        ASSERT_TRUE(mCryptoHelper.Decrypt(test.mEncryptedFile, cDecryptedFile, test.mDecryptInfo).IsNone());
        EXPECT_EQ(test.mDecryptedContent, ReadFileFromAESDir("decrypted.raw"));
    }
}

TEST_F(CryptoHelperTest, ValidateSigns)
{
    struct TestData {
        std::vector<cloudprotocol::CertificateInfo> mCerts;
        cloudprotocol::CertificateChainInfo         mChain;
        cloudprotocol::SignInfo                     mSigns;
    };

    constexpr auto cDecryptedFile = CRYPTOHELPER_CERTS_DIR "/hello-world.txt";

    auto rootCA         = CreateCert(*mCryptoProvider, "rootCA");
    auto secondaryCA    = CreateCert(*mCryptoProvider, "secondaryCA");
    auto intermediateCA = CreateCert(*mCryptoProvider, "intermediateCA");

    auto online      = CreateCert(*mCryptoProvider, "online");
    auto offline1    = CreateCert(*mCryptoProvider, "offline1");
    auto offline2    = CreateCert(*mCryptoProvider, "offline2");
    auto onlineTest1 = CreateCert(*mCryptoProvider, "onlineTest1");
    auto onlineTest2 = CreateCert(*mCryptoProvider, "onlineTest2");

    std::vector<TestData> testData = {
        {{online, intermediateCA, secondaryCA}, CreateCertChain("online", {"online", "intermediateCA", "secondaryCA"}),
            CreateSigns("online", "RSA/SHA256/PKCS1v1_5")},
        {{offline1, intermediateCA, secondaryCA},
            CreateCertChain("offline1", {"offline1", "intermediateCA", "secondaryCA"}),
            CreateSigns("offline1", "RSA/SHA256/PKCS1v1_5")},
        {{offline2, intermediateCA, secondaryCA},
            CreateCertChain("offline2", {"offline2", "intermediateCA", "secondaryCA"}),
            CreateSigns("offline2", "RSA/SHA256/PKCS1v1_5")},
        {{onlineTest1, intermediateCA, secondaryCA},
            CreateCertChain("onlineTest1", {"onlineTest1", "intermediateCA", "secondaryCA"}),
            CreateSigns("onlineTest1", "RSA/SHA256/PKCS1v1_5")},
        {{onlineTest2, intermediateCA, secondaryCA},
            CreateCertChain("onlineTest2", {"onlineTest2", "intermediateCA", "secondaryCA"}),
            CreateSigns("onlineTest2", "RSA/SHA256/PKCS1v1_5")}};

    for (const auto& item : testData) {
        StaticArray<cloudprotocol::CertificateInfo, 10> certs;
        certs = Array<cloudprotocol::CertificateInfo>(item.mCerts.data(), item.mCerts.size());

        StaticArray<cloudprotocol::CertificateChainInfo, 1> chains;
        chains.PushBack(item.mChain);

        ASSERT_TRUE(mCryptoHelper.ValidateSigns(cDecryptedFile, item.mSigns, chains, certs).IsNone());
    }
}

TEST_F(CryptoHelperTest, DecryptMetadata)
{
    StaticArray<uint8_t, cCloudMetadataSize> output;

    auto inputData = ReadFileFromCrtDir("hello-world-cms.txt.offline1.cms");
    auto input     = Array<uint8_t>(inputData.data(), inputData.size());

    mCertProvider.AddCert("offline", "offline1");

    ASSERT_TRUE(mCryptoHelper.DecryptMetadata(input, output).IsNone());

    auto expected = ReadFileFromCrtDir("hello-world-cms.txt");
    EXPECT_EQ(std::vector<uint8_t>(output.begin(), output.end()), expected);
}

} // namespace aos::crypto
