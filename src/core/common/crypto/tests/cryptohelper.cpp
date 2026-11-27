/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <sys/resource.h>
#include <unistd.h>

#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>

#include <core/common/crypto/certloader.hpp>
#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/tests/stubs/failingallocator.hpp>
#if defined(WITH_MBEDTLS)
#include <core/common/crypto/mbedtls/cryptoprovider.hpp>
#else
#include <core/common/crypto/openssl/cryptoprovider.hpp>
#endif
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/crypto/softhsmenv.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/common/tools/utils.hpp>

#include "gcmtestvector.hpp"
#include "stubs/certprovider.hpp"

using namespace testing;
using namespace aos::tests::utils;
using aos::tests::FailingAllocator;

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

DecryptInfo CreateDecryptionInfo(
    const char* blockAlg, const std::vector<uint8_t>& blockIV, const std::vector<uint8_t>& blockKey)
{
    DecryptInfo decryptInfo;

    decryptInfo.mBlockAlg = blockAlg;
    decryptInfo.mBlockIV  = Array<uint8_t>(blockIV.data(), blockIV.size());
    decryptInfo.mBlockIV.Resize(16);

    decryptInfo.mBlockKey = Array<uint8_t>(blockKey.data(), blockKey.size());

    return decryptInfo;
}

CertificateInfo CreateCert(CryptoProviderItf& provider, const char* name)
{
    auto fullPath = CRYPTOHELPER_CERTS_DIR "/" + std::string(name) + ".pem";

    StaticString<cCertPEMLen> pem;
    x509::CertificateChain    chain;

    AssertOK(fs::ReadFileToString(fullPath.c_str(), pem));
    AssertOK(provider.PEMToX509Certs(pem, chain));

    CertificateInfo cert;

    cert.mFingerprint = name;
    cert.mCertificate = chain[0].mRaw;

    return cert;
}

CertificateChainInfo CreateCertChain(const char* name, const std::vector<std::string> fingerprints)
{
    CertificateChainInfo chain;

    chain.mName = name;

    for (const auto& item : fingerprints) {
        AssertOK(chain.mFingerprints.PushBack(item.c_str()));
    }

    return chain;
}

SignInfo CreateSigns(const char* chainName, const char* algName)
{
    SignInfo signs;

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

        ASSERT_TRUE(mCryptoFactory.Init(mAllocator).IsNone());

        mCryptoProvider = &mCryptoFactory.GetCryptoProvider();

        ASSERT_TRUE(mSoftHSMEnv.Init(mAllocator, cPIN, cLabel).IsNone());
        ASSERT_TRUE(mCertLoader.Init(mAllocator, *mCryptoProvider, mSoftHSMEnv.GetManager()).IsNone());

        mCertProvider.AddRootCert("rootCA");

        ASSERT_TRUE(
            mCryptoHelper.Init(mAllocator, mCertProvider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL)
                .IsNone());
    }

protected:
    static constexpr auto cDefaultServiceDiscoveryURL = "http://service-discovery-url.html";

    static constexpr auto cLabel = "iam pkcs11 test slot";
    static constexpr auto cPIN   = "admin";

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

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
        EXPECT_EQ(discoveryURLs, ConvertToArray({StaticString<cURLLen>(url.c_str())}));
    }
}

TEST_F(CryptoHelperTest, Decrypt)
{
    struct TestData {
        const char*          mEncryptedFile;
        DecryptInfo          mDecryptInfo;
        std::vector<uint8_t> mDecryptedContent;
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

namespace {
// unique per process: tests can run in parallel
const std::string cGCMTestDir = "/tmp/cryptohelper_gcm_test_" + std::to_string(getpid());

std::vector<uint8_t> Vec(const uint8_t* data, size_t size)
{
    return {data, data + size};
}

void WriteFileBytes(const std::string& path, const std::vector<uint8_t>& data)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::vector<uint8_t> ReadFileBytes(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

DecryptInfo CreateGCMDecryptInfo(const char* alg, size_t ivSize = sizeof(testvectors::cGCMIV))
{
    DecryptInfo info;

    info.mBlockAlg = alg;
    info.mBlockIV  = Array<uint8_t>(testvectors::cGCMIV, ivSize);
    info.mBlockKey = Array<uint8_t>(testvectors::cGCMKey, sizeof(testvectors::cGCMKey));

    return info;
}

} // namespace

class CryptoHelperGCMTest : public CryptoHelperTest {
protected:
    void SetUp() override
    {
        CryptoHelperTest::SetUp();

        std::filesystem::remove_all(cGCMTestDir);
        std::filesystem::create_directories(cGCMTestDir);

        // ciphertext followed by the authentication tag, as `AESGCM(key).encrypt(iv, plain, None)` returns it
        mEncrypted = Vec(testvectors::cGCMCipher, sizeof(testvectors::cGCMCipher));
        mEncrypted.insert(mEncrypted.end(), testvectors::cGCMTag, testvectors::cGCMTag + sizeof(testvectors::cGCMTag));

        WriteFileBytes(mEncryptedPath, mEncrypted);
    }

    void TearDown() override { std::filesystem::remove_all(cGCMTestDir); }

    // files left behind by an unfinished decryption
    std::vector<std::string> StagedFiles() const
    {
        std::vector<std::string> result;

        for (const auto& entry : std::filesystem::directory_iterator(cGCMTestDir)) {
            if (entry.path().extension() == ".tmp") {
                result.push_back(entry.path().string());
            }
        }

        return result;
    }

    const std::string mEncryptedPath = std::string(cGCMTestDir) + "/file.enc";
    const std::string mDecryptedPath = std::string(cGCMTestDir) + "/file.dec";

    std::vector<uint8_t> mEncrypted;
};

TEST_F(CryptoHelperGCMTest, DecryptGCM)
{
    // the padding part of the algorithm name doesn't apply to GCM and is ignored; names are case insensitive
    for (const auto* alg : {"AES256/GCM", "AES256/GCM/PKCS7PADDING", "aes256/gcm/nopadding"}) {
        SCOPED_TRACE(alg);

        std::filesystem::remove(mDecryptedPath);

        ASSERT_TRUE(
            mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo(alg)).IsNone());

        EXPECT_EQ(ReadFileBytes(mDecryptedPath), Vec(testvectors::cGCMPlain, sizeof(testvectors::cGCMPlain)));
        EXPECT_TRUE(StagedFiles().empty());
    }
}

TEST_F(CryptoHelperGCMTest, DecryptGCMRejectsModifiedFileAndRemovesOutput)
{
    for (const size_t position : {size_t(0), size_t(31), mEncrypted.size() - 17, mEncrypted.size() - 1}) {
        SCOPED_TRACE("modified byte " + std::to_string(position));

        auto modified = mEncrypted;

        modified[position] ^= 0x01;

        WriteFileBytes(mEncryptedPath, modified);
        std::filesystem::remove(mDecryptedPath);

        EXPECT_FALSE(
            mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo("AES256/GCM"))
                .IsNone());
        EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
        EXPECT_TRUE(StagedFiles().empty());
    }
}

TEST_F(CryptoHelperGCMTest, DecryptGCMKeepsExistingOutputWhenVerificationFails)
{
    const std::vector<uint8_t> previous {'o', 'l', 'd'};
    auto                       modified = mEncrypted;

    modified.back() ^= 0x01;

    WriteFileBytes(mEncryptedPath, modified);
    WriteFileBytes(mDecryptedPath, previous);

    EXPECT_FALSE(
        mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo("AES256/GCM"))
            .IsNone());

    // unauthenticated plaintext never reaches the output path
    EXPECT_EQ(ReadFileBytes(mDecryptedPath), previous);
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperGCMTest, DecryptGCMDoesNotUsePredictableStagingPath)
{
    const auto target = std::string(cGCMTestDir) + "/target";

    // an attacker plants symlinks on the likely staging names
    for (const auto* suffix : {".tmp", ".part", ".dec"}) {
        std::filesystem::create_symlink(target, mDecryptedPath + suffix);
    }

    ASSERT_TRUE(
        mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo("AES256/GCM"))
            .IsNone());

    EXPECT_FALSE(std::filesystem::exists(target));
    EXPECT_EQ(ReadFileBytes(mDecryptedPath), Vec(testvectors::cGCMPlain, sizeof(testvectors::cGCMPlain)));
}

TEST_F(CryptoHelperGCMTest, DecryptGCMOutputCantBeReplacedFails)
{
    // rename to the output path fails: the staged plaintext must be discarded
    std::filesystem::create_directory(mDecryptedPath);

    EXPECT_FALSE(
        mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo("AES256/GCM"))
            .IsNone());
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperGCMTest, DecryptGCMMissingInputFails)
{
    std::filesystem::remove(mEncryptedPath);

    EXPECT_FALSE(
        mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo("AES256/GCM"))
            .IsNone());
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperGCMTest, DecryptGCMFileTooShortForTagFails)
{
    WriteFileBytes(mEncryptedPath, std::vector<uint8_t>(15, 0));

    EXPECT_TRUE(
        mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo("AES256/GCM"))
            .Is(ErrorEnum::eInvalidArgument));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(CryptoHelperGCMTest, DecryptChecksIVSizeForTheMode)
{
    // GCM needs a 12 byte IV
    EXPECT_TRUE(
        mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo("AES256/GCM", 16))
            .Is(ErrorEnum::eInvalidArgument));

    // CBC still needs a 16 byte IV
    EXPECT_FALSE(mCryptoHelper
                     .Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(),
                         CreateGCMDecryptInfo("AES256/CBC/PKCS7PADDING", 12))
                     .IsNone());
}

TEST_F(CryptoHelperGCMTest, DecryptUnknownModeIsNotSupported)
{
    EXPECT_FALSE(
        mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo("AES256/CTR"))
            .IsNone());
}

namespace {
#if defined(WITH_MBEDTLS)
using RealCryptoProvider = MbedTLSCryptoProvider;
#else
using RealCryptoProvider = OpenSSLCryptoProvider;
#endif

// Faults injected into the crypto provider and the AES decoders it creates.
struct Faults {
    bool                  mFailRand     = false;
    bool                  mIdentity     = false; // decoder passes data through instead of decrypting
    bool                  mFailDecrypt  = false;
    bool                  mFailSetTag   = false;
    bool                  mFailFinalize = false;
    bool                  mFailCreate   = false;
    std::string           mMode;
    std::vector<uint8_t>  mIV;
    std::function<void()> mOnFinalize;
};

class FaultyCipher : public AESCipherItf {
public:
    explicit FaultyCipher(Faults& faults)
        : mFaults(faults)
    {
    }

    void Init(UniquePtr<AESCipherItf> cipher) { mCipher = Move(cipher); }

    Error EncryptBlock(const Array<uint8_t>& input, Array<uint8_t>& output) override
    {
        return mCipher->EncryptBlock(input, output);
    }

    Error DecryptBlock(const Array<uint8_t>& input, Array<uint8_t>& output) override
    {
        if (mFaults.mFailDecrypt) {
            return ErrorEnum::eFailed;
        }

        if (mFaults.mIdentity) {
            output.Clear();

            return output.Insert(output.end(), input.begin(), input.end());
        }

        return mCipher->DecryptBlock(input, output);
    }

    Error Finalize(Array<uint8_t>& output) override
    {
        if (mFaults.mOnFinalize) {
            mFaults.mOnFinalize();
        }

        if (mFaults.mFailFinalize) {
            return ErrorEnum::eFailed;
        }

        if (mFaults.mIdentity) {
            output.Clear();

            return ErrorEnum::eNone;
        }

        return mCipher->Finalize(output);
    }

    Error SetTag(const Array<uint8_t>& tag) override
    {
        if (mFaults.mFailSetTag) {
            return ErrorEnum::eFailed;
        }

        return mFaults.mIdentity ? Error(ErrorEnum::eNone) : mCipher->SetTag(tag);
    }

private:
    Faults&                 mFaults;
    UniquePtr<AESCipherItf> mCipher;
};

class FaultyCryptoProvider : public RealCryptoProvider {
public:
    Error Init(AllocatorItf& allocator)
    {
        mTestAllocator = &allocator;

        return RealCryptoProvider::Init(allocator);
    }

    Error RandBuffer(Array<uint8_t>& buffer, size_t size) override
    {
        if (mFaults.mFailRand) {
            return ErrorEnum::eFailed;
        }

        return RealCryptoProvider::RandBuffer(buffer, size);
    }

    RetWithError<UniquePtr<AESCipherItf>> CreateAESDecoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv) override
    {
        if (mFaults.mFailCreate) {
            return {{}, ErrorEnum::eFailed};
        }

        auto [cipher, err] = RealCryptoProvider::CreateAESDecoder(mFaults.mMode.empty() ? mode : mFaults.mMode.c_str(),
            key, mFaults.mIV.empty() ? iv : Array<uint8_t>(mFaults.mIV.data(), mFaults.mIV.size()));
        if (!err.IsNone()) {
            return {{}, err};
        }

        auto faulty = MakeUnique<FaultyCipher>(mTestAllocator, mFaults);

        faulty->Init(Move(cipher));

        return {UniquePtr<AESCipherItf>(Move(faulty)), ErrorEnum::eNone};
    }

    Faults mFaults;

private:
    AllocatorItf* mTestAllocator {};
};

// Limits the size of files the process can write while the object is alive.
class FileSizeLimit {
public:
    explicit FileSizeLimit(rlim_t size)
    {
        getrlimit(RLIMIT_FSIZE, &mOld);

        auto limit     = mOld;
        limit.rlim_cur = size;

        mOldHandler = signal(SIGXFSZ, SIG_IGN);
        setrlimit(RLIMIT_FSIZE, &limit);
    }

    ~FileSizeLimit()
    {
        setrlimit(RLIMIT_FSIZE, &mOld);
        signal(SIGXFSZ, mOldHandler);
    }

private:
    rlimit       mOld {};
    sighandler_t mOldHandler {};
};

std::vector<uint8_t> MakePlain(size_t size)
{
    std::vector<uint8_t> plain(size);

    for (size_t i = 0; i < size; i++) {
        plain[i] = static_cast<uint8_t>(i * 7 + i / 251);
    }

    return plain;
}

} // namespace

class CryptoHelperDecryptFaultTest : public CryptoHelperGCMTest {
protected:
    static constexpr size_t cCBCIVSize = 16;

    void SetUp() override
    {
        CryptoHelperGCMTest::SetUp();

        ASSERT_TRUE(mFaultyProvider.Init(mAllocator).IsNone());
        ASSERT_TRUE(
            mFaultyHelper.Init(mTestAllocator, mCertProvider, mFaultyProvider, mCertLoader, cDefaultServiceDiscoveryURL)
                .IsNone());
    }

    Error Decrypt(const std::string& decryptedPath = "")
    {
        return mFaultyHelper.Decrypt(mEncryptedPath.c_str(),
            decryptedPath.empty() ? mDecryptedPath.c_str() : decryptedPath.c_str(), CreateGCMDecryptInfo("AES256/GCM"));
    }

    Error DecryptCBC()
    {
        DecryptInfo info;

        info.mBlockAlg = "AES256/CBC/PKCS7PADDING";
        info.mBlockIV  = Array<uint8_t>(mZeroIV, cCBCIVSize);
        info.mBlockKey = Array<uint8_t>(testvectors::cGCMKey, sizeof(testvectors::cGCMKey));

        return mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), info);
    }

    // encrypts data the way an AES GCM file is stored: ciphertext followed by the authentication tag
    std::vector<uint8_t> EncryptGCM(const std::vector<uint8_t>& plain)
    {
        auto [encoder, err] = mCryptoProvider->CreateAESEncoder(
            "GCM", AsKey(), Array<uint8_t>(testvectors::cGCMIV, sizeof(testvectors::cGCMIV)));
        AssertOK(err);

        return Encrypt(*encoder, plain, true);
    }

    std::vector<uint8_t> EncryptCBC(const std::vector<uint8_t>& plain)
    {
        auto [encoder, err] = mCryptoProvider->CreateAESEncoder("CBC", AsKey(), Array<uint8_t>(mZeroIV, cCBCIVSize));
        AssertOK(err);

        return Encrypt(*encoder, plain, false);
    }

    static Array<uint8_t> AsKey() { return Array<uint8_t>(testvectors::cGCMKey, sizeof(testvectors::cGCMKey)); }

    static std::vector<uint8_t> Encrypt(AESCipherItf& encoder, const std::vector<uint8_t>& plain, bool withTag)
    {
        auto                 block = std::make_unique<StaticArray<uint8_t, cFileChunkSize + 16>>();
        std::vector<uint8_t> result;

        for (size_t offset = 0; offset < plain.size(); offset += cFileChunkSize) {
            const auto size = std::min<size_t>(cFileChunkSize, plain.size() - offset);

            AssertOK(encoder.EncryptBlock(Array<uint8_t>(plain.data() + offset, size), *block));
            result.insert(result.end(), block->begin(), block->end());
        }

        AssertOK(encoder.Finalize(*block));
        result.insert(result.end(), block->begin(), block->end());

        if (withTag) {
            StaticArray<uint8_t, AESCipherItf::cGCMTagSize> tag;

            AssertOK(encoder.GetTag(tag));
            result.insert(result.end(), tag.begin(), tag.end());
        }

        return result;
    }

    const uint8_t mZeroIV[cCBCIVSize] = {};

    FailingAllocator     mTestAllocator;
    FaultyCryptoProvider mFaultyProvider;
    CryptoHelper         mFaultyHelper;
};

TEST_F(CryptoHelperDecryptFaultTest, DecryptFilesOfAnySize)
{
    // sizes around the authentication tag and file chunk boundaries
    const size_t sizes[] = {0, 1, 15, 16, 17, cFileChunkSize - 17, cFileChunkSize - 16, cFileChunkSize - 1,
        cFileChunkSize, cFileChunkSize + 1, 3 * cFileChunkSize + 5};

    for (const auto size : sizes) {
        SCOPED_TRACE(size);

        const auto plain = MakePlain(size);

        WriteFileBytes(mEncryptedPath, EncryptGCM(plain));
        ASSERT_TRUE(Decrypt().IsNone());
        EXPECT_EQ(ReadFileBytes(mDecryptedPath), plain);

        std::filesystem::remove(mDecryptedPath);

        WriteFileBytes(mEncryptedPath, EncryptCBC(plain));
        ASSERT_TRUE(DecryptCBC().IsNone());
        EXPECT_EQ(ReadFileBytes(mDecryptedPath), plain);

        EXPECT_TRUE(StagedFiles().empty());
    }
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptCBCFailureLeavesNoOutput)
{
    // not a multiple of the block size
    WriteFileBytes(mEncryptedPath, std::vector<uint8_t>(17, 1));

    EXPECT_FALSE(DecryptCBC().IsNone());
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());

    // wrong padding
    WriteFileBytes(mEncryptedPath, std::vector<uint8_t>(32, 1));

    EXPECT_FALSE(DecryptCBC().IsNone());
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptSupportedAlgorithms)
{
    // the key doesn't match the algorithm or the algorithm is unknown
    for (const auto* alg : {"AES128/GCM", "AES192/GCM", "AES128/CBC", "AES192/CBC", "AES512/GCM"}) {
        SCOPED_TRACE(alg);

        EXPECT_FALSE(
            mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), CreateGCMDecryptInfo(alg)).IsNone());
    }

    // matching key sizes
    const uint8_t key[24] = {};

    for (const auto* alg : {"AES128/GCM", "AES192/GCM"}) {
        SCOPED_TRACE(alg);

        auto info = CreateGCMDecryptInfo(alg);

        info.mBlockKey = Array<uint8_t>(key, String(alg) == "AES128/GCM" ? 16 : 24);

        // authentication fails: the file is encrypted with another key. The exact error enum isn't part of the
        // contract: a backend can report it through its own error queue (e.g. a pre-existing OpenSSL DSO error).
        EXPECT_FALSE(mCryptoHelper.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str(), info).IsNone());
    }
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptKeepsOutputPrivateRegardlessOfUmask)
{
    // the staged plaintext, and the decrypted file it becomes, stay owner-only no matter the umask: fchmod-style
    // widening (which would ignore the umask) and inheriting a wider mode from the destination are both avoided
    for (const auto mask : {0, 0022, 0077}) {
        SCOPED_TRACE(mask);

        std::filesystem::remove(mDecryptedPath);

        std::vector<std::filesystem::perms> stagedPerms;

        mFaultyProvider.mFaults.mOnFinalize = [this, &stagedPerms] {
            for (const auto& staged : StagedFiles()) {
                stagedPerms.push_back(std::filesystem::status(staged).permissions());
            }
        };

        const auto oldUmask = umask(static_cast<mode_t>(mask));
        const auto err      = Decrypt();
        umask(oldUmask);

        ASSERT_TRUE(err.IsNone());
        ASSERT_EQ(stagedPerms.size(), 1);
        EXPECT_EQ(stagedPerms[0], std::filesystem::perms(0600));
        EXPECT_EQ(std::filesystem::status(mDecryptedPath).permissions(), std::filesystem::perms(0600));
    }
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptDoesNotWidenAnExistingDestination)
{
    // a successful decryption replaces an existing, more permissively readable destination with an owner-only file
    // rather than widening access to it
    WriteFileBytes(mDecryptedPath, {'o', 'l', 'd'});
    std::filesystem::permissions(mDecryptedPath, std::filesystem::perms(0644));

    ASSERT_TRUE(Decrypt().IsNone());

    EXPECT_EQ(std::filesystem::status(mDecryptedPath).permissions(), std::filesystem::perms(0600));
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptFailsWhenStagedFileNameCantBeGenerated)
{
    mFaultyProvider.mFaults.mFailRand = true;

    EXPECT_TRUE(Decrypt().Is(ErrorEnum::eFailed));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptFailsWhenOutputPathIsTooLong)
{
    EXPECT_FALSE(Decrypt(std::string(cFilePathLen, 'a')).IsNone());
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptFailsWhenStagedFileCantBeCreated)
{
    EXPECT_FALSE(Decrypt(std::string(cGCMTestDir) + "/no-such-dir/file.dec").IsNone());
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptFailsWhenInputCantBeRead)
{
    // missing file
    std::filesystem::remove(mEncryptedPath);

    EXPECT_FALSE(Decrypt().IsNone());

    // a directory can be opened but not read
    std::filesystem::create_directory(mEncryptedPath);

    EXPECT_FALSE(Decrypt().IsNone());
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptFailsWithoutMemory)
{
    mTestAllocator.FailAll();

    EXPECT_TRUE(Decrypt().Is(ErrorEnum::eNoMemory));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptFailsWhenOutputCantBeWritten)
{
    auto& faults = mFaultyProvider.mFaults;

    faults.mIdentity = true;

    WriteFileBytes(mEncryptedPath, MakePlain(64 * 1024));

    // limit is set only for the time of decryption to not affect the test output
    faults.mOnFinalize = [] {};

    Error err;
    {
        FileSizeLimit limit(1024);

        err = Decrypt();
    }

    EXPECT_FALSE(err.IsNone());
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptDiscardsOutputOnCipherFailures)
{
    auto& faults = mFaultyProvider.mFaults;

    faults.mFailDecrypt = true;
    EXPECT_TRUE(Decrypt().Is(ErrorEnum::eFailed));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());

    faults             = {};
    faults.mFailSetTag = true;
    EXPECT_TRUE(Decrypt().Is(ErrorEnum::eFailed));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());

    faults               = {};
    faults.mFailFinalize = true;
    EXPECT_TRUE(Decrypt().Is(ErrorEnum::eFailed));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_TRUE(StagedFiles().empty());
}

TEST_F(CryptoHelperDecryptFaultTest, DecryptReportsUnremovableStagedFile)
{
    auto& faults = mFaultyProvider.mFaults;

    // the staged file is replaced with a non-empty directory, which can't be removed as a file
    faults.mFailFinalize = true;
    faults.mOnFinalize   = [this] {
        for (const auto& staged : StagedFiles()) {
            std::filesystem::remove(staged);
            std::filesystem::create_directory(staged);
            WriteFileBytes(staged + "/keep", {1});
        }
    };

    const auto err = Decrypt();

    EXPECT_FALSE(err.IsNone());
    EXPECT_FALSE(err.Is(ErrorEnum::eFailed));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
    EXPECT_EQ(StagedFiles().size(), 1);
}

TEST_F(CryptoHelperTest, ValidateSigns)
{
    struct TestData {
        std::vector<CertificateInfo> mCerts;
        CertificateChainInfo         mChain;
        SignInfo                     mSigns;
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
        StaticArray<CertificateInfo, 10> certs;
        certs = Array<CertificateInfo>(item.mCerts.data(), item.mCerts.size());

        StaticArray<CertificateChainInfo, 1> chains;
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

TEST_F(CryptoHelperTest, GetServiceDiscoveryURLsFallsBackWithoutOnlineCert)
{
    StaticArray<StaticString<cURLLen>, cMaxNumURLs> discoveryURLs;

    ASSERT_TRUE(mCryptoHelper.GetServiceDiscoveryURLs(discoveryURLs).IsNone());
    EXPECT_EQ(discoveryURLs, ConvertToArray({StaticString<cURLLen>(cDefaultServiceDiscoveryURL)}));
}

TEST_F(CryptoHelperTest, DecryptAcceptsAlgorithmNameWithoutModeAndPadding)
{
    constexpr auto cDecryptedFile = CRYPTOHELPER_AES_DIR "/decrypted.raw";
    auto           info           = CreateDecryptionInfo("AES256", {1, 2, 3, 4, 5}, ReadFileFromAESDir("aes.key"));

    mCertProvider.AddCert("offline", "offline1");

    ASSERT_TRUE(mCryptoHelper.Decrypt(CRYPTOHELPER_AES_DIR "/hello-world.txt.enc", cDecryptedFile, info).IsNone());
    EXPECT_EQ(ReadFileFromAESDir("hello-world.txt"), ReadFileFromAESDir("decrypted.raw"));
}

TEST_F(CryptoHelperTest, DecryptRejectsUnsupportedSymmetricAlgorithm)
{
    constexpr auto cDecryptedFile = CRYPTOHELPER_AES_DIR "/decrypted.raw";
    auto info = CreateDecryptionInfo("AES512/CBC/PKCS7PADDING", {1, 2, 3, 4, 5}, ReadFileFromAESDir("aes.key"));

    EXPECT_TRUE(mCryptoHelper.Decrypt(CRYPTOHELPER_AES_DIR "/hello-world.txt.enc", cDecryptedFile, info)
                    .Is(ErrorEnum::eNotSupported));
}

TEST_F(CryptoHelperTest, DecryptRejectsMismatchedKeySize)
{
    constexpr auto cDecryptedFile = CRYPTOHELPER_AES_DIR "/decrypted.raw";
    auto info = CreateDecryptionInfo("AES128/CBC/PKCS7PADDING", {1, 2, 3, 4, 5}, ReadFileFromAESDir("aes.key"));

    EXPECT_TRUE(mCryptoHelper.Decrypt(CRYPTOHELPER_AES_DIR "/hello-world.txt.enc", cDecryptedFile, info)
                    .Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CryptoHelperTest, DecryptMetadataRejectsInvalidInput)
{
    StaticArray<uint8_t, cCloudMetadataSize> output;
    const uint8_t                            garbage[] = {0x01, 0x02, 0x03, 0x04};

    mCertProvider.AddCert("offline", "offline1");

    EXPECT_FALSE(mCryptoHelper.DecryptMetadata(Array<uint8_t>(garbage, sizeof(garbage)), output).IsNone());
}

TEST_F(CryptoHelperTest, ValidateSignsAcceptsAlgNameDefaults)
{
    constexpr auto                       cDecryptedFile = CRYPTOHELPER_CERTS_DIR "/hello-world.txt";
    auto                                 intermediateCA = CreateCert(*mCryptoProvider, "intermediateCA");
    auto                                 secondaryCA    = CreateCert(*mCryptoProvider, "secondaryCA");
    auto                                 offline1       = CreateCert(*mCryptoProvider, "offline1");
    StaticArray<CertificateInfo, 10>     certs;
    StaticArray<CertificateChainInfo, 1> chains;
    auto                                 signs = CreateSigns("offline1", "RSA");

    ASSERT_TRUE(certs.PushBack(offline1).IsNone());
    ASSERT_TRUE(certs.PushBack(intermediateCA).IsNone());
    ASSERT_TRUE(certs.PushBack(secondaryCA).IsNone());
    ASSERT_TRUE(chains.PushBack(CreateCertChain("offline1", {"offline1", "intermediateCA", "secondaryCA"})).IsNone());

    ASSERT_TRUE(mCryptoHelper.ValidateSigns(cDecryptedFile, signs, chains, certs).IsNone());
}

TEST_F(CryptoHelperTest, ValidateSignsRejectsUnsupportedHashVariants)
{
    constexpr auto                       cDecryptedFile = CRYPTOHELPER_CERTS_DIR "/hello-world.txt";
    auto                                 intermediateCA = CreateCert(*mCryptoProvider, "intermediateCA");
    auto                                 secondaryCA    = CreateCert(*mCryptoProvider, "secondaryCA");
    auto                                 offline1       = CreateCert(*mCryptoProvider, "offline1");
    StaticArray<CertificateInfo, 10>     certs;
    StaticArray<CertificateChainInfo, 1> chains;

    ASSERT_TRUE(certs.PushBack(offline1).IsNone());
    ASSERT_TRUE(certs.PushBack(intermediateCA).IsNone());
    ASSERT_TRUE(certs.PushBack(secondaryCA).IsNone());
    ASSERT_TRUE(chains.PushBack(CreateCertChain("offline1", {"offline1", "intermediateCA", "secondaryCA"})).IsNone());

    for (const auto* alg : {"RSA/SHA384/PKCS1v1_5", "RSA/SHA512/PKCS1v1_5", "RSA/SHA999/PKCS1v1_5"}) {
        SCOPED_TRACE(alg);

        auto signs = CreateSigns("offline1", alg);

        EXPECT_FALSE(mCryptoHelper.ValidateSigns(cDecryptedFile, signs, chains, certs).IsNone());
    }
}

TEST_F(CryptoHelperTest, ValidateSignsRejectsUnknownChain)
{
    constexpr auto                       cDecryptedFile = CRYPTOHELPER_CERTS_DIR "/hello-world.txt";
    auto                                 offline1       = CreateCert(*mCryptoProvider, "offline1");
    StaticArray<CertificateInfo, 10>     certs;
    StaticArray<CertificateChainInfo, 1> chains;
    auto                                 signs = CreateSigns("offline1", "RSA/SHA256/PKCS1v1_5");

    ASSERT_TRUE(certs.PushBack(offline1).IsNone());
    ASSERT_TRUE(chains.PushBack(CreateCertChain("missing-chain", {"offline1"})).IsNone());

    EXPECT_FALSE(mCryptoHelper.ValidateSigns(cDecryptedFile, signs, chains, certs).IsNone());
}

TEST_F(CryptoHelperTest, ValidateSignsFailsWithoutMemory)
{
    FailingAllocator                     testAllocator;
    CryptoHelper                         helper;
    constexpr auto                       cDecryptedFile = CRYPTOHELPER_CERTS_DIR "/hello-world.txt";
    auto                                 offline1       = CreateCert(*mCryptoProvider, "offline1");
    StaticArray<CertificateInfo, 10>     certs;
    StaticArray<CertificateChainInfo, 1> chains;
    auto                                 signs = CreateSigns("offline1", "RSA/SHA256/PKCS1v1_5");

    ASSERT_TRUE(
        helper.Init(testAllocator, mCertProvider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL).IsNone());

    ASSERT_TRUE(certs.PushBack(offline1).IsNone());
    ASSERT_TRUE(chains.PushBack(CreateCertChain("offline1", {"offline1"})).IsNone());

    testAllocator.FailAll();

    EXPECT_TRUE(helper.ValidateSigns(cDecryptedFile, signs, chains, certs).Is(ErrorEnum::eNoMemory));
}

TEST_F(CryptoHelperTest, DecryptMetadataFailsWithoutMemory)
{
    FailingAllocator                         testAllocator;
    CryptoHelper                             helper;
    StaticArray<uint8_t, cCloudMetadataSize> output;
    auto                                     inputData = ReadFileFromCrtDir("hello-world-cms.txt.offline1.cms");
    auto                                     input     = Array<uint8_t>(inputData.data(), inputData.size());

    ASSERT_TRUE(
        helper.Init(testAllocator, mCertProvider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL).IsNone());

    mCertProvider.AddCert("offline", "offline1");

    testAllocator.FailAll();

    EXPECT_TRUE(helper.DecryptMetadata(input, output).Is(ErrorEnum::eNoMemory));
}

TEST_F(CryptoHelperTest, InitFailsWhenRootCertsTypeMissing)
{
    CertProviderStub provider;
    CryptoHelper     helper;

    ASSERT_TRUE(helper.Init(mAllocator, provider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL)
                    .Is(ErrorEnum::eNotFound));
}

TEST_F(CryptoHelperTest, InitFailsWhenNoRootCertificatesFound)
{
    CertProviderStub provider;
    CryptoHelper     helper;

    provider.AddEmptyRootCerts();

    ASSERT_TRUE(helper.Init(mAllocator, provider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL)
                    .Is(ErrorEnum::eNotFound));
}

TEST_F(CryptoHelperTest, InitFailsWhenRootCertChainCantLoad)
{
    CertProviderStub provider;
    CryptoHelper     helper;

    provider.AddRootCertURL("file:///nonexistent/root.pem");

    ASSERT_FALSE(
        helper.Init(mAllocator, provider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL).IsNone());
}

TEST_F(CryptoHelperTest, InitFailsWhenRootCertIsNotCA)
{
    CertProviderStub provider;
    CryptoHelper     helper;

    provider.AddRootCert("online");

    ASSERT_FALSE(
        helper.Init(mAllocator, provider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL).IsNone());
}

TEST_F(CryptoHelperTest, InitFailsWithoutMemory)
{
    FailingAllocator alloc;
    CertProviderStub provider;
    CryptoHelper     helper;

    provider.AddRootCert("rootCA");

    alloc.FailAll();

    ASSERT_TRUE(helper.Init(alloc, provider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL)
                    .Is(ErrorEnum::eNoMemory));
}

TEST_F(CryptoHelperTest, InitFailsWhenTooManyRootCerts)
{
    CertProviderStub provider;
    CryptoHelper     helper;

    provider.AddRootCertCopies("rootCA", cMaxNumRootCerts + 1);

    ASSERT_TRUE(helper.Init(mAllocator, provider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL)
                    .Is(ErrorEnum::eNoMemory));
}

namespace {
enum class InjectMode {
    eNone,
    eFail,
    eRemaining,
    eNotFound,
    eCorrupt,
    eCorruptTag,
    eCorruptSize,
    eOversize,
    eCallReader,
};

const uint8_t cExtraData[] = {0x05};
uint8_t       cOversizedData[cCloudMetadataSize + 1] {};

class InjectingCryptoProvider : public FaultyCryptoProvider {
public:
    void Inject(const std::string& method, size_t call, InjectMode mode)
    {
        mMethod = method;
        mCall   = call;
        mMode   = mode;
        mCounts.clear();
    }

    void Reset() { Inject("", 0, InjectMode::eNone); }

    size_t Count(const std::string& method) const
    {
        auto it = mCounts.find(method);

        return it == mCounts.end() ? 0 : it->second;
    }

    std::string mDN;

    asn1::ASN1ParseResult ReadStruct(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ASN1ReaderItf& reader) override
    {
        const auto mode = Hit("ReadStruct");
        if (mode == InjectMode::eFail) {
            return {ErrorEnum::eFailed, data};
        }

        if (mode == InjectMode::eCallReader) {
            return {reader.OnASN1Element(asn1::ASN1Value()), data};
        }

        return Post(mode, RealCryptoProvider::ReadStruct(data, opt, reader));
    }

    asn1::ASN1ParseResult ReadSet(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ASN1ReaderItf& reader) override
    {
        const auto mode = Hit("ReadSet");
        if (mode == InjectMode::eFail) {
            return {ErrorEnum::eFailed, data};
        }

        if (mode == InjectMode::eCallReader) {
            return {reader.OnASN1Element(asn1::ASN1Value()), data};
        }

        if (mode == InjectMode::eCorruptTag) {
            auto wrongTag = asn1::MakeASN1Reader([&reader](const asn1::ASN1Value& value) {
                auto copy = value;

                copy.mTagNumber = 99;

                return reader.OnASN1Element(copy);
            });

            return RealCryptoProvider::ReadSet(data, opt, wrongTag);
        }

        return Post(mode, RealCryptoProvider::ReadSet(data, opt, reader));
    }

    asn1::ASN1ParseResult ReadInteger(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, int32_t& value) override
    {
        const auto mode = Hit("ReadInteger");
        if (mode == InjectMode::eFail) {
            return {ErrorEnum::eFailed, data};
        }

        return Post(mode, RealCryptoProvider::ReadInteger(data, opt, value));
    }

    asn1::ASN1ParseResult ReadBigInt(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, Array<uint8_t>& result) override
    {
        const auto mode = Hit("ReadBigInt");
        if (mode == InjectMode::eFail) {
            return {ErrorEnum::eFailed, data};
        }

        return Post(mode, RealCryptoProvider::ReadBigInt(data, opt, result));
    }

    asn1::ASN1ParseResult ReadOID(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ObjectIdentifier& oid) override
    {
        const auto mode = Hit("ReadOID");
        if (mode == InjectMode::eFail) {
            return {ErrorEnum::eFailed, data};
        }

        auto result = RealCryptoProvider::ReadOID(data, opt, oid);

        if (mode == InjectMode::eCorrupt) {
            oid = "1.2.3.4";
        }

        return Post(mode, result);
    }

    asn1::ASN1ParseResult ReadAID(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::AlgorithmIdentifier& aid) override
    {
        const auto mode = Hit("ReadAID");
        if (mode == InjectMode::eFail) {
            return {ErrorEnum::eFailed, data};
        }

        auto result = RealCryptoProvider::ReadAID(data, opt, aid);

        if (mode == InjectMode::eCorrupt) {
            aid.mOID = "1.2.3.4";
        } else if (mode == InjectMode::eCorruptTag) {
            aid.mParams.mTagNumber = 99;
        } else if (mode == InjectMode::eCorruptSize) {
            aid.mParams.mValue.Rebind(Array<uint8_t>(cExtraData, sizeof(cExtraData)));
        }

        return Post(mode, result);
    }

    asn1::ASN1ParseResult ReadOctetString(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, Array<uint8_t>& result) override
    {
        const auto mode = Hit("ReadOctetString");
        if (mode == InjectMode::eFail) {
            return {ErrorEnum::eFailed, data};
        }

        auto parseResult = RealCryptoProvider::ReadOctetString(data, opt, result);

        if (mode == InjectMode::eCorruptSize) {
            result.Resize(1);
        } else if (mode == InjectMode::eCorrupt) {
            for (auto& byte : result) {
                byte ^= 0x5a;
            }
        }

        return Post(mode, parseResult);
    }

    asn1::ASN1ParseResult ReadRawValue(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ASN1Value& result) override
    {
        const auto mode = Hit("ReadRawValue");
        if (mode == InjectMode::eFail) {
            return {ErrorEnum::eFailed, data};
        }

        auto parseResult = RealCryptoProvider::ReadRawValue(data, opt, result);

        if (mode == InjectMode::eOversize) {
            result.mValue.Rebind(Array<uint8_t>(cOversizedData, sizeof(cOversizedData)));
        } else if (mode == InjectMode::eCorruptSize) {
            result.mValue.Rebind(Array<uint8_t>(cExtraData, sizeof(cExtraData)));
        }

        return Post(mode, parseResult);
    }

    Error ASN1DecodeDN(const Array<uint8_t>& dn, String& result) override
    {
        const auto mode = Hit("ASN1DecodeDN");
        if (mode == InjectMode::eFail) {
            return ErrorEnum::eFailed;
        }

        if (mode == InjectMode::eCorrupt) {
            return result.Assign(mDN.c_str());
        }

        return RealCryptoProvider::ASN1DecodeDN(dn, result);
    }

    Error Verify(const Variant<ECDSAPublicKey, RSAPublicKey>& pubKey, const Hash& hashFunc,
        const x509::Padding& padding, const Array<uint8_t>& digest, const Array<uint8_t>& signature) override
    {
        if (Hit("VerifySign") == InjectMode::eFail) {
            return ErrorEnum::eFailed;
        }

        return RealCryptoProvider::Verify(pubKey, hashFunc, padding, digest, signature);
    }

    Error Verify(const Array<x509::Certificate>& rootCerts, const Array<x509::Certificate>& intermCerts,
        const x509::VerifyOptions& options, const x509::Certificate& cert) override
    {
        if (Hit("VerifyChain") == InjectMode::eFail) {
            return ErrorEnum::eFailed;
        }

        return RealCryptoProvider::Verify(rootCerts, intermCerts, options, cert);
    }

private:
    InjectMode Hit(const std::string& method)
    {
        const auto index = mCounts[method]++;

        return (method == mMethod && index == mCall) ? mMode : InjectMode::eNone;
    }

    static asn1::ASN1ParseResult Post(InjectMode mode, asn1::ASN1ParseResult result)
    {
        if (mode == InjectMode::eRemaining) {
            result.mRemaining.Rebind(Array<uint8_t>(cExtraData, sizeof(cExtraData)));
        } else if (mode == InjectMode::eNotFound) {
            result.mError = ErrorEnum::eNotFound;
        }

        return result;
    }

    std::string                   mMethod;
    size_t                        mCall {};
    InjectMode                    mMode {InjectMode::eNone};
    std::map<std::string, size_t> mCounts;
};

} // namespace

class CryptoHelperInjectTest : public CryptoHelperTest {
protected:
    void SetUp() override
    {
        CryptoHelperTest::SetUp();

        ASSERT_TRUE(mProvider.Init(mAllocator).IsNone());
        ASSERT_TRUE(
            mHelper.Init(mTestAllocator, mCertProvider, mProvider, mCertLoader, cDefaultServiceDiscoveryURL).IsNone());

        mProvider.Reset();

        mCMS      = ReadFileFromCrtDir("hello-world-cms.txt.offline1.cms");
        mExpected = ReadFileFromCrtDir("hello-world-cms.txt");
    }

    Error DecryptMetadata(std::vector<uint8_t> input = {})
    {
        if (input.empty()) {
            input = mCMS;
        }

        mOutput.Clear();

        return mHelper.DecryptMetadata(Array<uint8_t>(input.data(), input.size()), mOutput);
    }

    bool OutputIsExpected() const { return std::vector<uint8_t>(mOutput.begin(), mOutput.end()) == mExpected; }

    Error ValidateSigns(const SignInfo& signs, const std::vector<CertificateInfo>& certs,
        const std::vector<CertificateChainInfo>& chains, const char* file = CRYPTOHELPER_CERTS_DIR "/hello-world.txt")
    {
        return mHelper.ValidateSigns(file, signs, Array<CertificateChainInfo>(chains.data(), chains.size()),
            Array<CertificateInfo>(certs.data(), certs.size()));
    }

    std::vector<CertificateInfo> Offline1Certs()
    {
        return {CreateCert(*mCryptoProvider, "offline1"), CreateCert(*mCryptoProvider, "intermediateCA"),
            CreateCert(*mCryptoProvider, "secondaryCA")};
    }

    static std::vector<CertificateChainInfo> Offline1Chain()
    {
        return {CreateCertChain("offline1", {"offline1", "intermediateCA", "secondaryCA"})};
    }

    FailingAllocator                         mTestAllocator;
    InjectingCryptoProvider                  mProvider;
    CryptoHelper                             mHelper;
    std::vector<uint8_t>                     mCMS;
    std::vector<uint8_t>                     mExpected;
    StaticArray<uint8_t, cCloudMetadataSize> mOutput;
};

TEST_F(CryptoHelperInjectTest, DecryptMetadataFailsOnAnyASN1Error)
{
    const std::vector<std::string> methods = {
        "ReadStruct", "ReadSet", "ReadInteger", "ReadBigInt", "ReadOID", "ReadAID", "ReadOctetString", "ReadRawValue"};

    mCertProvider.AddCert("offline", "offline1");

    ASSERT_TRUE(DecryptMetadata().IsNone());
    ASSERT_TRUE(OutputIsExpected());

    std::map<std::string, size_t> counts;

    for (const auto& method : methods) {
        counts[method] = mProvider.Count(method);

        EXPECT_GT(counts[method], 0) << method;
    }

    for (const auto& method : methods) {
        for (size_t call = 0; call < counts[method]; call++) {
            SCOPED_TRACE(method + " call " + std::to_string(call));

            mProvider.Inject(method, call, InjectMode::eFail);
            EXPECT_FALSE(DecryptMetadata().IsNone());
        }
    }
}

TEST_F(CryptoHelperInjectTest, DecryptMetadataNeverReturnsWrongData)
{
    const std::vector<std::pair<std::string, std::vector<InjectMode>>> faults = {
        {"ReadStruct", {InjectMode::eRemaining, InjectMode::eNotFound}},
        {"ReadSet", {InjectMode::eRemaining, InjectMode::eNotFound, InjectMode::eCorruptTag}},
        {"ReadInteger", {InjectMode::eRemaining}},
        {"ReadBigInt", {InjectMode::eRemaining}},
        {"ReadOID", {InjectMode::eRemaining, InjectMode::eCorrupt}},
        {"ReadAID", {InjectMode::eRemaining, InjectMode::eCorrupt, InjectMode::eCorruptTag, InjectMode::eCorruptSize}},
        {"ReadOctetString", {InjectMode::eRemaining, InjectMode::eCorruptSize, InjectMode::eCorrupt}},
        {"ReadRawValue",
            {InjectMode::eRemaining, InjectMode::eNotFound, InjectMode::eOversize, InjectMode::eCorruptSize}},
    };

    mCertProvider.AddCert("offline", "offline1");

    ASSERT_TRUE(DecryptMetadata().IsNone());

    std::map<std::string, size_t> counts;

    for (const auto& [method, modes] : faults) {
        counts[method] = mProvider.Count(method);
    }

    size_t failures = 0;

    for (const auto& [method, modes] : faults) {
        for (const auto mode : modes) {
            for (size_t call = 0; call < counts[method]; call++) {
                SCOPED_TRACE(method + " call " + std::to_string(call) + " mode " + std::to_string(int(mode)));

                mProvider.Inject(method, call, mode);

                // a corrupted structure is either rejected or doesn't affect the decrypted data
                if (const auto err = DecryptMetadata(); !err.IsNone()) {
                    failures++;
                } else {
                    EXPECT_TRUE(OutputIsExpected());
                }
            }
        }
    }

    EXPECT_GT(failures, 0);
}

TEST_F(CryptoHelperInjectTest, DecryptMetadataRejectsTrailingData)
{
    auto input = mCMS;

    input.push_back(0);

    mCertProvider.AddCert("offline", "offline1");

    EXPECT_TRUE(DecryptMetadata(input).Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CryptoHelperInjectTest, DecryptMetadataFailsWithoutRecipientKey)
{
    EXPECT_TRUE(DecryptMetadata().Is(ErrorEnum::eFailed));

    mCertProvider.AddCertURLs("offline", "file://" CRYPTOHELPER_CERTS_DIR "/offline1.pem", "file:///nonexistent.key");

    EXPECT_TRUE(DecryptMetadata().Is(ErrorEnum::eFailed));
}

TEST_F(CryptoHelperInjectTest, DecryptMetadataFailsWhenMessageCantBeDecrypted)
{
    mCertProvider.AddCert("offline", "offline1");

    auto& faults = mProvider.mFaults;

    faults.mFailDecrypt = true;
    EXPECT_TRUE(DecryptMetadata().Is(ErrorEnum::eFailed));

    faults               = {};
    faults.mFailFinalize = true;
    EXPECT_TRUE(DecryptMetadata().Is(ErrorEnum::eFailed));

    faults = {};

    StaticArray<uint8_t, 1> output;

    EXPECT_TRUE(mHelper.DecryptMetadata(Array<uint8_t>(mCMS.data(), mCMS.size()), output).Is(ErrorEnum::eFailed));
}

TEST_F(CryptoHelperInjectTest, DecryptMetadataFailsOnAnyAllocationFailure)
{
    mCertProvider.AddCert("offline", "offline1");

    for (size_t allocation = 0;; allocation++) {
        SCOPED_TRACE(allocation);

        mTestAllocator.FailAt(allocation, FailingAllocator::cUnlimited);

        const auto err = DecryptMetadata();
        if (err.IsNone()) {
            EXPECT_TRUE(OutputIsExpected());
            break;
        }

        // a recipient whose key can't be retrieved is skipped
        EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory) || err.Is(ErrorEnum::eFailed));
        ASSERT_LT(allocation, 100);
    }
}

TEST_F(CryptoHelperInjectTest, GetServiceDiscoveryURLsOnAnyAllocationFailure)
{
    mCertProvider.AddCert("online", "onlineTest1");

    for (size_t allocation = 0;; allocation++) {
        SCOPED_TRACE(allocation);

        StaticArray<StaticString<cURLLen>, cMaxNumURLs> urls;

        mTestAllocator.FailAt(allocation, FailingAllocator::cUnlimited);

        const auto err = mHelper.GetServiceDiscoveryURLs(urls);
        if (err.IsNone() && urls == ConvertToArray({StaticString<cURLLen>("https://Test1:9000")})) {
            break;
        }

        // without memory for the online certificate, the default URL is used
        EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)
            || (err.IsNone() && urls == ConvertToArray({StaticString<cURLLen>(cDefaultServiceDiscoveryURL)})));
        ASSERT_LT(allocation, 100);
    }
}

TEST_F(CryptoHelperInjectTest, GetServiceDiscoveryURLsFallsBackWhenOnlineCertCantLoad)
{
    mCertProvider.AddCertURLs("online", "file:///nonexistent.pem", "file:///nonexistent.key");

    StaticArray<StaticString<cURLLen>, cMaxNumURLs> urls;

    ASSERT_TRUE(mHelper.GetServiceDiscoveryURLs(urls).IsNone());
    EXPECT_EQ(urls, ConvertToArray({StaticString<cURLLen>(cDefaultServiceDiscoveryURL)}));
}

TEST_F(CryptoHelperInjectTest, GetServiceDiscoveryURLsKeepsProvidedURLs)
{
    StaticArray<StaticString<cURLLen>, cMaxNumURLs> urls;

    ASSERT_TRUE(urls.PushBack("https://provided").IsNone());

    // no online certificate: the default URL isn't added to the provided ones
    ASSERT_TRUE(mHelper.GetServiceDiscoveryURLs(urls).IsNone());
    EXPECT_EQ(urls, ConvertToArray({StaticString<cURLLen>("https://provided")}));
}

TEST_F(CryptoHelperInjectTest, GetServiceDiscoveryURLsFailsWhenURLsDontFit)
{
    StaticArray<StaticString<cURLLen>, 1> urls;

    ASSERT_TRUE(urls.PushBack("https://provided").IsNone());

    mCertProvider.AddCert("online", "online");

    EXPECT_TRUE(mHelper.GetServiceDiscoveryURLs(urls).Is(ErrorEnum::eNoMemory));
}

TEST_F(CryptoHelperInjectTest, GetServiceDiscoveryURLsFromOrganization)
{
    mCertProvider.AddCert("online", "onlineTest1");

    StaticArray<StaticString<cURLLen>, cMaxNumURLs> urls;

    mProvider.Inject("ASN1DecodeDN", 0, InjectMode::eFail);

    EXPECT_TRUE(mHelper.GetServiceDiscoveryURLs(urls).Is(ErrorEnum::eFailed));

    // organization is the last subject field
    mProvider.mDN = "CN=test, O=org";
    mProvider.Inject("ASN1DecodeDN", 0, InjectMode::eCorrupt);
    urls.Clear();

    ASSERT_TRUE(mHelper.GetServiceDiscoveryURLs(urls).IsNone());
    EXPECT_EQ(urls, ConvertToArray({StaticString<cURLLen>("https://org:9000")}));

    // organization is empty: the default URL is used
    mProvider.mDN = "O=, CN=test";
    mProvider.Inject("ASN1DecodeDN", 0, InjectMode::eCorrupt);
    urls.Clear();

    ASSERT_TRUE(mHelper.GetServiceDiscoveryURLs(urls).IsNone());
    EXPECT_EQ(urls, ConvertToArray({StaticString<cURLLen>(cDefaultServiceDiscoveryURL)}));
}

TEST_F(CryptoHelperInjectTest, DecryptRejectsMalformedAlgorithmNames)
{
    constexpr auto cDecryptedFile = CRYPTOHELPER_AES_DIR "/decrypted.raw";
    const auto     key            = ReadFileFromAESDir("aes.key");

    EXPECT_FALSE(mHelper
                     .Decrypt(CRYPTOHELPER_AES_DIR "/hello-world.txt.enc", cDecryptedFile,
                         CreateDecryptionInfo("AES256/CBC/PKCS7PADDING/EXTRA", {1, 2, 3, 4, 5}, key))
                     .IsNone());

    EXPECT_TRUE(mHelper
                    .Decrypt(CRYPTOHELPER_AES_DIR "/hello-world.txt.enc", cDecryptedFile,
                        CreateDecryptionInfo("", {1, 2, 3, 4, 5}, key))
                    .Is(ErrorEnum::eNotSupported));
}

TEST_F(CryptoHelperInjectTest, ValidateSignsIgnoresDuplicates)
{
    auto certs  = Offline1Certs();
    auto chains = Offline1Chain();

    // fingerprints are case insensitive
    certs.push_back(certs[0]);
    certs.back().mFingerprint = "OFFLINE1";
    chains.push_back(CreateCertChain("offline1", {"unknown"}));

    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "RSA/SHA256/PKCS1v1_5"), certs, chains).IsNone());
}

TEST_F(CryptoHelperInjectTest, ValidateSignsRejectsBadCertificates)
{
    const auto signs = CreateSigns("offline1", "RSA/SHA256/PKCS1v1_5");

    auto certs = Offline1Certs();

    certs[1].mCertificate.Resize(10);

    EXPECT_FALSE(ValidateSigns(signs, certs, Offline1Chain()).IsNone());

    certs = Offline1Certs();

    for (size_t i = 0; i < cMaxNumCertificates; i++) {
        certs.push_back(certs[0]);
        certs.back().mFingerprint = ("extra" + std::to_string(i)).c_str();
    }

    EXPECT_TRUE(ValidateSigns(signs, certs, Offline1Chain()).Is(ErrorEnum::eNoMemory));
}

TEST_F(CryptoHelperInjectTest, ValidateSignsRejectsBadChains)
{
    const auto certs = Offline1Certs();

    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "RSA"), certs, {CreateCertChain("", {"offline1"})},
        CRYPTOHELPER_CERTS_DIR "/hello-world.txt")
                    .Is(ErrorEnum::eNotFound));

    auto signs = CreateSigns("offline1", "RSA");

    signs.mChainName = "";

    EXPECT_TRUE(ValidateSigns(signs, certs, {CreateCertChain("", {"offline1"})}).Is(ErrorEnum::eInvalidArgument));

    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "RSA"), certs, {CreateCertChain("offline1", {})})
                    .Is(ErrorEnum::eInvalidArgument));

    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "RSA"), certs, {CreateCertChain("offline1", {"absent"})})
                    .Is(ErrorEnum::eNotFound));

    EXPECT_TRUE(
        ValidateSigns(CreateSigns("offline1", "RSA"), certs, {CreateCertChain("offline1", {"offline1", "absent"})})
            .Is(ErrorEnum::eNotFound));

    EXPECT_FALSE(
        ValidateSigns(CreateSigns("offline1", "RSA"), certs, {CreateCertChain("offline1", {"offline1"})}).IsNone());
}

TEST_F(CryptoHelperInjectTest, ValidateSignsRejectsBadSignatures)
{
    const auto certs  = Offline1Certs();
    const auto chains = Offline1Chain();

    EXPECT_FALSE(ValidateSigns(CreateSigns("offline1", "RSA/SHA256/PKCS1v1_5/EXTRA"), certs, chains).IsNone());

    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "ECDSA/SHA256"), certs, chains).Is(ErrorEnum::eNotSupported));

    EXPECT_FALSE(ValidateSigns(CreateSigns("offline1", "RSA/SHA256/PSS"), certs, chains).IsNone());

    EXPECT_FALSE(
        ValidateSigns(CreateSigns("offline1", "RSA"), certs, chains, CRYPTOHELPER_CERTS_DIR "/absent.txt").IsNone());

    auto signs = CreateSigns("offline1", "RSA");

    signs.mValue[0] ^= 0xff;

    EXPECT_FALSE(ValidateSigns(signs, certs, chains).IsNone());

    mProvider.Inject("VerifySign", 0, InjectMode::eFail);
    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "RSA"), certs, chains).Is(ErrorEnum::eFailed));

    mProvider.Inject("VerifyChain", 0, InjectMode::eFail);
    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "RSA"), certs, chains).Is(ErrorEnum::eFailed));

    mProvider.Reset();
    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "RSA"), certs, chains).IsNone());
}

TEST_F(CryptoHelperInjectTest, ValidateSignsFailsOnAnyAllocationFailure)
{
    const auto certs  = Offline1Certs();
    const auto chains = Offline1Chain();
    const auto signs  = CreateSigns("offline1", "RSA");

    for (size_t allocation = 0;; allocation++) {
        SCOPED_TRACE(allocation);

        mTestAllocator.FailAt(allocation, FailingAllocator::cUnlimited);

        const auto err = ValidateSigns(signs, certs, chains);
        if (err.IsNone()) {
            break;
        }

        EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory));
        ASSERT_LT(allocation, 100);
    }
}

TEST_F(CryptoHelperInjectTest, DecryptMetadataFailsWhenKeyCantBeDecrypted)
{
    mCertProvider.AddCert("offline", "offline1");

    mProvider.Inject("ReadOctetString", 0, InjectMode::eCorrupt);

    EXPECT_TRUE(DecryptMetadata().Is(ErrorEnum::eFailed));

    mProvider.Reset();
    mProvider.mFaults.mFailCreate = true;

    EXPECT_TRUE(DecryptMetadata().Is(ErrorEnum::eFailed));
}

TEST_F(CryptoHelperInjectTest, OptionalCMSFieldsAreSkipped)
{
    mCertProvider.AddCert("offline", "offline1");

    // optional OriginatorInfo and UnprotectedAttrs are present
    for (const auto& method : {"ReadStruct", "ReadSet"}) {
        mProvider.Reset();
        ASSERT_TRUE(DecryptMetadata().IsNone());

        const auto count = mProvider.Count(method);

        for (size_t call = 0; call < count; call++) {
            SCOPED_TRACE(std::string(method) + " call " + std::to_string(call));

            mProvider.Inject(method, call, InjectMode::eCallReader);

            if (DecryptMetadata().IsNone()) {
                EXPECT_TRUE(OutputIsExpected());
            }
        }
    }
}

TEST_F(CryptoHelperInjectTest, DecryptChecksSessionKeyForTheMode)
{
    constexpr auto cDecryptedFile = CRYPTOHELPER_AES_DIR "/decrypted.raw";
    const auto     key            = ReadFileFromAESDir("aes.key");

    // decoder accepts an unknown mode
    mProvider.mFaults.mMode = "CBC";

    EXPECT_TRUE(mHelper
                    .Decrypt(CRYPTOHELPER_AES_DIR "/hello-world.txt.enc", cDecryptedFile,
                        CreateDecryptionInfo("AES256/CTR", {1, 2, 3, 4, 5}, key))
                    .Is(ErrorEnum::eNotSupported));

    // decoder accepts an IV of a wrong size
    mProvider.mFaults.mMode = "";
    mProvider.mFaults.mIV   = std::vector<uint8_t>(16, 0);

    auto info = CreateDecryptionInfo("AES256/CBC", {1, 2, 3, 4, 5}, key);

    info.mBlockIV.Resize(8);

    EXPECT_TRUE(mHelper.Decrypt(CRYPTOHELPER_AES_DIR "/hello-world.txt.enc", cDecryptedFile, info)
                    .Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CryptoHelperInjectTest, ValidateSignsFailsWithTooManyChains)
{
    auto chains = Offline1Chain();

    for (size_t i = 0; i < cCertChainsCount; i++) {
        chains.push_back(CreateCertChain(("chain" + std::to_string(i)).c_str(), {"offline1"}));
    }

    EXPECT_TRUE(ValidateSigns(CreateSigns("offline1", "RSA"), Offline1Certs(), chains).Is(ErrorEnum::eNoMemory));
}

TEST_F(CryptoHelperInjectTest, InitFailsWhenRootCertsDontFit)
{
    const auto       path = cGCMTestDir + "_roots.pem";
    std::string      pem;
    CertProviderStub provider;
    CryptoHelper     helper;

    {
        std::ifstream     file(CRYPTOHELPER_CERTS_DIR "/rootCA.pem");
        std::stringstream content;

        content << file.rdbuf();

        for (size_t i = 0; i < 3; i++) {
            pem += content.str();
        }
    }

    std::ofstream(path) << pem;

    provider.AddRootCertURLCopies("file://" + path, cMaxNumRootCerts / 3 + 1);

    const auto err = helper.Init(mAllocator, provider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL);

    std::filesystem::remove(path);

    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory));
}

TEST(CryptoHelperInfoTest, Compare)
{
    const uint8_t data[]  = {0x01, 0x02};
    const uint8_t other[] = {0x01, 0x03};

    CertificateInfo cert;

    cert.mCertificate = Array<uint8_t>(data, sizeof(data));
    cert.mFingerprint = "fingerprint";

    auto certCopy = cert;

    EXPECT_EQ(cert, certCopy);

    certCopy.mFingerprint = "other";
    EXPECT_NE(cert, certCopy);

    certCopy              = cert;
    certCopy.mCertificate = Array<uint8_t>(other, sizeof(other));
    EXPECT_NE(cert, certCopy);

    const auto chain     = CreateCertChain("chain", {"a", "b"});
    auto       chainCopy = chain;

    EXPECT_EQ(chain, chainCopy);

    chainCopy.mName = "other";
    EXPECT_NE(chain, chainCopy);

    chainCopy = CreateCertChain("chain", {"a"});
    EXPECT_NE(chain, chainCopy);

    const auto decrypt     = CreateDecryptionInfo("AES256/CBC", {1, 2}, {3, 4});
    auto       decryptCopy = decrypt;

    EXPECT_EQ(decrypt, decryptCopy);

    decryptCopy.mBlockAlg = "AES128/CBC";
    EXPECT_NE(decrypt, decryptCopy);

    decryptCopy = CreateDecryptionInfo("AES256/CBC", {1, 3}, {3, 4});
    EXPECT_NE(decrypt, decryptCopy);

    decryptCopy = CreateDecryptionInfo("AES256/CBC", {1, 2}, {3, 5});
    EXPECT_NE(decrypt, decryptCopy);

    SignInfo sign;

    sign.mChainName        = "chain";
    sign.mAlg              = "RSA";
    sign.mValue            = Array<uint8_t>(data, sizeof(data));
    sign.mTrustedTimestamp = Time::Unix(1000);

    auto signCopy = sign;

    EXPECT_EQ(sign, signCopy);

    signCopy.mChainName = "other";
    EXPECT_NE(sign, signCopy);

    signCopy      = sign;
    signCopy.mAlg = "ECDSA";
    EXPECT_NE(sign, signCopy);

    signCopy        = sign;
    signCopy.mValue = Array<uint8_t>(other, sizeof(other));
    EXPECT_NE(sign, signCopy);

    signCopy                   = sign;
    signCopy.mTrustedTimestamp = Time::Unix(2000);
    EXPECT_NE(sign, signCopy);

    signCopy = sign;
    ASSERT_TRUE(signCopy.mOCSPValues.EmplaceBack("ocsp").IsNone());
    EXPECT_NE(sign, signCopy);
}

} // namespace aos::crypto
