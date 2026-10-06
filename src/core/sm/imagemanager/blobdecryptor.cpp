/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/error.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>

#include "blobdecryptor.hpp"

namespace aos::sm::imagemanager {

namespace {

constexpr auto cBlockSize = crypto::AESCipherItf::cBlockSize;

// random bytes in a staged output file's name.
constexpr size_t cStagedSuffixSize = 16;

// Low 32 bits of the counter blocks GCM derives from a 96-bit IV: J0 = IV || 0x00000001 encrypts the tag,
// the payload is plain CTR starting at inc32(J0).
constexpr uint8_t cJ0CounterSuffix[]      = {0x00, 0x00, 0x00, 0x01};
constexpr uint8_t cPayloadCounterSuffix[] = {0x00, 0x00, 0x00, 0x02};

// GCM's GHASH over a ciphertext without additional authenticated data, fed incrementally. 4-bit table
// multiplication in GF(2^128) (Shoup's method, as in mbedTLS): the hash key H = AES_K(0^128) comes from the
// token, everything else is plain arithmetic.
class GHash {
public:
    explicit GHash(const Array<uint8_t>& h)
    {
        uint64_t vh = LoadBE64(h.Get());
        uint64_t vl = LoadBE64(h.Get() + 8);

        // 8 = 0b1000 corresponds to 1 in GF(2^128)
        mHH[8] = vh;
        mHL[8] = vl;

        for (size_t i = 4; i > 0; i >>= 1) {
            uint64_t t = (vl & 1) * 0xe100000000000000ULL;

            vl = (vh << 63) | (vl >> 1);
            vh = (vh >> 1) ^ t;

            mHH[i] = vh;
            mHL[i] = vl;
        }

        for (size_t i = 2; i <= 8; i *= 2) {
            for (size_t j = 1; j < i; j++) {
                mHH[i + j] = mHH[i] ^ mHH[j];
                mHL[i + j] = mHL[i] ^ mHL[j];
            }
        }
    }

    void Update(const Array<uint8_t>& data)
    {
        for (const auto byte : data) {
            mPending[mPendingSize++] = byte;

            if (mPendingSize == cBlockSize) {
                Absorb(mPending);
                mPendingSize = 0;
            }
        }

        mDataSize += data.Size();
    }

    // Completes the hash (zero-pads the last partial block, appends the length block) into out.
    void Finalize(uint8_t (&out)[cBlockSize])
    {
        if (mPendingSize != 0) {
            (void)memset(mPending + mPendingSize, 0, cBlockSize - mPendingSize);
            Absorb(mPending);
            mPendingSize = 0;
        }

        // len(AAD) = 0 || len(C), both in bits, 64-bit big-endian.
        uint8_t lengths[cBlockSize] = {};

        StoreBE64(static_cast<uint64_t>(mDataSize) * 8, lengths + 8);
        Absorb(lengths);

        (void)memcpy(out, mY, cBlockSize);
    }

private:
    static uint64_t LoadBE64(const uint8_t* data)
    {
        uint64_t value = 0;

        for (size_t i = 0; i < 8; i++) {
            value = (value << 8) | data[i];
        }

        return value;
    }

    static void StoreBE64(uint64_t value, uint8_t* data)
    {
        for (size_t i = 0; i < 8; i++) {
            data[7 - i] = static_cast<uint8_t>(value >> (i * 8));
        }
    }

    // Y = (Y ^ block) * H
    void Absorb(const uint8_t* block)
    {
        static constexpr uint64_t cLast4[16] = {0x0000, 0x1c20, 0x3840, 0x2460, 0x7080, 0x6ca0, 0x48c0, 0x54e0, 0xe100,
            0xfd20, 0xd940, 0xc560, 0x9180, 0x8da0, 0xa9c0, 0xb5e0};

        uint8_t x[cBlockSize] {};

        for (size_t i = 0; i < cBlockSize; i++) {
            x[i] = mY[i] ^ block[i];
        }

        uint8_t  lo = x[15] & 0xf;
        uint64_t zh = mHH[lo];
        uint64_t zl = mHL[lo];

        for (int32_t i = 15; i >= 0; i--) {
            lo         = x[i] & 0xf;
            uint8_t hi = (x[i] >> 4) & 0xf;

            if (i != 15) {
                uint8_t rem = zl & 0xf;

                zl = (zh << 60) | (zl >> 4);
                zh = (zh >> 4) ^ (cLast4[rem] << 48) ^ mHH[lo];
                zl ^= mHL[lo];
            }

            uint8_t rem = zl & 0xf;

            zl = (zh << 60) | (zl >> 4);
            zh = (zh >> 4) ^ (cLast4[rem] << 48) ^ mHH[hi];
            zl ^= mHL[hi];
        }

        StoreBE64(zh, mY);
        StoreBE64(zl, mY + 8);
    }

    uint64_t mHH[16] {};
    uint64_t mHL[16] {};
    uint8_t  mY[cBlockSize] {};
    uint8_t  mPending[cBlockSize] {};
    size_t   mPendingSize = 0;
    size_t   mDataSize    = 0;
};

// Builds a CTR counter block from a 12-byte GCM IV and the counter's low 32 bits.
Error MakeCounter(const Array<uint8_t>& iv, const uint8_t (&suffix)[4], crypto::CTRDecryptionOptions& options)
{
    if (auto err = options.mCounter.Assign(iv); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto byte : suffix) {
        if (auto err = options.mCounter.PushBack(byte); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

// Returns AES_K(counter) - the key's CTR keystream block for counter - by CTR-decrypting a zero block on the
// token: needs nothing beyond the CKA_DECRYPT/CKM_AES_CTR the payload itself is decrypted with.
Error KeystreamBlock(const crypto::PrivateKeyItf& key, const crypto::CTRDecryptionOptions& options,
    StaticArray<uint8_t, cBlockSize>& block)
{
    StaticArray<uint8_t, cBlockSize> zeros;

    if (auto err = zeros.Resize(cBlockSize, 0); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = key.Decrypt(zeros, crypto::DecryptionOptions {options}, block); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (block.Size() != cBlockSize) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "unexpected keystream block size"));
    }

    return ErrorEnum::eNone;
}

// Adapts an open fs::File into a crypto::ChunkProviderItf, so the whole ciphertext file never needs to be
// read into memory before decrypting it. Delivers exactly size bytes from the file's current position, so
// trailing data (the GCM tag) is left out, and feeds every delivered byte into ghash. buffer is allocator-owned by the
// caller (BlobDecryptor, which has an AllocatorItf) rather than by this class itself: a chunk-sized (tens of KiB)
// buffer would blow the stack budget on a stack-constrained target if it lived in a local variable instead.
class FileChunkProvider : public crypto::ChunkProviderItf {
public:
    FileChunkProvider(fs::File& file, Array<uint8_t>& buffer, size_t size, GHash& ghash)
        : mFile(file)
        , mBuffer(buffer)
        , mRemaining(size)
        , mGHash(ghash)
    {
    }

    bool IsExhausted() const { return mRemaining == 0; }

    RetWithError<Array<uint8_t>> NextChunk() override
    {
        if (mRemaining == 0) {
            return {Array<uint8_t>(), ErrorEnum::eEOF};
        }

        Array<uint8_t> chunk(mBuffer.Get(), Min(mBuffer.MaxSize(), mRemaining));

        if (auto err = mFile.ReadBlock(chunk); !err.IsNone() && !err.Is(ErrorEnum::eEOF)) {
            return {Array<uint8_t>(), AOS_ERROR_WRAP(err)};
        }

        if (chunk.IsEmpty()) {
            return {Array<uint8_t>(), AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "encrypted file ended prematurely"))};
        }

        mRemaining -= chunk.Size();

        mGHash.Update(chunk);

        return {chunk, ErrorEnum::eNone};
    }

private:
    fs::File&       mFile;
    Array<uint8_t>& mBuffer;
    size_t          mRemaining;
    GHash&          mGHash;
};

// Adapts an open fs::File into a crypto::ChunkReceiverItf, so decrypted data is written out as the key
// releases it instead of being collected in memory first. buffer is allocator-owned by the caller, for the
// same reason as FileChunkProvider's.
class FileChunkReceiver : public crypto::ChunkReceiverItf {
public:
    FileChunkReceiver(fs::File& file, Array<uint8_t>& buffer)
        : mFile(file)
        , mBuffer(buffer)
    {
    }

    Array<uint8_t>& GetBuffer() override { return mBuffer; }

    Error OnChunk(const Array<uint8_t>& chunk) override
    {
        if (auto err = mFile.WriteBlock(chunk); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

private:
    fs::File&       mFile;
    Array<uint8_t>& mBuffer;
};

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error BlobDecryptor::Init(AllocatorItf& allocator, iamclient::CertProviderItf& certProvider,
    crypto::CertLoaderItf& certLoader, crypto::RandomItf& random, const String& certType)
{
    mAllocator    = &allocator;
    mCertProvider = &certProvider;
    mCertLoader   = &certLoader;
    mRandom       = &random;

    if (auto err = mCertType.Assign(certType); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error BlobDecryptor::Decrypt(const String& encryptedPath, const String& decryptedPath)
{
    LOG_DBG() << "Decrypting blob" << Log::Field("encryptedPath", encryptedPath)
              << Log::Field("decryptedPath", decryptedPath);

    auto [key, keyErr] = GetKey();
    if (!keyErr.IsNone()) {
        return AOS_ERROR_WRAP(keyErr);
    }

    auto [encryptedSize, sizeErr] = fs::CalculateSize(*mAllocator, encryptedPath);
    if (!sizeErr.IsNone()) {
        return AOS_ERROR_WRAP(sizeErr);
    }

    if (encryptedSize < crypto::AESCipherItf::cGCMIVSize + crypto::AESCipherItf::cGCMTagSize) {
        return AOS_ERROR_WRAP(
            Error(ErrorEnum::eInvalidArgument, "encrypted file too short to contain an IV and a tag"));
    }

    // CTR releases exactly as much plaintext per C_DecryptUpdate as it is given ciphertext, so a buffer the
    // size of one ciphertext chunk is always big enough for whatever a single decrypt step releases.
    auto plainBuf = mAllocator->Allocate(cDecryptChunkSize);
    if (!plainBuf) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto freePlain = DeferRelease(mAllocator, [plainBuf](AllocatorItf* allocator) { allocator->Free(plainBuf); });

    Array<uint8_t> plaintext(static_cast<uint8_t*>(plainBuf), cDecryptChunkSize);

    // staged to a uniquely named sibling path and unconditionally removed, unless renamed into place on success
    // below: decryptedPath itself is never touched unless the whole operation succeeds. The random suffix keeps
    // concurrent decryptions of the same blob, or a stage left behind by a crash, from colliding with this one.
    StaticString<cStagedSuffixSize * 2> suffix;

    if (auto err = crypto::GenerateRandomString<cStagedSuffixSize>(suffix, *mRandom); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cFilePathLen> stagedPath;

    if (auto err = stagedPath.Format("%s.%s.tmp", decryptedPath.CStr(), suffix.CStr()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    fs::File output;

    // owner-only: it holds plaintext while decryption is still in progress.
    if (auto err = output.Open(stagedPath, fs::File::Mode::WriteNew, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // installed only once this call has created the stage, so it never removes a file it doesn't own.
    auto cleanUp = DeferRelease(&stagedPath, [](const auto* path) { (void)fs::Remove(*path); });

    FileChunkReceiver chunkReceiver(output, plaintext);

    if (auto err = StreamDecrypt(*key, encryptedPath, encryptedSize, chunkReceiver); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = output.Close(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return fs::Rename(stagedPath, decryptedPath);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

RetWithError<SharedPtr<crypto::PrivateKeyItf>> BlobDecryptor::GetKey()
{
    LockGuard lock(mKeyMutex);

    if (mKey) {
        return {mKey, ErrorEnum::eNone};
    }

    return FetchKey();
}

RetWithError<SharedPtr<crypto::PrivateKeyItf>> BlobDecryptor::FetchKey()
{
    auto certInfo = MakeUnique<CertInfo>(mAllocator);
    if (!certInfo) {
        return {nullptr, AOS_ERROR_WRAP(ErrorEnum::eNoMemory)};
    }

    if (auto err = mCertProvider->GetCert(mCertType, {}, {}, *certInfo); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    auto [key, err] = mCertLoader->LoadPrivKeyByURL(certInfo->mKeyURL);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    mKey = key;

    return {mKey, ErrorEnum::eNone};
}

Error BlobDecryptor::StreamDecrypt(const crypto::PrivateKeyItf& key, const String& encryptedPath, size_t encryptedSize,
    crypto::ChunkReceiverItf& chunkReceiver) const
{
    fs::File input;

    if (auto err = input.Open(encryptedPath, fs::File::Mode::Read); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticArray<uint8_t, crypto::AESCipherItf::cGCMIVSize> iv;

    if (auto err = input.ReadBlock(iv); !err.IsNone() && !err.Is(ErrorEnum::eEOF)) {
        return AOS_ERROR_WRAP(err);
    }

    if (iv.Size() != crypto::AESCipherItf::cGCMIVSize) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "encrypted file too short to contain an IV"));
    }

    StaticArray<uint8_t, cBlockSize> keystream;
    crypto::CTRDecryptionOptions     ctrOptions;

    // GHASH key H = AES_K(0^128), i.e. the keystream block for an all-zero counter.
    if (auto err = ctrOptions.mCounter.Resize(cBlockSize, 0); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = KeystreamBlock(key, ctrOptions, keystream); !err.IsNone()) {
        return err;
    }

    auto ghash = MakeUnique<GHash>(mAllocator, keystream);
    if (!ghash) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    // GCM encrypts the payload with plain AES-CTR, starting from counter block IV || 0x00000002, so CTR
    // decrypts it as is.
    if (auto err = MakeCounter(iv, cPayloadCounterSuffix, ctrOptions); !err.IsNone()) {
        return err;
    }

    // fixed-size chunk buffer, reused for every chunk read off disk: this is what keeps ciphertext-side
    // memory bounded regardless of how large the encrypted file is. Deliberately cDecryptChunkSize, not the
    // larger generic cFileChunkSize: some PKCS11 modules (TEE-backed ones especially) reject a single
    // C_DecryptUpdate call above a much smaller size with CKR_DEVICE_MEMORY.
    auto chunkBuf = mAllocator->Allocate(cDecryptChunkSize);
    if (!chunkBuf) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto freeChunk = DeferRelease(mAllocator, [chunkBuf](AllocatorItf* allocator) { allocator->Free(chunkBuf); });

    Array<uint8_t> chunk(static_cast<uint8_t*>(chunkBuf), cDecryptChunkSize);

    // the trailing tag is not passed on to the token: it's verified below instead.
    FileChunkProvider chunkProvider(
        input, chunk, encryptedSize - crypto::AESCipherItf::cGCMIVSize - crypto::AESCipherItf::cGCMTagSize, *ghash);

    if (auto err = key.StreamDecrypt(chunkProvider, crypto::DecryptionOptions {ctrOptions}, chunkReceiver);
        !err.IsNone()) {
        return err;
    }

    // the tag must cover the whole ciphertext, not just whatever the key happened to consume.
    if (!chunkProvider.IsExhausted()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "ciphertext not fully decrypted"));
    }

    StaticArray<uint8_t, crypto::AESCipherItf::cGCMTagSize> tag;

    if (auto err = input.ReadBlock(tag); !err.IsNone() && !err.Is(ErrorEnum::eEOF)) {
        return AOS_ERROR_WRAP(err);
    }

    if (tag.Size() != crypto::AESCipherItf::cGCMTagSize) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "encrypted file too short to contain a tag"));
    }

    // expected tag = AES_K(J0) ^ GHASH_H(ciphertext), J0 = IV || 0x00000001.
    if (auto err = MakeCounter(iv, cJ0CounterSuffix, ctrOptions); !err.IsNone()) {
        return err;
    }

    if (auto err = KeystreamBlock(key, ctrOptions, keystream); !err.IsNone()) {
        return err;
    }

    uint8_t expected[cBlockSize] = {};

    ghash->Finalize(expected);

    // constant time: don't reveal how many leading tag bytes matched.
    uint8_t diff = 0;

    for (size_t i = 0; i < cBlockSize; i++) {
        diff |= static_cast<uint8_t>(expected[i] ^ keystream[i] ^ tag[i]);
    }

    if (diff != 0) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "authentication tag mismatch"));
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::imagemanager
