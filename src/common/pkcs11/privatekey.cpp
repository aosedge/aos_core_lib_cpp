/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/pkcs11/privatekey.hpp"

#include "log.hpp"

namespace aos::pkcs11 {

/***********************************************************************************************************************
 * PKCS11RSAPrivateKey
 **********************************************************************************************************************/

constexpr uint8_t PKCS11RSAPrivateKey::cSHA1Prefix[];
constexpr uint8_t PKCS11RSAPrivateKey::cSHA224Prefix[];
constexpr uint8_t PKCS11RSAPrivateKey::cSHA256Prefix[];
constexpr uint8_t PKCS11RSAPrivateKey::cSHA384Prefix[];
constexpr uint8_t PKCS11RSAPrivateKey::cSHA512Prefix[];

PKCS11RSAPrivateKey::PKCS11RSAPrivateKey(
    const SharedPtr<SessionContext>& session, ObjectHandle privKeyHandle, const crypto::RSAPublicKey& pubKey)
    : mSession(session)
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

    t->Append(GetPrefix(options.mHash));
    t->Append(digest);

    CK_MECHANISM mechanism = {CKM_RSA_PKCS, nullptr, 0};

    return mSession->Sign(&mechanism, mPrivKeyHandle, *t, signature);
}

Error PKCS11RSAPrivateKey::Decrypt(const Array<uint8_t>& cipher, Array<uint8_t>& result) const
{
    CK_MECHANISM mechanism = {CKM_RSA_PKCS, nullptr, 0};

    return mSession->Decrypt(&mechanism, mPrivKeyHandle, cipher, result);
}

Array<uint8_t> PKCS11RSAPrivateKey::GetPrefix(crypto::Hash hash) const
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
 * PKCS11ECDSAPrivateKey
 **********************************************************************************************************************/

PKCS11ECDSAPrivateKey::PKCS11ECDSAPrivateKey(const SharedPtr<SessionContext>& session,
    crypto::x509::ProviderItf& cryptoProvider, ObjectHandle privKeyHandle, const crypto::ECDSAPublicKey& pubKey)
    : mSession(session)
    , mCryptoProvider(cryptoProvider)
    , mPrivKeyHandle(privKeyHandle)
    , mPublicKey(pubKey)
{
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

} // namespace aos::pkcs11
