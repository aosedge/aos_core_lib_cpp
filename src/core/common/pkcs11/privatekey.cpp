/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "privatekey.hpp"

namespace aos::pkcs11 {

/***********************************************************************************************************************
 * PKCS11RSAPrivateKey
 **********************************************************************************************************************/

constexpr uint8_t PKCS11RSAPrivateKey::cSHA1Prefix[];
constexpr uint8_t PKCS11RSAPrivateKey::cSHA224Prefix[];
constexpr uint8_t PKCS11RSAPrivateKey::cSHA256Prefix[];
constexpr uint8_t PKCS11RSAPrivateKey::cSHA384Prefix[];
constexpr uint8_t PKCS11RSAPrivateKey::cSHA512Prefix[];

PKCS11RSAPrivateKey::PKCS11RSAPrivateKey(AllocatorItf& allocator, const SharedPtr<SessionContext>& session,
    ObjectHandle privKeyHandle, const crypto::RSAPublicKey& pubKey)
    : mAllocator(allocator)
    , mSession(session)
    , mPrivKeyHandle(privKeyHandle)
    , mPublicKey(pubKey)
{
    LOG_DBG() << "Create RSA private key";
}

const crypto::PublicKeyItf& PKCS11RSAPrivateKey::GetPublic() const
{
    return mPublicKey;
}

Error PKCS11RSAPrivateKey::Sign(
    const Array<uint8_t>& digest, const crypto::SignOptions& options, Array<uint8_t>& signature) const
{
    auto t = MakeUnique<StaticArray<uint8_t, crypto::cSHA2DigestSize + cMaxPrefixSize>>(&mAllocator);
    if (!t) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    (void)t->Append(GetPrefix(options.mHash));
    (void)t->Append(digest);

    CK_MECHANISM mechanism = {CKM_RSA_PKCS, nullptr, 0};

    return mSession->Sign(&mechanism, mPrivKeyHandle, *t, signature);
}

Error PKCS11RSAPrivateKey::Decrypt(
    const Array<uint8_t>& cipher, const crypto::DecryptionOptions& options, Array<uint8_t>& result) const
{

    PCKS11RSAMechConverter visitor;

    auto [mech, err] = options.ApplyVisitor(visitor);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return mSession->Decrypt(&mech, mPrivKeyHandle, cipher, result);
}

Array<uint8_t> PKCS11RSAPrivateKey::GetPrefix(const crypto::Hash& hash) const
{
    switch (hash.GetValue()) {
    case crypto::HashEnum::eSHA1:
        return Array<uint8_t>(cSHA1Prefix, sizeof(cSHA1Prefix));
    case crypto::HashEnum::eSHA224:
        return Array<uint8_t>(cSHA224Prefix, sizeof(cSHA224Prefix));
    case crypto::HashEnum::eSHA256:
        return Array<uint8_t>(cSHA256Prefix, sizeof(cSHA256Prefix));
    case crypto::HashEnum::eSHA384:
        return Array<uint8_t>(cSHA384Prefix, sizeof(cSHA384Prefix));
    case crypto::HashEnum::eSHA512:
        return Array<uint8_t>(cSHA512Prefix, sizeof(cSHA512Prefix));
    default:
        assert(false);
        return Array<uint8_t>(nullptr, 0);
    }
}

/***********************************************************************************************************************
 * PCKS11RSAMechConverter
 **********************************************************************************************************************/

RetWithError<CK_MECHANISM> PCKS11RSAMechConverter::Visit(const crypto::PKCS1v15DecryptionOptions& options) const
{
    if (options.mKeySize != 0) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
    }

    return CK_MECHANISM {CKM_RSA_PKCS, nullptr, 0};
}

RetWithError<CK_MECHANISM> PCKS11RSAMechConverter::Visit(const crypto::OAEPDecryptionOptions& options) const
{
    CK_MECHANISM_TYPE    hashAlg = 0;
    CK_RSA_PKCS_MGF_TYPE mgf     = 0;

    switch (options.mHash.GetValue()) {
    case crypto::HashEnum::eSHA1:
        hashAlg = CKM_SHA_1;
        mgf     = CKG_MGF1_SHA1;
        break;

    case crypto::HashEnum::eSHA256:
        hashAlg = CKM_SHA256;
        mgf     = CKG_MGF1_SHA256;
        break;

    case crypto::HashEnum::eSHA384:
        hashAlg = CKM_SHA384;
        mgf     = CKG_MGF1_SHA384;
        break;

    case crypto::HashEnum::eSHA512:
        hashAlg = CKM_SHA512;
        mgf     = CKG_MGF1_SHA512;
        break;

    case crypto::HashEnum::eSHA3_224:
        hashAlg = CKM_SHA3_224;
        mgf     = CKG_MGF1_SHA3_224;
        break;

    case crypto::HashEnum::eSHA3_256:
        hashAlg = CKM_SHA3_256;
        mgf     = CKG_MGF1_SHA3_256;
        break;

    case crypto::HashEnum::eSHA512_224:
    case crypto::HashEnum::eSHA512_256:
    default:
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
    }

    mOAEPParams.hashAlg         = hashAlg;
    mOAEPParams.mgf             = mgf;
    mOAEPParams.source          = CKZ_DATA_SPECIFIED;
    mOAEPParams.pSourceData     = nullptr;
    mOAEPParams.ulSourceDataLen = 0;

    CK_MECHANISM mech = {CKM_RSA_PKCS_OAEP, &mOAEPParams, sizeof(mOAEPParams)};

    return mech;
}

RetWithError<CK_MECHANISM> PCKS11RSAMechConverter::Visit(const crypto::GCMDecryptionOptions& options) const
{
    (void)options;

    return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
}

RetWithError<CK_MECHANISM> PCKS11RSAMechConverter::Visit(const crypto::CTRDecryptionOptions& options) const
{
    (void)options;

    return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
}

/***********************************************************************************************************************
 * PKCS11ECDSAPrivateKey
 **********************************************************************************************************************/

PKCS11ECDSAPrivateKey::PKCS11ECDSAPrivateKey(const SharedPtr<SessionContext>& session,
    const crypto::x509::ProviderItf& cryptoProvider, ObjectHandle privKeyHandle, const crypto::ECDSAPublicKey& pubKey)
    : mSession(session)
    , mPrivKeyHandle(privKeyHandle)
    , mPublicKey(pubKey)
{
    (void)cryptoProvider;

    LOG_DBG() << "Create EC private key";
}

const crypto::PublicKeyItf& PKCS11ECDSAPrivateKey::GetPublic() const
{
    return mPublicKey;
}

Error PKCS11ECDSAPrivateKey::Sign(
    const Array<uint8_t>& digest, const crypto::SignOptions& options, Array<uint8_t>& signature) const
{
    (void)options;

    CK_MECHANISM mechanism = {CKM_ECDSA, nullptr, 0};

    return mSession->Sign(&mechanism, mPrivKeyHandle, digest, signature);
}

/***********************************************************************************************************************
 * PKCS11AESMechConverter
 **********************************************************************************************************************/

RetWithError<CK_MECHANISM> PKCS11AESMechConverter::Visit(const crypto::PKCS1v15DecryptionOptions& options) const
{
    (void)options;

    return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
}

RetWithError<CK_MECHANISM> PKCS11AESMechConverter::Visit(const crypto::OAEPDecryptionOptions& options) const
{
    (void)options;

    return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
}

RetWithError<CK_MECHANISM> PKCS11AESMechConverter::Visit(const crypto::GCMDecryptionOptions& options) const
{
    if (options.mIV.Size() != crypto::AESCipherItf::cGCMIVSize) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }

    mGCMParams.pIv       = const_cast<uint8_t*>(options.mIV.Get()); // NOSONAR cpp:M23_090
    mGCMParams.ulIvLen   = static_cast<CK_ULONG>(options.mIV.Size());
    mGCMParams.ulIvBits  = static_cast<CK_ULONG>(options.mIV.Size() * 8);
    mGCMParams.ulTagBits = crypto::AESCipherItf::cGCMTagSize * 8;

    return CK_MECHANISM {CKM_AES_GCM, &mGCMParams, sizeof(mGCMParams)};
}

RetWithError<CK_MECHANISM> PKCS11AESMechConverter::Visit(const crypto::CTRDecryptionOptions& options) const
{
    if (options.mCounter.Size() != sizeof(mCTRParams.cb)) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }

    mCTRParams.ulCounterBits = mCTRCounterBits;
    (void)memcpy(mCTRParams.cb, options.mCounter.Get(), sizeof(mCTRParams.cb));

    return CK_MECHANISM {CKM_AES_CTR, &mCTRParams, sizeof(mCTRParams)};
}

/***********************************************************************************************************************
 * AESPrivateKey
 **********************************************************************************************************************/

AESPrivateKey::AESPrivateKey(const SharedPtr<SessionContext>& session, ObjectHandle keyHandle)
    : mSession(session)
    , mKeyHandle(keyHandle)
{
    LOG_DBG() << "Create AES secret key";
}

const crypto::PublicKeyItf& AESPrivateKey::GetPublic() const
{
    return mNoPublicKey;
}

Error AESPrivateKey::Sign(
    const Array<uint8_t>& digest, const crypto::SignOptions& options, Array<uint8_t>& signature) const
{
    (void)digest;
    (void)options;
    (void)signature;

    return AOS_ERROR_WRAP(ErrorEnum::eNotSupported);
}

Error AESPrivateKey::Decrypt(
    const Array<uint8_t>& cipher, const crypto::DecryptionOptions& options, Array<uint8_t>& result) const
{
    return WithMechanism(
        options, [&](CK_MECHANISM& mech) { return mSession->Decrypt(&mech, mKeyHandle, cipher, result); });
}

Error AESPrivateKey::StreamDecrypt(crypto::ChunkProviderItf& chunkProvider, const crypto::DecryptionOptions& options,
    crypto::ChunkReceiverItf& chunkReceiver) const
{
    return WithMechanism(options, [&](CK_MECHANISM& mech) {
        return mSession->DecryptMultiPart(&mech, mKeyHandle, chunkProvider, chunkReceiver);
    });
}

template <typename Op>
Error AESPrivateKey::WithMechanism(const crypto::DecryptionOptions& options, Op op) const
{
    // both outlive op: the mechanism's parameters point into the visitor that produced it.
    PKCS11AESMechConverter visitor(cCTRCounterBits);
    PKCS11AESMechConverter fallbackVisitor(cOPTEECTRCounterBits);

    auto [mech, err] = options.ApplyVisitor(visitor);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mech.mechanism == CKM_AES_CTR) {
        CK_ULONG counterBits = 0;

        if (auto counterErr = GetCTRCounterBits(counterBits); !counterErr.IsNone()) {
            return counterErr;
        }

        if (counterBits == cOPTEECTRCounterBits) {
            Tie(mech, err) = options.ApplyVisitor(fallbackVisitor);
            if (!err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return op(mech);
}

Error AESPrivateKey::GetCTRCounterBits(CK_ULONG& counterBits) const
{
    constexpr auto cBlockSize = crypto::AESCipherItf::cBlockSize;

    LockGuard lock {mMutex};

    if (mCTRCounterBits == 0) {
        // probed with a single-shot decrypt of one block, so whatever the token rejects, no caller data has been
        // consumed yet: the operation that needs the counter width only starts once it is known.
        CK_AES_CTR_PARAMS params {cCTRCounterBits, {}};
        CK_MECHANISM      mech {CKM_AES_CTR, &params, sizeof(params)};

        StaticArray<uint8_t, cBlockSize> zeros;
        StaticArray<uint8_t, cBlockSize> block;

        if (auto err = zeros.Resize(zeros.MaxSize(), 0); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = mSession->Decrypt(&mech, mKeyHandle, zeros, block); err.IsNone()) {
            mCTRCounterBits = cCTRCounterBits;
        } else if (err.Errno() == static_cast<int32_t>(CKR_MECHANISM_PARAM_INVALID)) {
            LOG_DBG() << "Token rejected CTR counter bits, falling back"
                      << Log::Field("counterBits", cOPTEECTRCounterBits);

            if (auto verifyErr = VerifyFallbackCounter(); !verifyErr.IsNone()) {
                return verifyErr;
            }

            mCTRCounterBits = cOPTEECTRCounterBits;
        } else {
            return AOS_ERROR_WRAP(err);
        }
    }

    counterBits = mCTRCounterBits;

    return ErrorEnum::eNone;
}

Error AESPrivateKey::VerifyFallbackCounter() const
{
    constexpr auto cBlockSize = crypto::AESCipherItf::cBlockSize;

    // T = 0^96 || 0xFFFFFFFF: its successor T + 1 = 0^95 1 || 0^32 needs a carry across the low 32 bits, while a
    // 1-bit counter would wrap to T - 1 instead. Neither block is a keystream block of any GCM message with a
    // 96-bit IV within GCM's length limit, and AES outputs don't reveal the key.
    CK_AES_CTR_PARAMS params {cOPTEECTRCounterBits, {}};
    CK_MECHANISM      mech {CKM_AES_CTR, &params, sizeof(params)};

    (void)memset(params.cb + cBlockSize - 4, 0xff, 4);

    StaticArray<uint8_t, cBlockSize * 2> zeros;
    StaticArray<uint8_t, cBlockSize * 2> twoBlocks;
    StaticArray<uint8_t, cBlockSize>     nextBlock;

    if (auto err = zeros.Resize(zeros.MaxSize(), 0); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mSession->Decrypt(&mech, mKeyHandle, zeros, twoBlocks); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    (void)memset(params.cb, 0, sizeof(params.cb));
    params.cb[cBlockSize - 5] = 0x01;

    if (auto err = mSession->Decrypt(&mech, mKeyHandle, Array<uint8_t>(zeros.Get(), cBlockSize), nextBlock);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (twoBlocks.Size() != cBlockSize * 2 || nextBlock.Size() != cBlockSize
        || memcmp(twoBlocks.Get() + cBlockSize, nextBlock.Get(), cBlockSize) != 0) {
        return AOS_ERROR_WRAP(
            Error(ErrorEnum::eNotSupported, "token's CTR counter doesn't span the whole counter block"));
    }

    return ErrorEnum::eNone;
}

} // namespace aos::pkcs11
