/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_BLOBDECRYPTOR_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_BLOBDECRYPTOR_HPP_

#include <core/common/config.hpp>
#include <core/common/crypto/itf/certloader.hpp>
#include <core/common/crypto/itf/rand.hpp>
#include <core/common/iamclient/itf/certprovider.hpp>
#include <core/common/tools/thread.hpp>

#include "itf/blobdecryptor.hpp"

namespace aos::sm::imagemanager {

/**
 * Chunk size used to stream ciphertext into the PKCS11 multi-part decrypt operation. See
 * AOS_CONFIG_IMAGEMANAGER_DECRYPT_CHUNK_SIZE for why this is deliberately smaller than the generic
 * aos::cFileChunkSize.
 */
constexpr auto cDecryptChunkSize = AOS_CONFIG_IMAGEMANAGER_DECRYPT_CHUNK_SIZE;

/**
 * Decrypts blobs using a device-local symmetric key that is never exchanged over the network or exposed to
 * this process, in any form: the key is stored as a CKO_SECRET_KEY object on the same PKCS11 token as a
 * certificate managed by IAM's cert module system (see certModules config), that cert module is dedicated
 * to locating the key (its registered key URL's token/library/PIN *and* id/label directly identify the
 * CKO_SECRET_KEY object), it is not used cryptographically itself. The key handle is resolved once, on
 * first use, and cached; the key's own value is never read - the actual decrypt call happens through it
 * (crypto::PrivateKeyItf::StreamDecrypt), this class handles the file I/O around it (splitting off the IV,
 * staging output, renaming into place once the authentication tag checks out). It is protected by the token's own
 * access control (PIN/login) the same way a certificate's private key is: both the key and the cert module are
 * provisioned onto the device's token out of band. Blobs are produced with AES-256-GCM; the IV (nonce)
 * travels with the data (first 12 bytes of the encrypted file) rather than being derived or exchanged
 * separately, since only the key itself needs to stay off the network. Encrypted file layout: IV (12 bytes) |
 * ciphertext | authentication tag (16 bytes).
 *
 * The blob is decrypted with AES-256-CTR on the token rather than with CKM_AES_GCM: TEE-backed PKCS11
 * modules (OP-TEE) hold the whole GCM plaintext in their limited TA heap until C_DecryptFinal verifies the
 * tag, failing anything larger than that heap with CKR_DEVICE_MEMORY, while CTR streams with constant token
 * memory. GCM's payload is plain CTR starting at counter block IV || 0x00000002, so the ciphertext decrypts
 * the same way. The tag is still verified, in software: tag = AES_K(J0) ^ GHASH_H(ciphertext), where both
 * key-dependent values, H = AES_K(0^128) and AES_K(J0) (J0 = IV || 0x00000001), are single CTR keystream
 * blocks obtained from the token, and GHASH is computed over the ciphertext as it is streamed. A wrong key
 * or a modified blob is rejected, and the staged output is deleted, exactly as with CKM_AES_GCM. H and
 * AES_K(J0) do pass through this process's memory: they don't reveal the key, but would allow forging a tag
 * for that IV, which an attacker able to read this process's memory could equally achieve by altering its
 * output.
 *
 * Decrypt reads the ciphertext off disk in chunks via key->StreamDecrypt, rather than buffering the whole
 * file, so the token/key this class is Init'd with must support that (pkcs11::AESPrivateKey does; there is
 * no whole-buffer fallback here), and writes decrypted data to the staged output file as the key releases it,
 * using a plaintext buffer the size of one ciphertext chunk.
 */
class BlobDecryptor : public BlobDecryptorItf {
public:
    /**
     * Initializes object instance.
     *
     * @param allocator allocator used for the plaintext buffer and the ciphertext chunk buffer.
     * @param certProvider provider of a certificate that identifies the token/id/label to read the key from.
     * @param certLoader loader used to resolve the token's session and read the secret key object.
     * @param random random generator used to give each staged output file a unique name.
     * @param certType certificate type/module id to fetch from certProvider.
     * @return Error.
     */
    Error Init(AllocatorItf& allocator, iamclient::CertProviderItf& certProvider, crypto::CertLoaderItf& certLoader,
        crypto::RandomItf& random, const String& certType);

    /**
     * Decrypts a blob file using the device-local symmetric key.
     *
     * @param encryptedPath path to the encrypted file (IV, ciphertext, authentication tag).
     * @param decryptedPath path where the decrypted file will be written.
     * @return Error.
     */
    Error Decrypt(const String& encryptedPath, const String& decryptedPath) override;

private:
    // Returns the symmetric key, resolving and caching it on first call.
    RetWithError<SharedPtr<crypto::PrivateKeyItf>> GetKey();
    RetWithError<SharedPtr<crypto::PrivateKeyItf>> FetchKey();

    // Reads the encrypted file's IV, then decrypts the ciphertext (excluding the trailing tag) via
    // key.StreamDecrypt, so it never needs to be fully buffered in memory (see
    // crypto::PrivateKeyItf::StreamDecrypt), and finally verifies the tag. Returns whatever error
    // key.StreamDecrypt returns, including ErrorEnum::eNotSupported if the key/token doesn't support it - there
    // is no whole-buffer fallback - or ErrorEnum::eInvalidChecksum if the tag doesn't match.
    Error StreamDecrypt(const crypto::PrivateKeyItf& key, const String& encryptedPath, size_t encryptedSize,
        crypto::ChunkReceiverItf& chunkReceiver) const;

    AllocatorItf*                    mAllocator {};
    Mutex                            mKeyMutex;
    iamclient::CertProviderItf*      mCertProvider {};
    crypto::CertLoaderItf*           mCertLoader {};
    crypto::RandomItf*               mRandom {};
    StaticString<cCertTypeLen>       mCertType;
    SharedPtr<crypto::PrivateKeyItf> mKey;
};

} // namespace aos::sm::imagemanager

#endif
