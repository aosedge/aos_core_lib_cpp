/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_PRIVKEY_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_PRIVKEY_HPP_

#include <core/common/config.hpp>
#include <core/common/tools/enum.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/variant.hpp>

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

    static const Array<const char* const> GetStrings()
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
    int mKeySize = 0;
};

/**
 * OAEP decryption options.
 */
struct OAEPDecryptionOptions {
    Hash mHash;
};

/**
 * Decryption options.
 */
using DecryptionOptions = Variant<PKCS1v15DecryptionOptions, OAEPDecryptionOptions>;

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

#endif // AOS_CORE_COMMON_CRYPTO_ITF_PRIVKEY_HPP_
