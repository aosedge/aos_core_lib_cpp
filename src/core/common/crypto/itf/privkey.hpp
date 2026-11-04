/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_PRIVKEY_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_PRIVKEY_HPP_

#include <core/common/config.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/enum.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/variant.hpp>

#include "aes.hpp"
#include "hash.hpp"

namespace aos::crypto {

/**
 * RSA modulus size.
 */
constexpr auto cRSAModulusSize = AOS_CONFIG_CRYPTO_RSA_MODULUS_SIZE;

/**
 * Size of RSA public exponent.
 */
constexpr auto cRSAPubExponentSize = AOS_CONFIG_CRYPTO_RSA_PUB_EXPONENT_SIZE;

/**
 * ECDSA params OID size.
 */
constexpr auto cECDSAParamsOIDSize = AOS_CONFIG_CRYPTO_ECDSA_PARAMS_OID_SIZE;

/**
 * DER-encoded X9.62 ECPoint.
 */
constexpr auto cECDSAPointDERSize = AOS_CONFIG_CRYPTO_ECDSA_POINT_DER_SIZE;

/**
 * Supported key types.
 */
class KeyAlgorithm {
public:
    enum class Enum { eRSA, eECDSA };

    static Array<const char* const> GetStrings()
    {
        static const char* const sContentTypeStrings[] = {"RSA", "ECDSA"};
        return Array<const char* const>(sContentTypeStrings, ArraySize(sContentTypeStrings));
    };
};

using KeyTypeEnum = KeyAlgorithm::Enum;
using KeyType     = EnumStringer<KeyAlgorithm>;

/**
 * Options being used while signing.
 */
struct SignOptions {
    /**
     * Hash function to be used when signing.
     */
    Hash mHash;
};

/**
 * PKCS1v15 decryption options.
 */
struct PKCS1v15DecryptionOptions {
    int32_t mKeySize = 0;
};

/**
 * OAEP decryption options.
 */
struct OAEPDecryptionOptions {
    Hash mHash;
};

/**
 * AES-GCM decryption options.
 */
struct GCMDecryptionOptions {
    /**
     * GCM initialization vector (nonce): AESCipherItf::cGCMIVSize (12) bytes.
     */
    StaticArray<uint8_t, AESCipherItf::cGCMIVSize> mIV;
};

/**
 * AES-CTR decryption options.
 */
struct CTRDecryptionOptions {
    /**
     * Initial counter block: AESCipherItf::cBlockSize (16) bytes, incremented as a single big-endian 128-bit
     * value for each subsequent block.
     */
    StaticArray<uint8_t, AESCipherItf::cBlockSize> mCounter;
};

/**
 * Decryption options.
 */
using DecryptionOptions
    = Variant<PKCS1v15DecryptionOptions, OAEPDecryptionOptions, GCMDecryptionOptions, CTRDecryptionOptions>;

/**
 * Supplies data chunks on demand to a streaming operation (see PrivateKeyItf::StreamDecrypt), so the
 * whole input never needs to be held in memory at once. The provider owns the chunk buffer itself (sized
 * and allocated however its own implementation sees fit) rather than requiring the caller to supply one:
 * on a stack-constrained target, a caller-supplied buffer sized for a whole chunk (tens of KiB) can blow a
 * function's stack budget, whereas a concrete provider (e.g. one backed by a file) can size its buffer via
 * whatever AllocatorItf it already has.
 */
class ChunkProviderItf {
public:
    /**
     * Returns the next chunk of data. The returned array is only valid until the next call to
     * NextChunk or until this provider is destroyed - callers must consume it before calling again.
     *
     * @return RetWithError<Array<uint8_t>>. ErrorEnum::eEOF (with an empty array) once no more data
     * remains; a call that delivers the last chunk returns ErrorEnum::eNone, even if that chunk is
     * short.
     */
    virtual RetWithError<Array<uint8_t>> NextChunk() = 0;

    /**
     * Destroys object instance.
     */
    virtual ~ChunkProviderItf() = default;
};

/**
 * Receives data chunks produced by a streaming operation (see PrivateKeyItf::StreamDecrypt), so the caller
 * can handle output (e.g. write it to a file) as it comes back instead of collecting it into a single array.
 * Like ChunkProviderItf, the receiver owns the buffer output is written into: the streaming operation writes
 * each produced chunk into GetBuffer() and then hands it back via OnChunk.
 */
class ChunkReceiverItf {
public:
    /**
     * Returns the buffer the next output chunk is written into. Its MaxSize bounds how much output a single
     * step of the operation may produce: many PKCS11 tokens only release AEAD-decrypted data all at once,
     * at the very end, so for those it must have capacity for the whole output.
     *
     * @return Array<uint8_t>&.
     */
    virtual Array<uint8_t>& GetBuffer() = 0;

    /**
     * Handles the next output chunk. chunk is a view into GetBuffer() and is only valid until this call
     * returns. Never called with an empty chunk.
     *
     * @param chunk output chunk.
     * @return Error. Any error aborts the streaming operation and is returned to its caller.
     */
    virtual Error OnChunk(const Array<uint8_t>& chunk) = 0;

    /**
     * Destroys object instance.
     */
    virtual ~ChunkReceiverItf() = default;
};

/**
 * Public key interface.
 */
class PublicKeyItf {
public:
    /**
     * Returns type of a public key.
     *
     * @return KeyType,
     */
    virtual KeyType GetKeyType() const = 0;

    /**
     * Tests whether current key is equal to the provided one.
     *
     * @param pubKey public key.
     * @return bool.
     */
    virtual bool IsEqual(const PublicKeyItf& pubKey) const = 0;

    /**
     * Destroys object instance.
     */
    virtual ~PublicKeyItf() = default;
};

/**
 * Private key interface.
 */
class PrivateKeyItf {
public:
    /**
     * Returns public part of a private key.
     *
     * @return const PublicKeyItf&.
     */
    virtual const PublicKeyItf& GetPublic() const = 0;

    /**
     * Calculates a signature of a given digest.
     *
     * @param digest input hash digest.
     * @param options signing options.
     * @param[out] signature result signature.
     * @return Error.
     */
    virtual Error Sign(const Array<uint8_t>& digest, const SignOptions& options, Array<uint8_t>& signature) const = 0;

    /**
     * Decrypts a cipher message.
     *
     * @param cipher encrypted message.
     * @param options decryption options.
     * @param[out] result decoded message.
     * @return Error.
     */
    virtual Error Decrypt(const Array<uint8_t>& cipher, const DecryptionOptions& options, Array<uint8_t>& result) const
        = 0;

    /**
     * Decrypts a cipher message supplied incrementally by chunkProvider, so the whole ciphertext
     * doesn't need to be held in memory at once - only whatever chunk size chunkProvider hands back
     * at a time. Decrypted data is handed to chunkReceiver as it becomes available. This doesn't
     * necessarily bound *output* memory the same way: many PKCS11 tokens only release AEAD-decrypted
     * data once the authentication tag has been verified, all at once, at the very end, so
     * chunkReceiver's buffer must still have capacity for the whole plaintext regardless of how the
     * input was chunked (see pkcs11::AESPrivateKey for the concrete behavior). Note that, for tokens
     * that do release data early, chunkReceiver may get plaintext before the tag is verified: it must
     * not trust it until StreamDecrypt returns successfully. Default implementation returns
     * ErrorEnum::eNotSupported; only override it where streaming input actually helps.
     *
     * @param chunkProvider supplies the cipher message in chunks.
     * @param options decryption options.
     * @param chunkReceiver receives the decoded message in chunks.
     * @return Error.
     */
    virtual Error StreamDecrypt(
        ChunkProviderItf& chunkProvider, const DecryptionOptions& options, ChunkReceiverItf& chunkReceiver) const
    {
        (void)chunkProvider;
        (void)options;
        (void)chunkReceiver;

        return ErrorEnum::eNotSupported;
    }

    /**
     * Destroys object instance.
     */
    virtual ~PrivateKeyItf() = default;
};

/**
 * RSA public key.
 */
class RSAPublicKey : public PublicKeyItf {
public:
    /**
     * Constructs object instance.
     *
     * @param n modulus.
     * @param e public exponent.
     */
    RSAPublicKey(const Array<uint8_t>& n, const Array<uint8_t>& e)
        : mN(n)
        , mE(e)
    {
    }

    /**
     * Returns type of a public key.
     *
     * @return KeyType,
     */
    KeyType GetKeyType() const override { return KeyTypeEnum::eRSA; }

    /**
     * Tests whether current key is equal to the provided one.
     *
     * @param pubKey public key.
     * @return bool.
     */
    bool IsEqual(const PublicKeyItf& pubKey) const override
    {
        if (pubKey.GetKeyType() != KeyTypeEnum::eRSA) {
            return false;
        }

        const auto& otherKey = static_cast<const RSAPublicKey&>(pubKey);

        return otherKey.mN == mN && otherKey.mE == mE;
    }

    /**
     * Returns RSA public modulus.
     *
     * @return const Array<uint8_t>&.
     */
    const Array<uint8_t>& GetN() const { return mN; }

    /**
     * Returns RSA public exponent.
     *
     * @return const Array<uint8_t>&.
     */
    const Array<uint8_t>& GetE() const { return mE; }

private:
    StaticArray<uint8_t, cRSAModulusSize>     mN;
    StaticArray<uint8_t, cRSAPubExponentSize> mE;
};

/**
 * ECDSA public key.
 */
class ECDSAPublicKey : public PublicKeyItf {
public:
    /**
     * Constructs object instance.
     *
     * @param n modulus.
     * @param e public exponent.
     */
    ECDSAPublicKey(const Array<uint8_t>& params, const Array<uint8_t>& point)
        : mECParamsOID(params)
        , mECPoint(point)
    {
    }

    /**
     * Returns type of a public key.
     *
     * @return KeyType,
     */
    KeyType GetKeyType() const override { return KeyTypeEnum::eECDSA; }

    /**
     * Tests whether current key is equal to the provided one.
     *
     * @param pubKey public key.
     * @return bool.
     */
    bool IsEqual(const PublicKeyItf& pubKey) const override
    {
        if (pubKey.GetKeyType() != KeyTypeEnum::eECDSA) {
            return false;
        }

        const auto& otherKey = static_cast<const ECDSAPublicKey&>(pubKey);

        return otherKey.mECParamsOID == mECParamsOID && otherKey.mECPoint == mECPoint;
    }

    /**
     * Returns ECDSA params OID.
     *
     * @return const Array<uint8_t>&.
     */
    const Array<uint8_t>& GetECParamsOID() const { return mECParamsOID; }

    /**
     * Returns ECDSA point.
     *
     * @return const Array<uint8_t>&.
     */
    const Array<uint8_t>& GetECPoint() const { return mECPoint; }

private:
    StaticArray<uint8_t, cECDSAParamsOIDSize> mECParamsOID;
    StaticArray<uint8_t, cECDSAPointDERSize>  mECPoint;
};

} // namespace aos::crypto

#endif
