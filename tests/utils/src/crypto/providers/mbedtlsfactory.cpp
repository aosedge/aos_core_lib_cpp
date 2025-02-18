/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

#include "aos/test/crypto/providers/mbedtlsfactory.hpp"

namespace aos::crypto {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

int ConvertMbedtlsMpiToArray(const mbedtls_mpi* mpi, aos::Array<uint8_t>& outArray)
{
    outArray.Resize(mbedtls_mpi_size(mpi));

    return mbedtls_mpi_write_binary(mpi, outArray.Get(), outArray.Size());
}

RetWithError<RSAPublicKey> ExtractRSAPublicKeyFromPrivateKey(const std::string& pemPrivKey)
{
    mbedtls_pk_context       pkContext;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_pk_init(&pkContext);

    std::unique_ptr<mbedtls_pk_context, decltype(&mbedtls_pk_free)> pkContextPtr(&pkContext, mbedtls_pk_free);
    std::unique_ptr<mbedtls_ctr_drbg_context, decltype(&mbedtls_ctr_drbg_free)> ctrDrbgPtr(
        &ctr_drbg, mbedtls_ctr_drbg_free);

    auto ret = mbedtls_pk_parse_key(pkContextPtr.get(), (const unsigned char*)pemPrivKey.c_str(), pemPrivKey.size() + 1,
        nullptr, 0, mbedtls_ctr_drbg_random, ctrDrbgPtr.get());
    if (ret != 0) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    if (mbedtls_pk_get_type(pkContextPtr.get()) != MBEDTLS_PK_RSA) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    mbedtls_rsa_context* rsa_context = mbedtls_pk_rsa(*pkContextPtr.get());

    mbedtls_mpi mpiN, mpiE;
    mbedtls_mpi_init(&mpiN);
    mbedtls_mpi_init(&mpiE);

    std::unique_ptr<mbedtls_mpi, decltype(&mbedtls_mpi_free)> mpiNPtr(&mpiN, mbedtls_mpi_free);
    std::unique_ptr<mbedtls_mpi, decltype(&mbedtls_mpi_free)> mpiEPtr(&mpiE, mbedtls_mpi_free);

    if ((ret = mbedtls_rsa_export(rsa_context, mpiNPtr.get(), nullptr, nullptr, nullptr, mpiEPtr.get())) != 0) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    StaticArray<uint8_t, cRSAModulusSize>     n;
    StaticArray<uint8_t, cRSAPubExponentSize> e;

    if ((ret = ConvertMbedtlsMpiToArray(mpiNPtr.get(), n)) != 0) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    if ((ret = ConvertMbedtlsMpiToArray(mpiEPtr.get(), e)) != 0) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    return {RSAPublicKey {n, e}, ErrorEnum::eNone};
}

RetWithError<ECDSAPublicKey> ExtractECDSAPublicKeyFromPrivateKey(const std::string& pemPrivKey)
{
    int                ret {};
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    std::unique_ptr<mbedtls_pk_context, decltype(&mbedtls_pk_free)> pkPtr(&pk, mbedtls_pk_free);

    if ((ret = mbedtls_pk_parse_key(
             &pk, (const unsigned char*)pemPrivKey.data(), pemPrivKey.size() + 1, nullptr, 0, nullptr, nullptr))
        != 0) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    if (mbedtls_pk_get_type(&pk) != MBEDTLS_PK_ECKEY) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    mbedtls_ecp_keypair* ecp = mbedtls_pk_ec(pk);
    if (ecp == nullptr) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    const char* oid;
    size_t      oid_len;
    if ((ret = mbedtls_oid_get_oid_by_ec_grp(ecp->MBEDTLS_PRIVATE(grp).id, &oid, &oid_len)) != 0) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    StaticArray<uint8_t, cECDSAParamsOIDSize> paramsOID;

    paramsOID.Resize(oid_len);
    std::copy(reinterpret_cast<const uint8_t*>(oid), reinterpret_cast<const uint8_t*>(oid) + oid_len, paramsOID.Get());

    uint8_t point_buf[cECDSAPointDERSize];
    size_t  point_len;
    ret = mbedtls_ecp_point_write_binary(&ecp->MBEDTLS_PRIVATE(grp), &ecp->MBEDTLS_PRIVATE(Q),
        MBEDTLS_ECP_PF_UNCOMPRESSED, &point_len, point_buf, sizeof(point_buf));
    if (ret != 0) {
        return {{{}, {}}, aos::ErrorEnum::eInvalidArgument};
    }

    StaticArray<uint8_t, cECDSAPointDERSize> ecPoint;

    ecPoint.Resize(point_len);
    std::copy(point_buf, point_buf + point_len, ecPoint.Get());

    return {ECDSAPublicKey {paramsOID, ecPoint}, ErrorEnum::eNone};
}

Error ImportRSAPublicKey(const RSAPublicKey& rsaKey, mbedtls_pk_context& ctx)
{
    const auto& n = rsaKey.GetN();
    const auto& e = rsaKey.GetE();

    if (mbedtls_pk_setup(&ctx, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    mbedtls_rsa_context* rsaCtx = mbedtls_pk_rsa(ctx);

    if (mbedtls_rsa_import_raw(rsaCtx, n.Get(), n.Size(), nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0) != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (mbedtls_rsa_import_raw(rsaCtx, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, e.Get(), e.Size()) != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (mbedtls_rsa_complete(rsaCtx) != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (mbedtls_rsa_check_pubkey(rsaCtx) != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    return ErrorEnum::eNone;
}

Error ImportECDSAPublicKey(const ECDSAPublicKey& ecdsaKey, mbedtls_pk_context& ctx)
{
    // We can ignore ECPARAMS as we support SECP384R1 curve only
    const auto& ecParams = ecdsaKey.GetECParamsOID();
    (void)ecParams;

    const auto& ecPoint = ecdsaKey.GetECPoint();

    if (mbedtls_pk_setup(&ctx, mbedtls_pk_info_from_type(MBEDTLS_PK_ECDSA)) != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    mbedtls_ecdsa_context* ecdsaCtx = mbedtls_pk_ec(ctx);

    mbedtls_ecdsa_init(ecdsaCtx);
    if (mbedtls_ecp_group_load(&ecdsaCtx->private_grp, MBEDTLS_ECP_DP_SECP384R1) != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    // read EC point
    if (mbedtls_ecp_point_read_binary(&ecdsaCtx->private_grp, &ecdsaCtx->private_Q, ecPoint.Get(), ecPoint.Size())
        != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * RSAPrivateKey implementation
 **********************************************************************************************************************/

class RSAPrivateKey : public PrivateKeyItf {
public:
    RSAPrivateKey(const std::string& privKeyPEM)
        : mPublicKey {{}, {}}
    {
        Error err;

        Tie(mPublicKey, err) = ExtractRSAPublicKeyFromPrivateKey(privKeyPEM.c_str());
        if (!err.IsNone()) {
            throw std::runtime_error("public key extraction failed");
        }

        mPrivateKey = privKeyPEM;
    }

    const PublicKeyItf& GetPublic() const { return mPublicKey; }

    aos::Error Sign(
        const aos::Array<uint8_t>& digest, const SignOptions& options, aos::Array<uint8_t>& signature) const override
    {
        if (options.mHash != HashEnum::eSHA256) {
            return aos::ErrorEnum::eInvalidArgument;
        }

        mbedtls_pk_context       pk;
        mbedtls_entropy_context  entropy;
        mbedtls_ctr_drbg_context ctrDrbg;
        const char*              pers = "rsa_sign";

        // Initialize
        mbedtls_pk_init(&pk);
        mbedtls_ctr_drbg_init(&ctrDrbg);
        mbedtls_entropy_init(&entropy);

        std::unique_ptr<mbedtls_pk_context, decltype(&mbedtls_pk_free)>             pkPtr(&pk, mbedtls_pk_free);
        std::unique_ptr<mbedtls_ctr_drbg_context, decltype(&mbedtls_ctr_drbg_free)> ctrDrbgPtr(
            &ctrDrbg, mbedtls_ctr_drbg_free);
        std::unique_ptr<mbedtls_entropy_context, decltype(&mbedtls_entropy_free)> entropyPtr(
            &entropy, mbedtls_entropy_free);

        int ret = mbedtls_pk_parse_key(pkPtr.get(), (const uint8_t*)mPrivateKey.data(), mPrivateKey.size() + 1, nullptr,
            0, mbedtls_ctr_drbg_random, ctrDrbgPtr.get());
        if (ret != 0) {
            return ret;
        }

        ret = mbedtls_ctr_drbg_seed(
            ctrDrbgPtr.get(), mbedtls_entropy_func, entropyPtr.get(), (const unsigned char*)pers, strlen(pers));
        if (ret != 0) {
            return ret;
        }

        size_t signatureLen;

        ret = mbedtls_pk_sign(pkPtr.get(), MBEDTLS_MD_SHA256, digest.Get(), digest.Size(), signature.Get(),
            signature.Size(), &signatureLen, mbedtls_ctr_drbg_random, ctrDrbgPtr.get());
        if (ret != 0) {
            return ret;
        }

        signature.Resize(signatureLen);

        return aos::ErrorEnum::eNone;
    }

    aos::Error Decrypt(const aos::Array<unsigned char>&, aos::Array<unsigned char>&) const
    {
        return aos::ErrorEnum::eNotSupported;
    }

public:
    RSAPublicKey mPublicKey;
    std::string  mPrivateKey;
};

/***********************************************************************************************************************
 * ECDSAPrivateKey implementation
 **********************************************************************************************************************/

class ECDSAPrivateKey : public PrivateKeyItf {
public:
    ECDSAPrivateKey(const std::string& privKeyPEM)
        : mPublicKey {{}, {}}
    {
        Error err;

        Tie(mPublicKey, err) = ExtractECDSAPublicKeyFromPrivateKey(privKeyPEM.c_str());
        if (!err.IsNone()) {
            throw std::runtime_error("public key extraction failed");
        }

        mPrivateKey = privKeyPEM;
    }

    const PublicKeyItf& GetPublic() const { return mPublicKey; }

    aos::Error Sign(
        const aos::Array<uint8_t>& digest, const SignOptions& options, aos::Array<uint8_t>& signature) const override
    {
        if (options.mHash != HashEnum::eSHA384) {
            return aos::ErrorEnum::eInvalidArgument;
        }

        mbedtls_pk_context       pk;
        mbedtls_entropy_context  entropy;
        mbedtls_ctr_drbg_context ctrDrbg;
        const char*              pers = "ecdsa_sign";

        mbedtls_pk_init(&pk);
        mbedtls_ctr_drbg_init(&ctrDrbg);
        mbedtls_entropy_init(&entropy);

        std::unique_ptr<mbedtls_pk_context, decltype(&mbedtls_pk_free)>             pkPtr(&pk, mbedtls_pk_free);
        std::unique_ptr<mbedtls_ctr_drbg_context, decltype(&mbedtls_ctr_drbg_free)> ctrDrbgPtr(
            &ctrDrbg, mbedtls_ctr_drbg_free);
        std::unique_ptr<mbedtls_entropy_context, decltype(&mbedtls_entropy_free)> entropyPtr(
            &entropy, mbedtls_entropy_free);

        int ret = mbedtls_pk_parse_key(pkPtr.get(), (const uint8_t*)mPrivateKey.data(), mPrivateKey.size() + 1, nullptr,
            0, mbedtls_ctr_drbg_random, ctrDrbgPtr.get());
        if (ret != 0) {
            return ret;
        }

        ret = mbedtls_ctr_drbg_seed(
            ctrDrbgPtr.get(), mbedtls_entropy_func, entropyPtr.get(), (const unsigned char*)pers, strlen(pers));
        if (ret != 0) {
            return ret;
        }

        size_t signatureLen;

        ret = mbedtls_pk_sign(pkPtr.get(), MBEDTLS_MD_SHA384, digest.Get(), digest.Size(), signature.Get(),
            signature.Size(), &signatureLen, mbedtls_ctr_drbg_random, ctrDrbgPtr.get());
        if (ret != 0) {
            return ret;
        }

        // mbedtls_pk_sign adds ASN1 tags to the signature that makes a result incorrect as a raw signature expected.
        // As a workaround for the tests just correct size of the result.
        signature.Resize(digest.Size() * 2);

        return aos::ErrorEnum::eNone;
    }

    aos::Error Decrypt(const aos::Array<unsigned char>&, aos::Array<unsigned char>&) const
    {
        return aos::ErrorEnum::eNotSupported;
    }

public:
    ECDSAPublicKey mPublicKey;
    std::string    mPrivateKey;
};

} // namespace

/***********************************************************************************************************************
 * MBedTLSCryptoFactory implementation
 **********************************************************************************************************************/

MBedTLSCryptoFactory::MBedTLSCryptoFactory()
{
}

Error MBedTLSCryptoFactory::Init()
{
    return mProvider.Init();
}

std::string MBedTLSCryptoFactory::GetName()
{
    return "MBedTLS";
}

x509::ProviderItf& MBedTLSCryptoFactory::GetCryptoProvider()
{
    return mProvider;
}

HasherItf& MBedTLSCryptoFactory::GetHashProvider()
{
    return mProvider;
}

RandomItf& MBedTLSCryptoFactory::GetRandomProvider()
{
    return mProvider;
}

RetWithError<std::shared_ptr<PrivateKeyItf>> MBedTLSCryptoFactory::GenerateRSAPrivKey()
{
    mbedtls_pk_context       pk;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char*              pers    = "rsa_genkey";
    constexpr size_t         keySize = 2048;
    constexpr size_t         expSize = 65537;

    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    std::unique_ptr<mbedtls_pk_context, decltype(&mbedtls_pk_free)>           pkPtr(&pk, mbedtls_pk_free);
    std::unique_ptr<mbedtls_entropy_context, decltype(&mbedtls_entropy_free)> entropyPtr(
        &entropy, mbedtls_entropy_free);
    std::unique_ptr<mbedtls_ctr_drbg_context, decltype(&mbedtls_ctr_drbg_free)> ctrDrbgPtr(
        &ctr_drbg, mbedtls_ctr_drbg_free);

    if (mbedtls_ctr_drbg_seed(
            ctrDrbgPtr.get(), mbedtls_entropy_func, entropyPtr.get(), (const unsigned char*)pers, strlen(pers))
        != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    if (mbedtls_pk_setup(pkPtr.get(), mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    if (mbedtls_rsa_gen_key(mbedtls_pk_rsa(*pkPtr.get()), mbedtls_ctr_drbg_random, ctrDrbgPtr.get(), keySize, expSize)
        != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    std::string pemKey(16000, '0');
    if (mbedtls_pk_write_key_pem(pkPtr.get(), (uint8_t*)pemKey.c_str(), pemKey.size()) != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    size_t keyLen = strlen(reinterpret_cast<char*>(pemKey.data()));
    pemKey.resize(keyLen);

    auto key = std::make_shared<RSAPrivateKey>(pemKey);

    return {key, ErrorEnum::eNone};
}

RetWithError<std::shared_ptr<PrivateKeyItf>> MBedTLSCryptoFactory::GenerateECDSAPrivKey()
{
    mbedtls_pk_context       pk;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    const char*              pers = "ecdsa_genkey";

    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctrDrbg);

    std::unique_ptr<mbedtls_pk_context, decltype(&mbedtls_pk_free)>           pkPtr(&pk, mbedtls_pk_free);
    std::unique_ptr<mbedtls_entropy_context, decltype(&mbedtls_entropy_free)> entropyPtr(
        &entropy, mbedtls_entropy_free);
    std::unique_ptr<mbedtls_ctr_drbg_context, decltype(&mbedtls_ctr_drbg_free)> ctrDrbgPtr(
        &ctrDrbg, mbedtls_ctr_drbg_free);

    if (mbedtls_ctr_drbg_seed(
            ctrDrbgPtr.get(), mbedtls_entropy_func, entropyPtr.get(), (const unsigned char*)pers, strlen(pers))
        != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    if (mbedtls_pk_setup(pkPtr.get(), mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    if (mbedtls_ecp_gen_key(
            MBEDTLS_ECP_DP_SECP384R1, mbedtls_pk_ec(*pkPtr.get()), mbedtls_ctr_drbg_random, ctrDrbgPtr.get())
        != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    std::string pemKey(2048, '0');
    if (mbedtls_pk_write_key_pem(pkPtr.get(), (uint8_t*)pemKey.c_str(), pemKey.size()) != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    size_t keyLen = strlen(reinterpret_cast<char*>(pemKey.data()));
    pemKey.resize(keyLen);

    auto key = std::make_shared<ECDSAPrivateKey>(pemKey);

    return {key, ErrorEnum::eNone};
}

RetWithError<std::vector<uint8_t>> MBedTLSCryptoFactory::PemCertToDer(const char* pem)
{
    mbedtls_x509_crt cert;

    mbedtls_x509_crt_init(&cert);

    std::unique_ptr<mbedtls_x509_crt, decltype(&mbedtls_x509_crt_free)> derDataPtr(&cert, mbedtls_x509_crt_free);

    auto ret = mbedtls_x509_crt_parse(derDataPtr.get(), reinterpret_cast<const uint8_t*>(pem), strlen(pem) + 1);
    if (ret != 0) {
        return {{}, aos::ErrorEnum::eInvalidArgument};
    }

    std::vector<uint8_t> derData;

    derData.resize(cert.raw.len);
    std::copy(cert.raw.p, cert.raw.p + cert.raw.len, derData.begin());

    return derData;
}

bool MBedTLSCryptoFactory::VerifyCertificate(const std::string& pemCert)
{
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);

    std::unique_ptr<mbedtls_x509_crt, decltype(&mbedtls_x509_crt_free)> certPtr(&cert, mbedtls_x509_crt_free);

    int ret = mbedtls_x509_crt_parse(&cert, reinterpret_cast<const uint8_t*>(pemCert.c_str()), pemCert.size() + 1);
    if (ret != 0) {
        return false;
    }

    uint32_t flags;

    return mbedtls_x509_crt_verify(&cert, &cert, nullptr, nullptr, &flags, nullptr, nullptr) == 0;
}

bool MBedTLSCryptoFactory::VerifyCSR(const std::string& pemCSR)
{
    mbedtls_x509_csr csr;
    mbedtls_x509_csr_init(&csr);

    std::unique_ptr<mbedtls_x509_csr, decltype(&mbedtls_x509_csr_free)> csrPtr(&csr, mbedtls_x509_csr_free);

    return mbedtls_x509_csr_parse(csrPtr.get(), reinterpret_cast<const uint8_t*>(pemCSR.c_str()), pemCSR.size() + 1)
        == 0;
}

bool MBedTLSCryptoFactory::VerifySignature(
    const RSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
{
    mbedtls_pk_context pubKeyCtx;

    mbedtls_pk_init(&pubKeyCtx);

    if (!ImportRSAPublicKey(pubKey, pubKeyCtx).IsNone()) {
        return false;
    }

    int ret = mbedtls_pk_verify(
        &pubKeyCtx, MBEDTLS_MD_SHA256, digest.Get(), digest.Size(), signature.Get(), signature.Size());

    mbedtls_pk_free(&pubKeyCtx);

    return ret == 0;
}

bool MBedTLSCryptoFactory::VerifySignature(
    const ECDSAPublicKey& pubKey, const Array<uint8_t>& signature, const StaticArray<uint8_t, 32>& digest)
{
    mbedtls_pk_context pubKeyCtx;

    mbedtls_pk_init(&pubKeyCtx);

    if (!ImportECDSAPublicKey(pubKey, pubKeyCtx).IsNone()) {
        return false;
    }

    mbedtls_ecdsa_context* ecdsaCtx = mbedtls_pk_ec(pubKeyCtx);

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    size_t rsLen = signature.Size() / 2;

    if (mbedtls_mpi_read_binary(&r, signature.Get(), rsLen) != 0) {
        return false;
    }

    if (mbedtls_mpi_read_binary(&s, signature.Get() + rsLen, rsLen) != 0) {
        return false;
    }

    if (mbedtls_ecdsa_verify(&ecdsaCtx->private_grp, digest.Get(), digest.Size(), &ecdsaCtx->private_Q, &r, &s) != 0) {
        return false;
    }

    mbedtls_pk_free(&pubKeyCtx);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return true;
}

Error MBedTLSCryptoFactory::Encrypt(const RSAPublicKey& pubKey, const Array<uint8_t>& msg, Array<uint8_t>& cipher)
{
    mbedtls_pk_context pubKeyCtx;
    mbedtls_pk_init(&pubKeyCtx);

    if (auto err = ImportRSAPublicKey(pubKey, pubKeyCtx); !err.IsNone()) {
        return err;
    }

    // setup entropy
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    const char*              pers = "mbedtls_pk_encrypt";

    mbedtls_ctr_drbg_init(&ctrDrbg);
    mbedtls_entropy_init(&entropy);

    int ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        mbedtls_pk_free(&pubKeyCtx);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctrDrbg);

        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    // encrypt
    size_t len = 0;

    cipher.Resize(cipher.MaxSize());

    ret = mbedtls_pk_encrypt(
        &pubKeyCtx, msg.Get(), msg.Size(), cipher.Get(), &len, cipher.Size(), mbedtls_ctr_drbg_random, &ctrDrbg);

    cipher.Resize(len);

    mbedtls_pk_free(&pubKeyCtx);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctrDrbg);

    if (ret != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::crypto
