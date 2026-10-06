/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_ITF_BLOBDECRYPTOR_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_ITF_BLOBDECRYPTOR_HPP_

#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>

namespace aos::sm::imagemanager {

/**
 * Decrypts blobs that were encrypted for this specific device using a symmetric key that never leaves
 * the device (see BlobDecryptor). The key is never exchanged over the network. The IV is not derived or
 * exchanged separately either: the encrypted file is the AES-256-GCM IV (12 bytes), the ciphertext and the
 * 16-byte authentication tag, so producing an encrypted blob needs no coordination beyond generating a
 * random IV and prepending it to the AES-GCM output.
 */
class BlobDecryptorItf {
public:
    /**
     * Decrypts a blob file using the device-local symmetric key. The encrypted file's first 12 bytes are
     * read as the IV; the remainder is the ciphertext followed by the authentication tag. Fails, leaving no
     * output, if the data doesn't authenticate (wrong key or modified blob).
     *
     * @param encryptedPath path to the encrypted file (IV, ciphertext, authentication tag).
     * @param decryptedPath path where the decrypted file will be written. May be encryptedPath itself: the
     * output is staged and only renamed into place, atomically replacing whatever is there, once it is
     * authenticated.
     * @return Error.
     */
    virtual Error Decrypt(const String& encryptedPath, const String& decryptedPath) = 0;

    /**
     * Destructor.
     */
    virtual ~BlobDecryptorItf() = default;
};

} // namespace aos::sm::imagemanager

#endif
