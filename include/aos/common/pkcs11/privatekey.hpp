/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_PKCS11_PRIVATEKEY_HPP_
#define AOS_PKCS11_PRIVATEKEY_HPP_

#include "aos/common/pkcs11/pkcs11.hpp"

namespace aos::pkcs11 {

/**
 * PKCS11 RSA private key.
 */
class PKCS11RSAPrivateKey : public crypto::PrivateKeyItf {
public:
    /**
     * Constructs object instance.
     *
     * @param session session context.
     * @param privKeyHandle private key handle.
     * @param pubKey public key.
     */
    PKCS11RSAPrivateKey(
        const SharedPtr<SessionContext>& session, ObjectHandle privKeyHandle, const crypto::RSAPublicKey& pubKey);

    /**
     * Returns public part of a private key.
     *
     * @return const PublicKeyItf&.
     */
    const crypto::PublicKeyItf& GetPublic() const override;

    /**
     * Calculates a signature of a given digest.
     * Currently supported PKCS#1v1.5 implementation only.
     *
     * @param digest input hash digest.
     * @param options signing options.
     * @param[out] signature result signature.
     * @return Error.
     */
    Error Sign(
        const Array<uint8_t>& digest, const crypto::SignOptions& options, Array<uint8_t>& signature) const override;

    /**
     * Decrypts a cipher message.
     * Implemented PKCS#1v1.5 decryption only.
     *
     * @param cipher encrypted message.
     * @param[out] result decoded message.
     * @return Error.
     */
    Error Decrypt(const Array<uint8_t>& cipher, Array<uint8_t>& result) const override;

private:
    static constexpr uint8_t cSHA1Prefix[]
        = {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14};
    static constexpr uint8_t cSHA224Prefix[] = {0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
        0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1c};
    static constexpr uint8_t cSHA256Prefix[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
        0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
    static constexpr uint8_t cSHA384Prefix[] = {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
        0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30};
    static constexpr uint8_t cSHA512Prefix[] = {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
        0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};

    static constexpr auto cMaxPrefixSize = Max(sizeof(cSHA1Prefix), sizeof(cSHA224Prefix), sizeof(cSHA256Prefix),
        sizeof(cSHA384Prefix), sizeof(cSHA512Prefix));

    Array<uint8_t> GetPrefix(crypto::Hash hash) const;

    mutable StaticAllocator<sizeof(StaticArray<uint8_t, crypto::cSHA2DigestSize + cMaxPrefixSize>)> mAllocator;

    SharedPtr<SessionContext> mSession;
    ObjectHandle              mPrivKeyHandle;
    crypto::RSAPublicKey      mPublicKey;
};

/**
 * PKCS11 ECDSA private key.
 */
class PKCS11ECDSAPrivateKey : public crypto::PrivateKeyItf {
public:
    /**
     * Constructs object instance.
     *
     * @param session session context.
     * @param cryptoProvider provider of crypto interface.
     * @param privKeyHandle private key handle.
     * @param pubKey public key.
     */
    PKCS11ECDSAPrivateKey(const SharedPtr<SessionContext>& session, crypto::x509::ProviderItf& cryptoProvider,
        ObjectHandle privKeyHandle, const crypto::ECDSAPublicKey& pubKey);

    /**
     * Returns public part of a private key.
     *
     * @return const PublicKeyItf&.
     */
    const crypto::PublicKeyItf& GetPublic() const override;

    /**
     * Calculates a signature of a given digest.
     *
     * @param digest input hash digest.
     * @param options signing options.
     * @param[out] signature result signature.
     * @return Error.
     */
    Error Sign(
        const Array<uint8_t>& digest, const crypto::SignOptions& options, Array<uint8_t>& signature) const override;

    /**
     * Decrypts a cipher message.
     * There is no Decrypt implementation for ECDSA.
     * Some information here: https://stackoverflow.com/questions/76741626/how-to-decrypt-data-with-a-ecdsa-private-key
     *
     * @param cipher encrypted message.
     * @param[out] result decoded message.
     * @return Error.
     */
    Error Decrypt(const Array<uint8_t>& cipher, Array<uint8_t>& result) const override
    {
        (void)cipher;
        (void)result;

        return ErrorEnum::eFailed;
    }

private:
    SharedPtr<SessionContext>  mSession;
    crypto::x509::ProviderItf& mCryptoProvider;
    ObjectHandle               mPrivKeyHandle;
    crypto::ECDSAPublicKey     mPublicKey;
};

/**
 * Maximum size of pkcs11 PrivateKey in bytes.
 */
constexpr auto cPrivateKeyMaxSize = Max(sizeof(PKCS11RSAPrivateKey), sizeof(PKCS11ECDSAPrivateKey));

} // namespace aos::pkcs11

#endif
