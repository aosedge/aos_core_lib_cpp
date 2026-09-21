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
     * AES block size.
     */
    static constexpr size_t cBlockSize = 16;

    /**
     * GCM authentication tag size.
     */
    static constexpr size_t cGCMTagSize = 16;

    /**
     * GCM initialization vector (nonce) size.
     */
    static constexpr size_t cGCMIVSize = 12;

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
    virtual Error EncryptBlock(const Array<uint8_t>& input, Array<uint8_t>& output) = 0;

    /**
     * Decrypts a block.
     *
     * Authenticated modes (GCM): the returned plaintext is NOT authenticated until Finalize succeeds. It must not be
     * used, persisted or exposed before that; the caller is responsible for staging it (e.g. in a temporary file or
     * buffer) and discarding everything if SetTag or Finalize fails.
     *
     * @param input  input block.
     * @param[out] output decrypted block.
     * @return Error.
     */
    virtual Error DecryptBlock(const Array<uint8_t>& input, Array<uint8_t>& output) = 0;

    /**
     * Finalizes encription/decryption.
     *
     * @param[out] output final block.
     * @return Error.
     */
    virtual Error Finalize(Array<uint8_t>& output) = 0;

    /**
     * Sets the expected authentication tag of a decoder. Authenticated modes (GCM) only: must be called before
     * Finalize, which then fails if the data doesn't match the tag. All plaintext returned by DecryptBlock must be
     * discarded on such a failure.
     *
     * @param tag authentication tag.
     * @return Error.
     */
    virtual Error SetTag(const Array<uint8_t>& tag)
    {
        (void)tag;

        return ErrorEnum::eNotSupported;
    }

    /**
     * Returns the authentication tag of an encoder. Authenticated modes (GCM) only: must be called after
     * Finalize.
     *
     * @param[out] tag authentication tag.
     * @return Error.
     */
    virtual Error GetTag(Array<uint8_t>& tag)
    {
        (void)tag;

        return ErrorEnum::eNotSupported;
    }
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
     * @param mode AES mode: "CBC" (PKCS7 padded) or "GCM" (no padding, authenticated).
     * @param key encryption key.
     * @param iv initialization vector: 16 bytes for CBC mode, 12 bytes for GCM mode.
     * @return RetWithError<UniquePtr<AESCipherItf>>.
     */
    virtual RetWithError<UniquePtr<AESCipherItf>> CreateAESEncoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv)
        = 0;

    /**
     * Creates a new AES decoder.
     *
     * @param mode AES mode: "CBC" (PKCS7 padded) or "GCM" (no padding, authenticated).
     * @param key decryption key.
     * @param iv initialization vector: 16 bytes for CBC mode, 12 bytes for GCM mode.
     * @return RetWithError<UniquePtr<AESCipherItf>>.
     */
    virtual RetWithError<UniquePtr<AESCipherItf>> CreateAESDecoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv)
        = 0;
};

} // namespace aos::crypto

#endif
