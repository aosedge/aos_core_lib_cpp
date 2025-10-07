/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_AES_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_AES_HPP_

#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>

namespace aos::crypto {

/**
 * AES cipher interface for 16-byte block encryption/decryption.
 */
class AESCipherItf {
public:
    /**
     * AES block.
     */
    using Block = StaticArray<uint8_t, 16>;

    /**
     * Destructor.
     */
    virtual ~AESCipherItf() = default;

    /**
     * Encrypts a 16-byte block.
     *
     * @param input  input block.
     * @param[out] output encrypted block.
     * @return Error.
     */
    virtual Error EncryptBlock(const Block& input, Block& output) = 0;

    /**
     * Decrypts a 16-byte block.
     *
     * @param input  input block.
     * @param[out] output decrypted block.
     * @return Error.
     */
    virtual Error DecryptBlock(const Block& input, Block& output) = 0;

    /**
     * Finalizes encription/decryption.
     *
     * @param[out] output final block.
     * @return Error.
     */
    virtual Error Finalize(Block& output) = 0;
};

/**
 * Interface for AES encoding/decoding.
 */
class AESEncoderDecoderItf {
public:
    /**
     * Destructor.
     */
    virtual ~AESEncoderDecoderItf() = default;

    /**
     * Creates a new AES encoder.
     *
     * @param mode AES mode: "CBC" supported only.
     * @param key encryption key.
     * @param iv initialization vector: must be 16 bytes for CBC mode.
     * @return RetWithError<UniquePtr<AESCipherItf>>.
     */
    virtual RetWithError<UniquePtr<AESCipherItf>> CreateAESEncoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv)
        = 0;

    /**
     * Creates a new AES decoder.
     *
     * @param mode AES mode: "CBC" supported only.
     * @param key decryption key.
     * @param iv initialization vector: must be 16 bytes for CBC mode.
     * @return RetWithError<UniquePtr<AESCipherItf>>.
     */
    virtual RetWithError<UniquePtr<AESCipherItf>> CreateAESDecoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv)
        = 0;
};

} // namespace aos::crypto

#endif // AOS_CORE_COMMON_CRYPTO_ITF_AES_HPP_
