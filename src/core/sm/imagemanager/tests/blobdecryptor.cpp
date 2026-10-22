/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>

#include <core/common/crypto/certloader.hpp>
#include <core/common/crypto/tests/gcmtestvector.hpp>
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/crypto/softhsmenv.hpp>
#include <core/common/tests/mocks/certprovidermock.hpp>
#include <core/common/tests/mocks/cryptomock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/iam/tests/mocks/certloadermock.hpp>
#include <core/sm/imagemanager/blobdecryptor.hpp>

using namespace testing;

namespace aos::sm::imagemanager {

namespace {

constexpr auto cTestDir  = "/tmp/blobdecryptor_test";
constexpr auto cIVLen    = crypto::AESCipherItf::cGCMIVSize;
constexpr auto cTagLen   = crypto::AESCipherItf::cGCMTagSize;
constexpr auto cBlockLen = crypto::AESCipherItf::cBlockSize;
constexpr auto cCertType = "diskencryption";
// the id/label embedded in this URL are what LoadPrivKeyByURL uses to resolve the layer key itself: this
// "diskencryption" cert module is dedicated to pointing at it, not at a real TLS keypair.
constexpr auto cKeyURL
    = "pkcs11:token=aoscore;object=aos-layer-key;id=%00%01%02?module-path=/lib/softhsm.so&pin-source=/pin";

using Bytes = std::vector<uint8_t>;

Bytes Pattern(size_t size, uint8_t seed)
{
    Bytes result(size);

    for (size_t i = 0; i < size; i++) {
        result[i] = static_cast<uint8_t>(seed + i * 31);
    }

    return result;
}

void WriteBytes(const std::string& path, const Bytes& data)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

Bytes ReadBytes(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

Bytes Concat(const Bytes& first, const Bytes& second)
{
    Bytes result(first);

    result.insert(result.end(), second.begin(), second.end());

    return result;
}

Action<Error(const String&, const Array<uint8_t>&, const Array<uint8_t>&, CertInfo&)> ReturnCert(const char* keyURL)
{
    return Invoke([keyURL](const String&, const Array<uint8_t>&, const Array<uint8_t>&, CertInfo& info) {
        info.mKeyURL = keyURL;

        return ErrorEnum::eNone;
    });
}

Action<RetWithError<SharedPtr<crypto::PrivateKeyItf>>(const String&)> ReturnKey(
    const SharedPtr<crypto::PrivateKeyItf>& key)
{
    return Invoke(
        [key](const String&) -> RetWithError<SharedPtr<crypto::PrivateKeyItf>> { return {key, ErrorEnum::eNone}; });
}

Bytes TagOf(const Bytes& blob)
{
    return Bytes(blob.end() - cTagLen, blob.end());
}

// Mocks the keystream blocks BlobDecryptor gets from the key to verify the tag: H = AES_K(0^128) is returned
// as zero, which makes GHASH zero for any ciphertext, so the expected tag is exactly the block returned for
// J0 = IV || 0x00000001.
Action<Error(const Array<uint8_t>&, const crypto::DecryptionOptions&, Array<uint8_t>&)> ReturnKeystream(
    const Bytes& iv, const Bytes& j0Block)
{
    return Invoke([iv, j0Block](const Array<uint8_t>& cipher, const crypto::DecryptionOptions& options,
                      Array<uint8_t>& result) -> Error {
        const auto& counter = options.GetValue<crypto::CTRDecryptionOptions>().mCounter;
        const auto  j0      = Concat(iv, Bytes {0x00, 0x00, 0x00, 0x01});

        EXPECT_EQ(cipher, Array<uint8_t>(Bytes(cBlockLen, 0).data(), cBlockLen));

        if (std::all_of(counter.begin(), counter.end(), [](uint8_t byte) { return byte == 0; })) {
            return result.Resize(cBlockLen, 0);
        }

        EXPECT_EQ(counter, Array<uint8_t>(j0.data(), j0.size()));

        return result.Assign(Array<uint8_t>(j0Block.data(), j0Block.size()));
    });
}

// Deterministic but never repeating "random" source: every call yields different bytes, so staged file names
// stay unique across calls just as with a real generator.
class CountingRandom : public crypto::RandomItf {
public:
    RetWithError<uint64_t> RandInt(uint64_t maxValue) override
    {
        return {mCounter++ % (maxValue + 1), ErrorEnum::eNone};
    }

    Error RandBuffer(Array<uint8_t>& buffer, size_t size = 0) override
    {
        if (auto err = buffer.Resize(size != 0 ? size : buffer.MaxSize()); !err.IsNone()) {
            return err;
        }

        mCounter++;

        for (size_t i = 0; i < buffer.Size(); i++) {
            buffer[i] = static_cast<uint8_t>(mCounter >> ((i % sizeof(mCounter)) * 8));
        }

        return ErrorEnum::eNone;
    }

private:
    uint64_t mCounter = 0;
};

// Random source that always fails.
class FailingRandom : public crypto::RandomItf {
public:
    RetWithError<uint64_t> RandInt(uint64_t maxValue) override
    {
        (void)maxValue;

        return {0, ErrorEnum::eFailed};
    }

    Error RandBuffer(Array<uint8_t>& buffer, size_t size = 0) override
    {
        (void)buffer;
        (void)size;

        return ErrorEnum::eFailed;
    }
};

// Returns the staged output files (decryptedPath.<suffix>.tmp) present next to decryptedPath.
std::vector<std::string> StagedFiles(const std::string& decryptedPath)
{
    const auto               dir    = std::filesystem::path(decryptedPath).parent_path();
    const auto               prefix = std::filesystem::path(decryptedPath).filename().string() + ".";
    std::vector<std::string> result;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        const auto name = entry.path().filename().string();

        if (name.rfind(prefix, 0) == 0 && name.size() > prefix.size() + 4
            && name.compare(name.size() - 4, 4, ".tmp") == 0) {
            result.push_back(entry.path().string());
        }
    }

    return result;
}

// Allocator that can be switched to fail, to exercise out-of-memory paths.
class SwitchAllocator : public AllocatorItf {
public:
    void* Allocate(size_t size) override
    {
        if (mFail) {
            return nullptr;
        }

        // with mFailSize set, fails allocations of that size once mFailSkip of them have succeeded.
        if (mFailSize != 0 && size == mFailSize) {
            if (mFailSkip == 0) {
                return nullptr;
            }

            mFailSkip--;
        }

        return mHeap.Allocate(size);
    }

    void Free(void* data) override { mHeap.Free(data); }

    bool   mFail     = false;
    size_t mFailSize = 0;
    size_t mFailSkip = 0;

private:
    HeapAllocator mHeap;
};

} // namespace

/***********************************************************************************************************************
 * Suite: mocked cert provider/loader/key - covers BlobDecryptor's own responsibilities (resolving and
 * caching the key via CertProviderItf/CertLoaderItf, splitting the IV off the file, staging output,
 * renaming into place only on success) without a real PKCS11 module. The actual AES-GCM
 * encrypt/decrypt/tamper-detection behavior lives in pkcs11::AESPrivateKey and is exercised end-to-end by
 * the round-trip suite below, against a real PKCS11 module.
 **********************************************************************************************************************/

class BlobDecryptorTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        ASSERT_TRUE(mDecryptor.Init(mAllocator, mCertProvider, mCertLoader, mRandom, cCertType).IsNone());

        mKey = MakeShared<StrictMock<crypto::PrivateKeyMock>>(&mAllocator);
        ASSERT_TRUE(mKey);
    }

    void TearDown() override
    {
        // whether decryption succeeded or not, no staged output may be left behind.
        EXPECT_THAT(StagedFiles(mDecryptedPath), IsEmpty());

        (void)fs::Remove(mEncryptedPath.c_str());
        (void)fs::Remove(mDecryptedPath.c_str());
    }

    const std::string mEncryptedPath = "/tmp/blobdecryptor_test_layer.tar.gz.enc";
    const std::string mDecryptedPath = "/tmp/blobdecryptor_test_layer.tar.gz.dec";

    HeapAllocator mAllocator;

    StrictMock<iamclient::CertProviderMock>       mCertProvider;
    StrictMock<crypto::CertLoaderMock>            mCertLoader;
    CountingRandom                                mRandom;
    SharedPtr<StrictMock<crypto::PrivateKeyMock>> mKey;

    BlobDecryptor mDecryptor;
};

TEST_F(BlobDecryptorTest, SplitsIVAndTagAndStreamsCipherToKey)
{
    const auto iv         = Pattern(cIVLen, 1);
    const auto ciphertext = Pattern(1000, 2);
    const auto tag        = Pattern(cTagLen, 3);
    const auto plain      = Pattern(1000, 4);

    WriteBytes(mEncryptedPath, Concat(Concat(iv, ciphertext), tag));

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _))
        .WillOnce(Invoke(
            [](const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& info) {
                EXPECT_STREQ(certType.CStr(), cCertType);
                EXPECT_TRUE(issuer.IsEmpty());
                EXPECT_TRUE(serial.IsEmpty());

                info.mKeyURL = cKeyURL;

                return ErrorEnum::eNone;
            }));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_))
        .WillOnce(Invoke([&](const String& url) -> RetWithError<SharedPtr<crypto::PrivateKeyItf>> {
            EXPECT_STREQ(url.CStr(), cKeyURL);

            return {mKey, ErrorEnum::eNone};
        }));
    EXPECT_CALL(*mKey, Decrypt(_, _, _)).Times(2).WillRepeatedly(ReturnKeystream(iv, tag));

    // verify the CTR counter starts where GCM's payload keystream does (IV || 0x00000002) and, by draining
    // chunkProvider entirely, that exactly the ciphertext (without the trailing tag) reaches the key intact,
    // however many chunks it takes to deliver it.
    EXPECT_CALL(*mKey, StreamDecrypt(_, _, _))
        .WillOnce(Invoke([&](crypto::ChunkProviderItf& chunkProvider, const crypto::DecryptionOptions& options,
                             crypto::ChunkReceiverItf& chunkReceiver) {
            const auto& ctrOptions      = options.GetValue<crypto::CTRDecryptionOptions>();
            const auto  expectedCounter = Concat(iv, Bytes {0x00, 0x00, 0x00, 0x02});

            EXPECT_EQ(ctrOptions.mCounter, Array<uint8_t>(expectedCounter.data(), expectedCounter.size()));

            Bytes drained;

            while (true) {
                auto [chunk, err] = chunkProvider.NextChunk();
                if (err.Is(ErrorEnum::eEOF)) {
                    break;
                }

                if (!err.IsNone()) {
                    ADD_FAILURE() << "unexpected chunk read error: " << err.Message();

                    break;
                }

                drained.insert(drained.end(), chunk.begin(), chunk.end());
            }

            EXPECT_EQ(drained, ciphertext);

            // deliver the plaintext in two parts, through the receiver's own buffer, to check that every
            // chunk reaches the output file in order.
            auto& buffer = chunkReceiver.GetBuffer();

            EXPECT_GE(buffer.MaxSize(), plain.size());

            const auto half = plain.size() / 2;

            for (const auto& [offset, size] : {std::pair {size_t {0}, half}, std::pair {half, plain.size() - half}}) {
                std::copy_n(plain.begin() + offset, size, buffer.Get());

                if (auto err = chunkReceiver.OnChunk(Array<uint8_t>(buffer.Get(), size)); !err.IsNone()) {
                    return err;
                }
            }

            return Error(ErrorEnum::eNone);
        }));

    ASSERT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).IsNone());
    EXPECT_EQ(ReadBytes(mDecryptedPath), plain);
}

TEST_F(BlobDecryptorTest, GetCertFailureIsReturned)
{
    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(Return(ErrorEnum::eNotFound));

    // Decrypt must not be reached (StrictMock on mKey/mCertLoader): there's no key to call it on. No
    // encrypted file is even needed on disk: the key is resolved before anything is read.
    EXPECT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eNotFound));
}

TEST_F(BlobDecryptorTest, LoadKeyFailureIsReturned)
{
    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_))
        .WillOnce(Return(RetWithError<SharedPtr<crypto::PrivateKeyItf>>(nullptr, ErrorEnum::eNotFound)));

    EXPECT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eNotFound));
}

TEST_F(BlobDecryptorTest, KeyIsCachedAfterFirstFetch)
{
    const auto blob = Pattern(cIVLen + cTagLen, 7);

    WriteBytes(mEncryptedPath, blob);

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).Times(1).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).Times(1).WillOnce(ReturnKey(mKey));
    EXPECT_CALL(*mKey, StreamDecrypt(_, _, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(*mKey, Decrypt(_, _, _))
        .Times(4)
        .WillRepeatedly(ReturnKeystream(Bytes(blob.begin(), blob.begin() + cIVLen), TagOf(blob)));

    ASSERT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).IsNone());
    ASSERT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).IsNone());
}

TEST_F(BlobDecryptorTest, GetCertFailureIsNotCachedAndIsRetried)
{
    const auto blob = Pattern(cIVLen + cTagLen, 8);

    WriteBytes(mEncryptedPath, blob);

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _))
        .WillOnce(Return(ErrorEnum::eNotFound))
        .WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));
    EXPECT_CALL(*mKey, StreamDecrypt(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(*mKey, Decrypt(_, _, _))
        .Times(2)
        .WillRepeatedly(ReturnKeystream(Bytes(blob.begin(), blob.begin() + cIVLen), TagOf(blob)));

    EXPECT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eNotFound));
    ASSERT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).IsNone());
}

TEST_F(BlobDecryptorTest, EncryptedFileTooShortIsRejectedWithoutCallingKey)
{
    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));

    // shorter than IV + tag: rejected by BlobDecryptor's own size check, before StreamDecrypt/Decrypt
    // (StrictMock) is ever reached.
    WriteBytes(mEncryptedPath, Pattern(cIVLen + cTagLen - 1, 5));

    EXPECT_FALSE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).IsNone());
}

TEST_F(BlobDecryptorTest, KeyDecryptFailureIsReturnedAndLeavesNoOutput)
{
    WriteBytes(mEncryptedPath, Pattern(cIVLen + cTagLen, 6));

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));
    EXPECT_CALL(*mKey, Decrypt(_, _, _)).WillOnce(ReturnKeystream({}, {}));
    EXPECT_CALL(*mKey, StreamDecrypt(_, _, _)).WillOnce(Return(ErrorEnum::eInvalidChecksum));

    EXPECT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eInvalidChecksum));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorTest, StreamDecryptNotSupportedIsReturnedWithoutFallingBack)
{
    WriteBytes(mEncryptedPath, Pattern(cIVLen + cTagLen, 15));

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));

    // Some PKCS11 modules don't support multi-part CKM_AES_GCM operations at all: StreamDecrypt reports
    // that as ErrorEnum::eNotSupported. BlobDecryptor has no whole-buffer fallback, so this must be
    // returned as-is - key->Decrypt (StrictMock) is never reached.
    EXPECT_CALL(*mKey, Decrypt(_, _, _)).WillOnce(ReturnKeystream({}, {}));
    EXPECT_CALL(*mKey, StreamDecrypt(_, _, _)).WillOnce(Return(ErrorEnum::eNotSupported));

    EXPECT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eNotSupported));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorTest, KeystreamFailureIsReturnedWithoutDecrypting)
{
    WriteBytes(mEncryptedPath, Pattern(cIVLen + cTagLen, 16));

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));

    // H can't be obtained: StreamDecrypt (StrictMock) is never reached.
    EXPECT_CALL(*mKey, Decrypt(_, _, _)).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eFailed));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorTest, TagMismatchIsRejectedAndLeavesNoOutput)
{
    const auto blob = Pattern(cIVLen + cTagLen, 17);
    auto       tag  = TagOf(blob);

    tag[0] ^= 0x01;

    WriteBytes(mEncryptedPath, blob);

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));
    EXPECT_CALL(*mKey, StreamDecrypt(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(*mKey, Decrypt(_, _, _))
        .Times(2)
        .WillRepeatedly(ReturnKeystream(Bytes(blob.begin(), blob.begin() + cIVLen), tag));

    EXPECT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eInvalidChecksum));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorTest, CiphertextNotFullyConsumedIsRejected)
{
    const auto blob = Pattern(cIVLen + 100 + cTagLen, 18);

    WriteBytes(mEncryptedPath, blob);

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));
    EXPECT_CALL(*mKey, Decrypt(_, _, _))
        .WillRepeatedly(ReturnKeystream(Bytes(blob.begin(), blob.begin() + cIVLen), TagOf(blob)));

    // a key that reports success without draining chunkProvider: the tag must not be checked against only
    // part of the ciphertext.
    EXPECT_CALL(*mKey, StreamDecrypt(_, _, _)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_FALSE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).IsNone());
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorTest, MissingEncryptedFileIsReturned)
{
    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));

    EXPECT_FALSE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).IsNone());
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorTest, RandomFailureIsReturnedWithoutCallingKey)
{
    FailingRandom random;
    BlobDecryptor decryptor;

    ASSERT_TRUE(decryptor.Init(mAllocator, mCertProvider, mCertLoader, random, cCertType).IsNone());

    WriteBytes(mEncryptedPath, Pattern(cIVLen + cTagLen, 19));

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));

    EXPECT_TRUE(decryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eFailed));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorTest, UnwritableOutputIsReturnedWithoutCallingKey)
{
    WriteBytes(mEncryptedPath, Pattern(cIVLen + cTagLen, 20));

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));

    EXPECT_FALSE(mDecryptor.Decrypt(mEncryptedPath.c_str(), "/tmp/blobdecryptor_no_such_dir/layer.dec").IsNone());
}

TEST_F(BlobDecryptorTest, UnexpectedKeystreamBlockSizeIsRejected)
{
    WriteBytes(mEncryptedPath, Pattern(cIVLen + cTagLen, 21));

    EXPECT_CALL(mCertProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
    EXPECT_CALL(mCertLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(mKey));
    EXPECT_CALL(*mKey, Decrypt(_, _, _))
        .WillOnce(Invoke([](const Array<uint8_t>&, const crypto::DecryptionOptions&, Array<uint8_t>& result) {
            return result.Resize(cBlockLen / 2);
        }));

    EXPECT_TRUE(mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str()).Is(ErrorEnum::eFailed));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST(BlobDecryptorInitTest, InitFailsIfCertTypeIsTooLong)
{
    HeapAllocator                           allocator;
    StrictMock<iamclient::CertProviderMock> certProvider;
    StrictMock<crypto::CertLoaderMock>      certLoader;
    CountingRandom                          random;
    BlobDecryptor                           decryptor;

    const std::string tooLong(cCertTypeLen + 1, 'a');

    EXPECT_TRUE(decryptor.Init(allocator, certProvider, certLoader, random, tooLong.c_str()).Is(ErrorEnum::eNoMemory));
}

TEST(BlobDecryptorInitTest, DecryptFailsIfAllocatorIsOutOfMemoryResolvingKey)
{
    SwitchAllocator                         allocator;
    StrictMock<iamclient::CertProviderMock> certProvider;
    StrictMock<crypto::CertLoaderMock>      certLoader;
    CountingRandom                          random;
    BlobDecryptor                           decryptor;

    ASSERT_TRUE(decryptor.Init(allocator, certProvider, certLoader, random, cCertType).IsNone());

    allocator.mFail = true;

    // neither GetCert nor LoadPrivKeyByURL is reached (StrictMock): the temporary CertInfo used to resolve
    // the key can't be allocated. No encrypted file is needed: this fails before any file I/O.
    EXPECT_TRUE(decryptor.Decrypt("/nonexistent.enc", "/nonexistent.dec").Is(ErrorEnum::eNoMemory));
}

TEST(BlobDecryptorInitTest, DecryptFailsIfChunkBuffersCanNotBeAllocated)
{
    const std::string encryptedPath = "/tmp/blobdecryptor_oom_layer.enc";
    const std::string decryptedPath = "/tmp/blobdecryptor_oom_layer.dec";

    WriteBytes(encryptedPath, Pattern(cIVLen + 64 + cTagLen, 22));

    // the plaintext buffer is allocated first, the ciphertext chunk buffer once H has been obtained from the key.
    for (size_t skip = 0; skip < 2; skip++) {
        HeapAllocator                           keyAllocator;
        SwitchAllocator                         allocator;
        StrictMock<iamclient::CertProviderMock> certProvider;
        StrictMock<crypto::CertLoaderMock>      certLoader;
        CountingRandom                          random;
        BlobDecryptor                           decryptor;

        auto key = MakeShared<StrictMock<crypto::PrivateKeyMock>>(&keyAllocator);
        ASSERT_TRUE(key);

        ASSERT_TRUE(decryptor.Init(allocator, certProvider, certLoader, random, cCertType).IsNone());

        allocator.mFailSize = cDecryptChunkSize;
        allocator.mFailSkip = skip;

        EXPECT_CALL(certProvider, GetCert(_, _, _, _)).WillOnce(ReturnCert(cKeyURL));
        EXPECT_CALL(certLoader, LoadPrivKeyByURL(_)).WillOnce(ReturnKey(key));

        if (skip != 0) {
            EXPECT_CALL(*key, Decrypt(_, _, _)).WillOnce(ReturnKeystream({}, {}));
        }

        EXPECT_TRUE(decryptor.Decrypt(encryptedPath.c_str(), decryptedPath.c_str()).Is(ErrorEnum::eNoMemory));
        EXPECT_FALSE(std::filesystem::exists(decryptedPath));
        EXPECT_THAT(StagedFiles(decryptedPath), IsEmpty());
    }

    (void)fs::Remove(encryptedPath.c_str());
}

/***********************************************************************************************************************
 * Suite: real PKCS11 key (SoftHSM), through the same CertLoader/BlobDecryptor chain production code uses -
 * encrypts with an independent software implementation, decrypts through the real PKCS11 path with a
 * non-extractable key, matching how the key is actually provisioned.
 **********************************************************************************************************************/

class BlobDecryptorRoundTripTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        std::filesystem::remove_all(cTestDir);
        std::filesystem::create_directories(cTestDir);

        ASSERT_TRUE(fs::WriteStringToFile(mPINSource.c_str(), mPIN, 0600).IsNone());

        ASSERT_TRUE(mCryptoFactory.Init(mAllocator).IsNone());
        mCryptoProvider = &mCryptoFactory.GetCryptoProvider();

        ASSERT_TRUE(mSoftHSMEnv.Init(mAllocator, mPIN, mTokenLabel).IsNone());
        ASSERT_TRUE(mCertLoader.Init(mAllocator, *mCryptoProvider, mSoftHSMEnv.GetManager()).IsNone());
        ASSERT_TRUE(mDecryptor.Init(mAllocator, mCertProvider, mCertLoader, *mCryptoProvider, cCertType).IsNone());

        // this cert module is dedicated to the layer key: its key URL's own id/label directly identify the
        // CKO_SECRET_KEY object imported by ImportSecretKey below (see LoadPrivKeyByURL/FindPrivateKey).
        const auto url = "pkcs11:token=" + std::string(mTokenLabel) + ";object=" + std::string(cKeyLabel)
            + ";id=%00%01%02?module-path=" SOFTHSM2_LIB "&pin-source=" + mPINSource;

        EXPECT_CALL(mCertProvider, GetCert(_, _, _, _))
            .WillRepeatedly(Invoke([url](const String&, const Array<uint8_t>&, const Array<uint8_t>&, CertInfo& info) {
                info.mKeyURL = url.c_str();

                return ErrorEnum::eNone;
            }));
    }

    void TearDown() override
    {
        std::filesystem::remove_all(cTestDir);
        (void)fs::Remove(mPINSource.c_str());
    }

    // Imports a raw AES key into the token as a non-extractable, non-sensitive-value-readable CKO_SECRET_KEY,
    // matching how the key is actually provisioned: Decrypt must work without ever reading it back.
    void ImportSecretKey(const Bytes& key)
    {
        Error                             err = ErrorEnum::eNone;
        SharedPtr<pkcs11::SessionContext> session;

        Tie(session, err) = mSoftHSMEnv.OpenUserSession(mPIN, true);
        ASSERT_TRUE(err.IsNone());

        CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
        CK_KEY_TYPE     keyType  = CKK_AES;
        CK_BBOOL        trueVal  = CK_TRUE;
        CK_BBOOL        falseVal = CK_FALSE;

        constexpr uint8_t idBytes[] = {0x00, 0x01, 0x02};

        StaticArray<uint8_t, pkcs11::cIDSize> id = Array<uint8_t>(idBytes, ArraySize(idBytes));

        // capped at pkcs11::cObjectAttributesCount (10): CreateObject's own internal conversion buffer is
        // sized to that, regardless of this array's own capacity. CKA_ENCRYPT is left out to make room for
        // CKA_ID: this key is only ever used to decrypt.
        StaticArray<pkcs11::ObjectAttribute, pkcs11::cObjectAttributesCount> templ;

        auto pushBytes = [&](pkcs11::AttributeType type, const void* data, size_t size) {
            ASSERT_TRUE(
                templ.PushBack({type, Array<uint8_t>(reinterpret_cast<uint8_t*>(const_cast<void*>(data)), size)})
                    .IsNone());
        };

        pushBytes(CKA_CLASS, &keyClass, sizeof(keyClass));
        pushBytes(CKA_KEY_TYPE, &keyType, sizeof(keyType));
        pushBytes(CKA_TOKEN, &trueVal, sizeof(trueVal));
        pushBytes(CKA_PRIVATE, &trueVal, sizeof(trueVal));
        // non-extractable, sensitive: this is the production posture. AESPrivateKey must still be able to
        // decrypt with it, since it never reads CKA_VALUE.
        pushBytes(CKA_EXTRACTABLE, &falseVal, sizeof(falseVal));
        pushBytes(CKA_SENSITIVE, &trueVal, sizeof(trueVal));
        pushBytes(CKA_DECRYPT, &trueVal, sizeof(trueVal));
        // must match the "diskencryption" cert module's key URL id/label, since that's what LoadPrivKeyByURL
        // searches for.
        pushBytes(CKA_ID, id.Get(), id.Size());
        pushBytes(CKA_LABEL, cKeyLabel, strlen(cKeyLabel));

        ASSERT_TRUE(templ.PushBack({CKA_VALUE, Array<uint8_t>(const_cast<uint8_t*>(key.data()), key.size())}).IsNone());

        pkcs11::ObjectHandle handle    = 0;
        Error                createErr = ErrorEnum::eNone;

        Tie(handle, createErr) = session->CreateObject(templ);
        ASSERT_TRUE(createErr.IsNone());
    }

    // Produces the encrypted blob layout: IV | AES-256-GCM ciphertext | authentication tag, using the
    // software crypto provider - independent of the PKCS11 decrypt path under test.
    void Encrypt(const Bytes& key, const Bytes& iv, const Bytes& plain, Bytes& blob)
    {
        auto [cipher, err] = mCryptoProvider->CreateAESEncoder(
            "GCM", Array<uint8_t>(key.data(), key.size()), Array<uint8_t>(iv.data(), iv.size()));
        ASSERT_TRUE(err.IsNone());

        auto outBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();

        blob = iv;

        constexpr size_t cChunk = 4096;

        for (size_t offset = 0; offset < plain.size(); offset += cChunk) {
            ASSERT_TRUE(cipher
                            ->EncryptBlock(
                                Array<uint8_t>(plain.data() + offset, std::min(cChunk, plain.size() - offset)), *outBuf)
                            .IsNone());

            blob.insert(blob.end(), outBuf->begin(), outBuf->end());
        }

        ASSERT_TRUE(cipher->Finalize(*outBuf).IsNone());
        ASSERT_TRUE(outBuf->IsEmpty());

        StaticArray<uint8_t, cTagLen> tag;

        ASSERT_TRUE(cipher->GetTag(tag).IsNone());

        blob.insert(blob.end(), tag.begin(), tag.end());
    }

    Error DecryptBlob(const Bytes& blob)
    {
        WriteBytes(mEncryptedPath, blob);

        return mDecryptor.Decrypt(mEncryptedPath.c_str(), mDecryptedPath.c_str());
    }

    static constexpr auto cKeyLabel = "aos-layer-key";

    const std::string mEncryptedPath = std::string(cTestDir) + "/layer.tar.gz.enc";
    const std::string mDecryptedPath = std::string(cTestDir) + "/layer.tar.gz.dec";

    static constexpr auto mTokenLabel = "blobdecryptor-roundtrip";
    static constexpr auto mPIN        = "admin";
    const std::string     mPINSource  = std::string(cTestDir) + "/pin.txt";

    // mAllocator must be declared first: members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

    crypto::DefaultCryptoFactory mCryptoFactory;
    crypto::CryptoProviderItf*   mCryptoProvider {};
    test::SoftHSMEnv             mSoftHSMEnv;
    crypto::CertLoader           mCertLoader;

    StrictMock<iamclient::CertProviderMock> mCertProvider;
    BlobDecryptor                           mDecryptor;
};

TEST_F(BlobDecryptorRoundTripTest, DecryptsWhatWasEncrypted)
{
    const auto key = Pattern(32, 20);

    ImportSecretKey(key);

    const std::vector<size_t> sizes {0, 1, 15, 16, 17, 1000, cFileChunkSize, 3 * cFileChunkSize + 123};

    for (const auto size : sizes) {
        SCOPED_TRACE("plaintext size " + std::to_string(size));

        const auto plain = Pattern(size, 21);
        const auto iv    = Pattern(cIVLen, static_cast<uint8_t>(size));

        Bytes blob;

        ASSERT_NO_FATAL_FAILURE(Encrypt(key, iv, plain, blob));

        ASSERT_TRUE(DecryptBlob(blob).IsNone());

        EXPECT_EQ(ReadBytes(mDecryptedPath), plain);
    }
}

TEST_F(BlobDecryptorRoundTripTest, DecryptsBlobProducedByAnIndependentGCMImplementation)
{
    // The layout an external tool has to produce: IV | ciphertext | tag, i.e. IV followed by the output of
    // e.g. Python's AESGCM(key).encrypt(iv, plain, None). The vector is a published AES-256-GCM known answer.
    const Bytes iv(
        aos::crypto::testvectors::cGCMIV, aos::crypto::testvectors::cGCMIV + sizeof(aos::crypto::testvectors::cGCMIV));
    const Bytes cipher(aos::crypto::testvectors::cGCMCipher,
        aos::crypto::testvectors::cGCMCipher + sizeof(aos::crypto::testvectors::cGCMCipher));
    const Bytes tag(aos::crypto::testvectors::cGCMTag,
        aos::crypto::testvectors::cGCMTag + sizeof(aos::crypto::testvectors::cGCMTag));
    const Bytes key(aos::crypto::testvectors::cGCMKey,
        aos::crypto::testvectors::cGCMKey + sizeof(aos::crypto::testvectors::cGCMKey));
    const Bytes plain(aos::crypto::testvectors::cGCMPlain,
        aos::crypto::testvectors::cGCMPlain + sizeof(aos::crypto::testvectors::cGCMPlain));

    ImportSecretKey(key);

    ASSERT_TRUE(DecryptBlob(Concat(Concat(iv, cipher), tag)).IsNone());

    EXPECT_EQ(ReadBytes(mDecryptedPath), plain);
}

TEST_F(BlobDecryptorRoundTripTest, WrongKeyIsRejectedAndLeavesNoOutput)
{
    const auto key   = Pattern(32, 30);
    const auto plain = Pattern(1000, 33);

    ImportSecretKey(Pattern(32, 31));

    Bytes blob;

    ASSERT_NO_FATAL_FAILURE(Encrypt(key, Pattern(cIVLen, 32), plain, blob));

    EXPECT_TRUE(DecryptBlob(blob).Is(ErrorEnum::eInvalidChecksum));
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorRoundTripTest, ModifiedBlobIsRejectedAndLeavesNoOutput)
{
    const auto key   = Pattern(32, 40);
    const auto plain = Pattern(1000, 42);

    ImportSecretKey(key);

    Bytes blob;

    ASSERT_NO_FATAL_FAILURE(Encrypt(key, Pattern(cIVLen, 41), plain, blob));

    struct Case {
        const char*                 mName;
        std::function<void(Bytes&)> mModify;
    };

    const std::vector<Case> cases {
        {"IV", [](Bytes& b) { b[0] ^= 0x01; }},
        {"first ciphertext byte", [](Bytes& b) { b[cIVLen] ^= 0x01; }},
        {"ciphertext in the middle", [](Bytes& b) { b[b.size() / 2] ^= 0x80; }},
        {"last ciphertext byte", [](Bytes& b) { b[b.size() - cTagLen - 1] ^= 0x01; }},
        {"tag", [](Bytes& b) { b[b.size() - 1] ^= 0x01; }},
        {"truncated by one byte", [](Bytes& b) { b.pop_back(); }},
        {"extra trailing byte", [](Bytes& b) { b.push_back(0); }},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(test.mName);

        auto modified = blob;

        test.mModify(modified);

        std::filesystem::remove(mDecryptedPath);

        EXPECT_TRUE(DecryptBlob(modified).Is(ErrorEnum::eInvalidChecksum));
        EXPECT_FALSE(std::filesystem::exists(mDecryptedPath)) << "unauthenticated plaintext was left behind";
    }
}

TEST_F(BlobDecryptorRoundTripTest, BlobWithoutRoomForTagIsRejected)
{
    ImportSecretKey(Pattern(32, 52));

    // IV followed by fewer bytes than an authentication tag
    EXPECT_FALSE(DecryptBlob(Concat(Pattern(cIVLen, 50), Pattern(cTagLen - 1, 51))).IsNone());
    EXPECT_FALSE(std::filesystem::exists(mDecryptedPath));
}

TEST_F(BlobDecryptorRoundTripTest, BlobHasIVCiphertextTagLayout)
{
    const auto plain = Pattern(100, 22);

    Bytes blob;

    ASSERT_NO_FATAL_FAILURE(Encrypt(Pattern(32, 23), Pattern(cIVLen, 24), plain, blob));

    // GCM doesn't pad: IV + as many ciphertext bytes as plaintext + tag
    EXPECT_EQ(blob.size(), cIVLen + plain.size() + cTagLen);
    EXPECT_EQ(Bytes(blob.begin(), blob.begin() + cIVLen), Pattern(cIVLen, 24));
}

} // namespace aos::sm::imagemanager
