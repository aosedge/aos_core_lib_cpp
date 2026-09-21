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
#include <vector>

#include <core/common/crypto/certloader.hpp>
#include <core/common/crypto/cryptohelper.hpp>
#if defined(WITH_MBEDTLS)
#include <core/common/crypto/mbedtls/cryptoprovider.hpp>
#else
#include <core/common/crypto/openssl/cryptoprovider.hpp>
#endif
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/crypto/softhsmenv.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/common/tools/utils.hpp>

#include "gcmtestvector.hpp"
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

        ASSERT_TRUE(
            mCryptoHelper
                .Init(mAllocator, mCertProvider, *mCryptoProvider, mCertLoader, cDefaultServiceDiscoveryURL, cCACert)
                .IsNone());
    }

protected:
    static constexpr auto cDefaultServiceDiscoveryURL = "http://service-discovery-url.html";
    static constexpr auto cCACert                     = CRYPTOHELPER_CERTS_DIR "/rootCA.pem";

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
        ASSERT_EQ(discoveryURLs.Size(), 1);
        EXPECT_EQ(url, discoveryURLs[0].CStr());
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
        auto [cipher, err] = RealCryptoProvider::CreateAESDecoder(mode, key, iv);
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

// Allocator that can be switched to fail.
class SwitchAllocator : public AllocatorItf {
public:
    void* Allocate(size_t size) override { return mFail ? nullptr : mHeap.Allocate(size); }
    void  Free(void* data) override { mHeap.Free(data); }

    bool mFail = false;

private:
    HeapAllocator mHeap;
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
        ASSERT_TRUE(mFaultyHelper
                        .Init(mSwitchAllocator, mCertProvider, mFaultyProvider, mCertLoader,
                            cDefaultServiceDiscoveryURL, cCACert)
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

    SwitchAllocator      mSwitchAllocator;
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
    mSwitchAllocator.mFail = true;

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

} // namespace aos::crypto
