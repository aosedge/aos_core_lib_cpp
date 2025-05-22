/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>

#include "aos/test/crypto/providers/opensslfactory.hpp"
#include "aos/test/log.hpp"

namespace aos::crypto {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

RetWithError<RSAPublicKey> ExtractRSAPublicKeyFromPrivateKey(const std::string& pemPrivKey)
{
    auto bio = DeferRelease(BIO_new_mem_buf(pemPrivKey.data(), pemPrivKey.size()), BIO_free);
    if (!bio) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    auto pkey = DeferRelease(PEM_read_bio_PrivateKey(bio.Get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
    if (!pkey) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    BIGNUM *n = nullptr, *e = nullptr;
    if (EVP_PKEY_get_bn_param(pkey.Get(), OSSL_PKEY_PARAM_RSA_N, &n) <= 0) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    if (EVP_PKEY_get_bn_param(pkey.Get(), OSSL_PKEY_PARAM_RSA_E, &e) <= 0) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    std::vector<uint8_t> modulus, exponent;

    modulus.resize(BN_num_bytes(n));
    if (BN_bn2bin(n, modulus.data()) <= 0) {
        return {{{}, {}}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
    }

    exponent.resize(BN_num_bytes(e));
    if (BN_bn2bin(e, exponent.data()) <= 0) {
        return {{{}, {}}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
    }

    return RSAPublicKey(
        Array<uint8_t>(modulus.data(), modulus.size()), Array<uint8_t>(exponent.data(), exponent.size()));
}

RetWithError<ECDSAPublicKey> ExtractECDSAPublicKeyFromPrivateKey(const std::string& pemPrivKey)
{
    auto bio = DeferRelease(BIO_new_mem_buf(pemPrivKey.data(), pemPrivKey.size()), BIO_free);
    if (!bio) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    auto pkey = DeferRelease(PEM_read_bio_PrivateKey(bio.Get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
    if (!pkey) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    // get ecp point
    size_t                                   ecPointSize = 0;
    StaticArray<uint8_t, cECDSAPointDERSize> ecPoint;

    ecPoint.Resize(ecPoint.MaxSize());

    if (EVP_PKEY_get_octet_string_param(
            pkey.Get(), OSSL_PKEY_PARAM_PUB_KEY, ecPoint.Get(), ecPoint.Size(), &ecPointSize)
        != 1) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    ecPoint.Resize(ecPointSize);

    // get curve name
    constexpr auto cOSSLMaxNameSize            = 50; // taken from openssl sources: include/internal/sizes.h
    char           curveName[cOSSLMaxNameSize] = {};

    if (EVP_PKEY_get_utf8_string_param(pkey.Get(), OSSL_PKEY_PARAM_GROUP_NAME, curveName, cOSSLMaxNameSize, nullptr)
        <= 0) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    auto obj = DeferRelease(OBJ_txt2obj(curveName, 0), ASN1_OBJECT_free);
    if (!obj) {
        return {{{}, {}}, OPENSSL_ERROR()};
    }

    auto objData    = OBJ_get0_data(obj.Get());
    auto objDataLen = OBJ_length(obj.Get());

    StaticArray<uint8_t, cECDSAParamsOIDSize> groupOID;

    auto err = groupOID.Insert(groupOID.begin(), objData, objData + objDataLen);
    if (!err.IsNone()) {
        return {{{}, {}}, err};
    }

    return ECDSAPublicKey(groupOID, ecPoint);
}

RetWithError<EVP_PKEY*> GetEvpPublicKey(const RSAPublicKey& pubKey)
{
    auto ctx = DeferRelease(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        return {nullptr, OPENSSL_ERROR()};
    }

    EVP_PKEY* pkey = nullptr;

    if (EVP_PKEY_fromdata_init(ctx.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto bld = DeferRelease(OSSL_PARAM_BLD_new(), OSSL_PARAM_BLD_free);
    if (!bld) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto n = DeferRelease(BN_bin2bn(pubKey.GetN().Get(), pubKey.GetN().Size(), nullptr), BN_free);
    if (!n) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto e = DeferRelease(BN_bin2bn(pubKey.GetE().Get(), pubKey.GetE().Size(), nullptr), BN_free);
    if (!e) {
        return {nullptr, OPENSSL_ERROR()};
    }

    if (OSSL_PARAM_BLD_push_BN(bld.Get(), OSSL_PKEY_PARAM_RSA_N, n.Get()) != 1
        || OSSL_PARAM_BLD_push_BN(bld.Get(), OSSL_PKEY_PARAM_RSA_E, e.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto params = DeferRelease(OSSL_PARAM_BLD_to_param(bld.Get()), openssl::AOS_OPENSSL_free);
    if (!params) {
        return {nullptr, OPENSSL_ERROR()};
    }

    if (EVP_PKEY_fromdata(ctx.Get(), &pkey, EVP_PKEY_PUBLIC_KEY, params.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    return {pkey, ErrorEnum::eNone};
}

// Takes OID with stripped tag & length
RetWithError<const char*> GetCurveName(const Array<uint8_t>& rawOID)
{
    auto [fullOID, err] = openssl::GetFullOID(rawOID);
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    const uint8_t* oidPtr = fullOID.Get();

    auto asn1oid = DeferRelease(d2i_ASN1_OBJECT(NULL, &oidPtr, fullOID.Size()), ASN1_OBJECT_free);
    if (!asn1oid) {
        return {"", OPENSSL_ERROR()};
    }

    int nid = OBJ_obj2nid(asn1oid.Get());
    if (nid == NID_undef) {
        return {"", OPENSSL_ERROR()};
    }

    const char* curveName = OBJ_nid2sn(nid);
    if (!curveName) {
        return {"", OPENSSL_ERROR()};
    }

    return {curveName, ErrorEnum::eNone};
}

RetWithError<EVP_PKEY*> GetEvpPublicKey(const ECDSAPublicKey& pubKey)
{
    auto [curveName, err] = GetCurveName(pubKey.GetECParamsOID());
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    auto octetStr = DeferRelease(ASN1_OCTET_STRING_new(), ASN1_OCTET_STRING_free);
    if (ASN1_OCTET_STRING_set(octetStr.Get(), pubKey.GetECPoint().Get(), pubKey.GetECPoint().Size()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto octetStrBuffer = ASN1_STRING_get0_data(octetStr.Get());
    auto octetStrLen    = ASN1_STRING_length(octetStr.Get());

    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>(curveName), 0),
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, const_cast<uint8_t*>(octetStrBuffer), octetStrLen),
        OSSL_PARAM_construct_end()};

    auto ctx = DeferRelease(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        return {nullptr, OPENSSL_ERROR()};
    }

    EVP_PKEY* pkey = nullptr;

    if (EVP_PKEY_fromdata_init(ctx.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    if (EVP_PKEY_fromdata(ctx.Get(), &pkey, EVP_PKEY_PUBLIC_KEY, params) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    return {pkey, ErrorEnum::eNone};
}

bool VerifyWithEVP(EVP_PKEY* evpKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
{
    auto ctx = DeferRelease(EVP_PKEY_CTX_new(evpKey, nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        return false;
    }

    if (EVP_PKEY_verify_init(ctx.Get()) != 1 || EVP_PKEY_CTX_set_signature_md(ctx.Get(), EVP_sha256()) != 1
        || EVP_PKEY_verify(ctx.Get(), signature.Get(), signature.Size(), digest.Get(), digest.Size()) != 1) {

        return false;
    }

    return true;
}

/***********************************************************************************************************************
 * RSAPrivateKey implementation
 **********************************************************************************************************************/

class RSAPrivateKey : public PrivateKeyItf {
public:
    RSAPrivateKey(const std::string& privKeyPEM)
        : mPublicKey {{}, {}}
    {
        mPrivKey = nullptr;
        Error err;

        Tie(mPublicKey, err) = ExtractRSAPublicKeyFromPrivateKey(privKeyPEM.c_str());
        if (!err.IsNone()) {
            throw std::runtime_error("public key extraction failed");
        }

        auto bio = DeferRelease(BIO_new_mem_buf(privKeyPEM.c_str(), privKeyPEM.size()), BIO_free);
        if (!bio) {
            throw std::runtime_error("mem buffer creating failed");
        }

        mPrivKey = PEM_read_bio_PrivateKey(bio.Get(), nullptr, nullptr, nullptr);
        if (!mPrivKey) {
            throw std::runtime_error("private key read failed");
        }
    }

    ~RSAPrivateKey()
    {
        if (mPrivKey) {
            EVP_PKEY_free(mPrivKey);
        }
    }

    const aos::crypto::PublicKeyItf& GetPublic() const { return mPublicKey; }

    aos::Error Sign(const aos::Array<uint8_t>& digest, const aos::crypto::SignOptions& options,
        aos::Array<uint8_t>& signature) const override
    {
        if (options.mHash != aos::crypto::HashEnum::eSHA256) {
            return aos::ErrorEnum::eInvalidArgument;
        }

        auto ctx = DeferRelease(EVP_PKEY_CTX_new(mPrivKey, nullptr), EVP_PKEY_CTX_free);
        if (!ctx) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }

        signature.Resize(signature.MaxSize());
        size_t sigLen = signature.MaxSize();

        if (EVP_PKEY_sign_init(ctx.Get()) != 1 || EVP_PKEY_CTX_set_signature_md(ctx.Get(), EVP_sha256()) != 1
            || EVP_PKEY_sign(ctx.Get(), signature.Get(), &sigLen, digest.Get(), digest.Size()) != 1) {

            return OPENSSL_ERROR();
        }

        return signature.Resize(sigLen);
    }

    aos::Error Decrypt(const aos::Array<unsigned char>&, aos::Array<unsigned char>&) const
    {
        return aos::ErrorEnum::eNotSupported;
    }

public:
    RSAPublicKey mPublicKey;
    EVP_PKEY*    mPrivKey;
};

/***********************************************************************************************************************
 * ECDSAPrivateKey implementation
 **********************************************************************************************************************/

class ECDSAPrivateKey : public PrivateKeyItf {
public:
    ECDSAPrivateKey(const std::string& privKeyPEM)
        : mPublicKey {{}, {}}
    {
        mPrivKey = nullptr;
        Error err;

        Tie(mPublicKey, err) = ExtractECDSAPublicKeyFromPrivateKey(privKeyPEM.c_str());
        if (!err.IsNone()) {
            throw std::runtime_error("public key extraction failed");
        }

        auto bio = DeferRelease(BIO_new_mem_buf(privKeyPEM.c_str(), privKeyPEM.size()), BIO_free);
        if (!bio) {
            throw std::runtime_error("mem buffer creating failed");
        }

        mPrivKey = PEM_read_bio_PrivateKey(bio.Get(), nullptr, nullptr, nullptr);
        if (!mPrivKey) {
            throw std::runtime_error("private key read failed");
        }
    }

    ~ECDSAPrivateKey()
    {
        if (mPrivKey) {
            EVP_PKEY_free(mPrivKey);
        }
    }

    const aos::crypto::PublicKeyItf& GetPublic() const { return mPublicKey; }

    aos::Error Sign(const aos::Array<uint8_t>& digest, const aos::crypto::SignOptions& options,
        aos::Array<uint8_t>& signature) const override
    {
        if (options.mHash != aos::crypto::HashEnum::eSHA384) {
            return aos::ErrorEnum::eInvalidArgument;
        }

        auto ctx = DeferRelease(EVP_PKEY_CTX_new(mPrivKey, nullptr), EVP_PKEY_CTX_free);
        if (!ctx) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }

        signature.Resize(signature.MaxSize());
        size_t sigLen = signature.MaxSize();

        if (EVP_PKEY_sign_init(ctx.Get()) != 1 || EVP_PKEY_CTX_set_signature_md(ctx.Get(), EVP_sha384()) != 1
            || EVP_PKEY_sign(ctx.Get(), signature.Get(), &sigLen, digest.Get(), digest.Size()) != 1) {

            return OPENSSL_ERROR();
        }

        return signature.Resize(sigLen);
    }

    aos::Error Decrypt(const aos::Array<unsigned char>&, aos::Array<unsigned char>&) const
    {
        return aos::ErrorEnum::eNotSupported;
    }

public:
    ECDSAPublicKey mPublicKey;
    EVP_PKEY*      mPrivKey;
};

} // namespace

/***********************************************************************************************************************
 * OpenSSLCryptoFactory implementation
 **********************************************************************************************************************/

OpenSSLCryptoFactory::OpenSSLCryptoFactory()
{
}

Error OpenSSLCryptoFactory::Init()
{
    return mProvider.Init();
}

std::string OpenSSLCryptoFactory::GetName()
{
    return "OpenSSL";
}

CryptoProviderItf& OpenSSLCryptoFactory::GetCryptoProvider()
{
    return mProvider;
}

HasherItf& OpenSSLCryptoFactory::GetHashProvider()
{
    return mProvider;
}

RandomItf& OpenSSLCryptoFactory::GetRandomProvider()
{
    return mProvider;
}

RetWithError<std::shared_ptr<PrivateKeyItf>> OpenSSLCryptoFactory::GenerateRSAPrivKey()
{
    auto ctx = DeferRelease(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        return {{}, OPENSSL_ERROR()};
    }

    // Generate key
    EVP_PKEY* pkey = nullptr;

    if (EVP_PKEY_keygen_init(ctx.Get()) <= 0) {
        return {{}, OPENSSL_ERROR()};
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.Get(), 2048) <= 0) {
        return {{}, OPENSSL_ERROR()};
    }

    if (EVP_PKEY_generate(ctx.Get(), &pkey) <= 0) {
        return {{}, OPENSSL_ERROR()};
    }

    auto releaseKey = DeferRelease(pkey, EVP_PKEY_free);

    // Write private key into buffer
    auto bio = DeferRelease(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio) {
        return {{}, OPENSSL_ERROR()};
    }

    if (!PEM_write_bio_PrivateKey_ex(bio.Get(), pkey, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr)) {
        return {{}, OPENSSL_ERROR()};
    }

    char* bioData = nullptr;

    long bioLen = BIO_get_mem_data(bio.Get(), &bioData);
    if (bioLen <= 0) {
        return {{}, OPENSSL_ERROR()};
    }

    auto key = std::make_shared<RSAPrivateKey>(std::string(bioData, bioData + bioLen));

    return {key, ErrorEnum::eNone};
}

RetWithError<std::shared_ptr<PrivateKeyItf>> OpenSSLCryptoFactory::GenerateECDSAPrivKey()
{
    // Create a context for key generation
    auto ctx = DeferRelease(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        return {{}, OPENSSL_ERROR()};
    }

    if (EVP_PKEY_keygen_init(ctx.Get()) <= 0) {
        return {{}, OPENSSL_ERROR()};
    }

    // Set the curve name to secp384r1
    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.Get(), NID_secp384r1) <= 0) {
        return {{}, OPENSSL_ERROR()};
    }

    // Generate the key
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_generate(ctx.Get(), &pkey) <= 0) {
        return {{}, OPENSSL_ERROR()};
    }

    auto freePKey = DeferRelease(pkey, EVP_PKEY_free);

    // Write private key into buffer
    auto bio = DeferRelease(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio) {
        return {{}, OPENSSL_ERROR()};
    }

    if (!PEM_write_bio_PrivateKey_ex(bio.Get(), pkey, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr)) {
        return {{}, OPENSSL_ERROR()};
    }

    char* bioData = nullptr;

    long bioLen = BIO_get_mem_data(bio.Get(), &bioData);
    if (bioLen <= 0) {
        return {{}, OPENSSL_ERROR()};
    }

    auto key = std::make_shared<ECDSAPrivateKey>(std::string(bioData, bioData + bioLen));

    return {key, ErrorEnum::eNone};
}

RetWithError<std::vector<uint8_t>> OpenSSLCryptoFactory::PemCertToDer(const char* pem)
{
    auto bio = DeferRelease(BIO_new_mem_buf(pem, strlen(pem)), BIO_free);
    if (!bio) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
    }

    auto cert = DeferRelease(PEM_read_bio_X509(bio.Get(), NULL, NULL, NULL), X509_free);
    if (!cert) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
    }

    auto derBIO = DeferRelease(BIO_new(BIO_s_mem()), BIO_free);
    if (!derBIO) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
    }

    if (i2d_X509_bio(derBIO.Get(), cert.Get()) <= 0) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
    }

    uint8_t* derData = nullptr;

    long derDataLen = BIO_get_mem_data(derBIO.Get(), reinterpret_cast<char*>(&derData));
    if (derDataLen <= 0) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
    }

    return {std::vector<uint8_t> {derData, derData + derDataLen}, ErrorEnum::eNone};
}

bool OpenSSLCryptoFactory::VerifyCertificate(const std::string& pemCert)
{
    auto bioCert = DeferRelease(BIO_new_mem_buf(pemCert.data(), pemCert.size()), BIO_free);
    if (!bioCert) {
        return false;
    }

    auto cert = DeferRelease(PEM_read_bio_X509(bioCert.Get(), nullptr, nullptr, nullptr), X509_free);
    if (!cert) {
        return false;
    }

    return X509_verify(cert.Get(), X509_get_pubkey(cert.Get())) == 1;
}

bool OpenSSLCryptoFactory::VerifyCSR(const std::string& pemCSR)
{
    auto bio = DeferRelease(BIO_new_mem_buf(pemCSR.data(), pemCSR.size()), BIO_free);
    if (!bio) {
        return false;
    }

    auto csr = DeferRelease(PEM_read_bio_X509_REQ(bio.Get(), nullptr, nullptr, nullptr), X509_REQ_free);
    if (!csr) {
        return false;
    }

    return true;
}

bool OpenSSLCryptoFactory::VerifySignature(
    const RSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
{
    auto [evpKey, err] = GetEvpPublicKey(pubKey);
    if (!err.IsNone()) {
        return false;
    }

    auto freeKey = DeferRelease(evpKey, EVP_PKEY_free);

    return VerifyWithEVP(evpKey, signature, digest);
}

bool OpenSSLCryptoFactory::VerifySignature(
    const ECDSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
{
    auto [evpKey, err] = GetEvpPublicKey(pubKey);
    if (!err.IsNone()) {
        return false;
    }

    auto freeKey = DeferRelease(evpKey, EVP_PKEY_free);

    return VerifyWithEVP(evpKey, signature, digest);
}

Error OpenSSLCryptoFactory::Encrypt(const RSAPublicKey& pubKey, const Array<uint8_t>& msg, Array<uint8_t>& cipher)
{
    auto [evpKey, err] = GetEvpPublicKey(pubKey);
    if (!err.IsNone()) {
        return err;
    }
    auto freeKey = DeferRelease(evpKey, EVP_PKEY_free);

    auto ctx = DeferRelease(EVP_PKEY_CTX_new(evpKey, nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (EVP_PKEY_encrypt_init(ctx.Get()) <= 0) {
        return OPENSSL_ERROR();
    }

    size_t outLen = 0;
    if (EVP_PKEY_encrypt(ctx.Get(), nullptr, &outLen, msg.Get(), msg.Size()) <= 0) {
        return OPENSSL_ERROR();
    }

    if (auto err = cipher.Resize(outLen); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (EVP_PKEY_encrypt(ctx.Get(), cipher.Get(), &outLen, msg.Get(), msg.Size()) <= 0) {
        return OPENSSL_ERROR();
    }

    cipher.Resize(outLen);

    return ErrorEnum::eNone;
}

} // namespace aos::crypto
