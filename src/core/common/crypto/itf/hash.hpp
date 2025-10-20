/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_HASH_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_HASH_HPP_

#include <core/common/tools/enum.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>

namespace aos::crypto {

/**
 * Maximum size of SHA2 digest.
 */
constexpr auto cSHA2DigestSize = AOS_CONFIG_CRYPTO_SHA2_DIGEST_SIZE;

/**
 * Maximum size of SHA1 digest.
 */
constexpr auto cSHA1DigestSize = AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE;

/**
 * Maximum size of input data for SHA1 hash calculation.
 */
constexpr auto cSHA1InputDataSize = AOS_CONFIG_CRYPTO_SHA1_INPUT_SIZE;

/**
 * SHA256 size.
 */
constexpr auto cSHA256Size = 32;

/**
 * SHA384 size.
 */
constexpr auto cSHA384Size = 48;

/**
 * SHA3-224 size.
 */
constexpr auto cSHA3_224Size = 28;

/**
 * Supported hash functions.
 */
class HashType {
public:
    enum class Enum {
        eSHA1,
        eSHA224,
        eSHA256,
        eSHA384,
        eSHA512,
        eSHA512_224,
        eSHA512_256,
        eSHA3_224,
        eSHA3_256,
        eNone,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sContentTypeStrings[] = {
            "SHA1",
            "SHA224",
            "SHA256",
            "SHA384",
            "SHA512",
            "SHA512-224",
            "SHA512-256",
            "SHA3-224",
            "SHA3-256",
            "NONE",
        };
        return Array<const char* const>(sContentTypeStrings, ArraySize(sContentTypeStrings));
    };
};

using HashEnum = HashType::Enum;
using Hash     = EnumStringer<HashType>;

/**
 * Hash interface.
 */
class HashItf {
public:
    /**
     * Updates hash with input data.
     *
     * @param data input data.
     * @return Error.
     */
    virtual Error Update(const Array<uint8_t>& data) = 0;

    /**
     * Finalizes hash calculation.
     *
     * @param[out] hash result hash.
     * @return Error.
     */
    virtual Error Finalize(Array<uint8_t>& hash) = 0;

    /**
     * Destructor.
     */
    virtual ~HashItf() = default;
};

/**
 * Hasher interface.
 */
class HasherItf {
public:
    /**
     * Creates hash instance.
     *
     * @param algorithm hash algorithm.
     * @return RetWithError<UniquePtr<HashItf>>.
     */
    virtual RetWithError<UniquePtr<HashItf>> CreateHash(Hash algorithm) = 0;

    /**
     * Destructor.
     */
    virtual ~HasherItf() = default;
};

} // namespace aos::crypto

#endif
