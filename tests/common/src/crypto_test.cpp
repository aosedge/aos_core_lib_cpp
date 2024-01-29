/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static std::pair<aos::Error, std::vector<unsigned char>> GenerateRSAPrivateKey()
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
        return {aos::ErrorEnum::eInvalidArgument, {}};
    }

    if (mbedtls_pk_setup(pkPtr.get(), mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) != 0) {
        return {aos::ErrorEnum::eInvalidArgument, {}};
    }

    if (mbedtls_rsa_gen_key(mbedtls_pk_rsa(*pkPtr.get()), mbedtls_ctr_drbg_random, ctrDrbgPtr.get(), keySize, expSize)
        != 0) {
        return {aos::ErrorEnum::eInvalidArgument, {}};
    }

    std::vector<unsigned char> keyBuf(16000);
    if (mbedtls_pk_write_key_pem(pkPtr.get(), keyBuf.data(), keyBuf.size()) != 0) {
        return {aos::ErrorEnum::eInvalidArgument, {}};
    }

    size_t keyLen = strlen(reinterpret_cast<char*>(keyBuf.data()));
    keyBuf.resize(keyLen + 1);

    return {aos::ErrorEnum::eNone, keyBuf};
}

static std::pair<aos::Error, std::vector<unsigned char>> GenerateECPrivateKey()
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
        return {aos::ErrorEnum::eInvalidArgument, {}};
    }

    if (mbedtls_pk_setup(pkPtr.get(), mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) {
        return {aos::ErrorEnum::eInvalidArgument, {}};
    }

    if (mbedtls_ecp_gen_key(
            MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(*pkPtr.get()), mbedtls_ctr_drbg_random, ctrDrbgPtr.get())
        != 0) {
        return {aos::ErrorEnum::eInvalidArgument, {}};
    }

    std::vector<unsigned char> keyBuf(2048);
    if (mbedtls_pk_write_key_pem(pkPtr.get(), keyBuf.data(), keyBuf.size()) != 0) {
        return {aos::ErrorEnum::eInvalidArgument, {}};
    }

    size_t keyLen = strlen(reinterpret_cast<char*>(keyBuf.data()));
    keyBuf.resize(keyLen + 1);

    return {aos::ErrorEnum::eNone, keyBuf};
}

static int PemToDer(const unsigned char* pemData, size_t pemSize, std::vector<unsigned char>& derData)
{
    mbedtls_x509_crt cert;

    mbedtls_x509_crt_init(&cert);

    std::unique_ptr<mbedtls_x509_crt, decltype(&mbedtls_x509_crt_free)> derDataPtr(&cert, mbedtls_x509_crt_free);

    auto ret = mbedtls_x509_crt_parse(derDataPtr.get(), pemData, pemSize);
    if (ret != 0) {
        return ret;
    }

    derData.resize(cert.raw.len);

    std::copy(cert.raw.p, cert.raw.p + cert.raw.len, derData.begin());

    return 0;
}

static int ConvertMbedtlsMpiToArray(const mbedtls_mpi* mpi, aos::Array<uint8_t>& outArray)
{
    outArray.Resize(mbedtls_mpi_size(mpi));

    return mbedtls_mpi_write_binary(mpi, outArray.Get(), outArray.Size());
}

static aos::Error ExtractRSAPublicKeyFromPrivateKey(const char* pemKey, aos::Array<uint8_t>& N, aos::Array<uint8_t>& E)
{
    mbedtls_pk_context       pkContext;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_pk_init(&pkContext);

    std::unique_ptr<mbedtls_pk_context, decltype(&mbedtls_pk_free)> pkContextPtr(&pkContext, mbedtls_pk_free);
    std::unique_ptr<mbedtls_ctr_drbg_context, decltype(&mbedtls_ctr_drbg_free)> ctrDrbgPtr(
        &ctr_drbg, mbedtls_ctr_drbg_free);

    auto ret = mbedtls_pk_parse_key(pkContextPtr.get(), (const unsigned char*)pemKey, strlen(pemKey) + 1, nullptr, 0,
        mbedtls_ctr_drbg_random, ctrDrbgPtr.get());
    if (ret != 0) {
        return ret;
    }

    if (mbedtls_pk_get_type(pkContextPtr.get()) != MBEDTLS_PK_RSA) {
        return aos::ErrorEnum::eInvalidArgument;
    }

    mbedtls_rsa_context* rsa_context = mbedtls_pk_rsa(*pkContextPtr.get());

    mbedtls_mpi mpiN, mpiE;
    mbedtls_mpi_init(&mpiN);
    mbedtls_mpi_init(&mpiE);

    std::unique_ptr<mbedtls_mpi, decltype(&mbedtls_mpi_free)> mpiNPtr(&mpiN, mbedtls_mpi_free);
    std::unique_ptr<mbedtls_mpi, decltype(&mbedtls_mpi_free)> mpiEPtr(&mpiE, mbedtls_mpi_free);

    if ((ret = mbedtls_rsa_export(rsa_context, mpiNPtr.get(), nullptr, nullptr, nullptr, mpiEPtr.get())) != 0) {
        return ret;
    }

    if ((ret = ConvertMbedtlsMpiToArray(mpiNPtr.get(), N)) != 0) {
        return ret;
    }

    if ((ret = ConvertMbedtlsMpiToArray(mpiEPtr.get(), E)) != 0) {
        return ret;
    }

    return aos::ErrorEnum::eNone;
}

static aos::Error ExtractECPublicKeyFromPrivate(
    aos::Array<uint8_t>& paramsOID, aos::Array<uint8_t>& ecPoint, const std::vector<unsigned char>& pemECPrivateKey)
{
    int                ret {};
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    std::unique_ptr<mbedtls_pk_context, decltype(&mbedtls_pk_free)> pkPtr(&pk, mbedtls_pk_free);

    if ((ret = mbedtls_pk_parse_key(&pk, pemECPrivateKey.data(), pemECPrivateKey.size(), nullptr, 0, nullptr, nullptr))
        != 0) {
        return ret;
    }

    if (mbedtls_pk_get_type(&pk) != MBEDTLS_PK_ECKEY) {
        return aos::ErrorEnum::eInvalidArgument;
    }

    mbedtls_ecp_keypair* ecp = mbedtls_pk_ec(pk);
    if (ecp == nullptr) {
        return aos::ErrorEnum::eInvalidArgument;
    }

    const char* oid;
    size_t      oid_len;
    if ((ret = mbedtls_oid_get_oid_by_ec_grp(ecp->MBEDTLS_PRIVATE(grp).id, &oid, &oid_len)) != 0) {
        return ret;
    }

    paramsOID.Resize(oid_len);
    std::copy(reinterpret_cast<const uint8_t*>(oid), reinterpret_cast<const uint8_t*>(oid) + oid_len, paramsOID.Get());

    uint8_t point_buf[aos::crypto::cECDSAPointDERSize];
    size_t  point_len;
    ret = mbedtls_ecp_point_write_binary(&ecp->MBEDTLS_PRIVATE(grp), &ecp->MBEDTLS_PRIVATE(Q),
        MBEDTLS_ECP_PF_UNCOMPRESSED, &point_len, point_buf, sizeof(point_buf));
    if (ret != 0) {
        return ret;
    }

    ecPoint.Resize(point_len);
    std::copy(point_buf, point_buf + point_len, ecPoint.Get());

    return 0;
}

static int VerifyCertificate(const aos::StaticArray<uint8_t, aos::crypto::cCertPEMSize>& pemCRT)
{
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);

    std::unique_ptr<mbedtls_x509_crt, decltype(&mbedtls_x509_crt_free)> certPtr(&cert, mbedtls_x509_crt_free);

    int ret = mbedtls_x509_crt_parse(&cert, pemCRT.Get(), pemCRT.Size());
    if (ret != 0) {
        return ret;
    }

    uint32_t flags;

    return mbedtls_x509_crt_verify(&cert, &cert, nullptr, nullptr, &flags, nullptr, nullptr);
}

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

class RSAPrivateKey : public aos::crypto::PrivateKeyItf {
public:
    RSAPrivateKey(const aos::crypto::RSAPublicKey& publicKey, std::vector<unsigned char>&& privateKey)
        : mPublicKey(publicKey)
        , mPrivateKey(std::move(privateKey))
    {
    }

    const aos::crypto::PublicKeyItf& GetPublic() const { return mPublicKey; }

    aos::Error Sign(const aos::Array<uint8_t>& digest, const aos::crypto::SignOptions& options,
        aos::Array<uint8_t>& signature) const override
    {
        if (options.mHash != aos::crypto::HashEnum::eSHA256) {
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

        int ret = mbedtls_pk_parse_key(
            pkPtr.get(), mPrivateKey.data(), mPrivateKey.size(), nullptr, 0, mbedtls_ctr_drbg_random, ctrDrbgPtr.get());
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
    aos::crypto::RSAPublicKey  mPublicKey;
    std::vector<unsigned char> mPrivateKey;
};

class ECDSAPrivateKey : public aos::crypto::PrivateKeyItf {
public:
    ECDSAPrivateKey(const aos::crypto::ECDSAPublicKey& publicKey, std::vector<unsigned char>&& privateKey)
        : mPublicKey(publicKey)
        , mPrivateKey(std::move(privateKey))
    {
    }

    const aos::crypto::PublicKeyItf& GetPublic() const { return mPublicKey; }

    aos::Error Sign(const aos::Array<uint8_t>& digest, const aos::crypto::SignOptions& options,
        aos::Array<uint8_t>& signature) const override
    {
        if (options.mHash != aos::crypto::HashEnum::eSHA256) {
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

        int ret = mbedtls_pk_parse_key(
            pkPtr.get(), mPrivateKey.data(), mPrivateKey.size(), nullptr, 0, mbedtls_ctr_drbg_random, ctrDrbgPtr.get());
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
    aos::crypto::ECDSAPublicKey mPublicKey;
    std::vector<unsigned char>  mPrivateKey;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST(CryptoTest, DERToX509Certs)
{
    aos::crypto::MbedTLSCryptoProvider crypto;
    ASSERT_EQ(crypto.Init(), aos::ErrorEnum::eNone);

    aos::crypto::x509::Certificate templ;
    aos::crypto::x509::Certificate parent;

    int64_t now_sec  = static_cast<int64_t>(time(nullptr));
    int64_t now_nsec = 0;

    templ.mNotBefore = aos::Time::Unix(now_sec, now_nsec);

    templ.mNotAfter = aos::Time::Unix(now_sec, now_nsec).Add(aos::Time::cYear);

    const char* subjectName = "C=UA, ST=Some-State, L=Kyiv, O=EPAM";
    ASSERT_EQ(crypto.ASN1EncodeDN(subjectName, templ.mSubject), aos::ErrorEnum::eNone);

    ASSERT_EQ(crypto.ASN1EncodeDN(subjectName, templ.mIssuer), aos::ErrorEnum::eNone);

    aos::StaticArray<uint8_t, aos::crypto::cRSAModulusSize>     mN;
    aos::StaticArray<uint8_t, aos::crypto::cRSAPubExponentSize> mE;

    auto rsaPKRet = GenerateRSAPrivateKey();
    ASSERT_EQ(rsaPKRet.first, aos::ErrorEnum::eNone);

    ASSERT_EQ(ExtractRSAPublicKeyFromPrivateKey((const char*)rsaPKRet.second.data(), mN, mE), 0);

    aos::crypto::RSAPublicKey rsaPublicKey {mN, mE};

    RSAPrivateKey                                        rsaPK {rsaPublicKey, std::move(rsaPKRet.second)};
    aos::StaticArray<uint8_t, aos::crypto::cCertPEMSize> pemCRT;

    ASSERT_EQ(crypto.CreateCertificate(templ, parent, rsaPK, pemCRT), aos::ErrorEnum::eNone);

    std::vector<unsigned char> derCertificate(aos::crypto::cCertDERSize);

    int ret = PemToDer(pemCRT.Get(), pemCRT.Size(), derCertificate);
    ASSERT_EQ(ret, 0);

    aos::Array<uint8_t>            pemBlob(derCertificate.data(), derCertificate.size());
    aos::crypto::x509::Certificate certs;

    auto error = crypto.DERToX509Cert(pemBlob, certs);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
    ASSERT_TRUE(certs.mSubjectKeyId == certs.mAuthorityKeyId);

    aos::StaticString<aos::crypto::cCertSubjSize> subject;
    error = crypto.ASN1DecodeDN(certs.mSubject, subject);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
    ASSERT_TRUE(subject == "C=UA, ST=Some-State, L=Kyiv, O=EPAM");

    aos::StaticString<aos::crypto::cCertSubjSize> issuer;
    error = crypto.ASN1DecodeDN(certs.mIssuer, issuer);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
    ASSERT_TRUE(issuer == "C=UA, ST=Some-State, L=Kyiv, O=EPAM");

    ASSERT_TRUE(certs.mSubject == certs.mIssuer);

    aos::StaticArray<uint8_t, aos::crypto::cCertSubjSize> rawSubject;
    error = crypto.ASN1EncodeDN("C=UA, ST=Some-State, L=Kyiv, O=EPAM", rawSubject);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
}

TEST(CryptoTest, PEMToX509Certs)
{
    aos::crypto::MbedTLSCryptoProvider crypto;
    ASSERT_EQ(crypto.Init(), aos::ErrorEnum::eNone);

    aos::crypto::x509::Certificate templ;
    aos::crypto::x509::Certificate parent;

    int64_t now_sec  = static_cast<int64_t>(time(nullptr));
    int64_t now_nsec = 0;

    templ.mNotBefore = aos::Time::Unix(now_sec, now_nsec);

    templ.mNotAfter = aos::Time::Unix(now_sec, now_nsec).Add(aos::Time::cYear);

    const char* subjectName = "C=UA, ST=Some-State, L=Kyiv, O=EPAM";
    ASSERT_EQ(crypto.ASN1EncodeDN(subjectName, templ.mSubject), aos::ErrorEnum::eNone);

    ASSERT_EQ(crypto.ASN1EncodeDN(subjectName, templ.mIssuer), aos::ErrorEnum::eNone);

    aos::StaticArray<uint8_t, aos::crypto::cRSAModulusSize>     mN;
    aos::StaticArray<uint8_t, aos::crypto::cRSAPubExponentSize> mE;

    auto rsaPKRet = GenerateRSAPrivateKey();
    ASSERT_EQ(rsaPKRet.first, aos::ErrorEnum::eNone);

    ASSERT_EQ(ExtractRSAPublicKeyFromPrivateKey((const char*)rsaPKRet.second.data(), mN, mE), 0);

    aos::crypto::RSAPublicKey rsaPublicKey {mN, mE};

    RSAPrivateKey                                        rsaPK {rsaPublicKey, std::move(rsaPKRet.second)};
    aos::StaticArray<uint8_t, aos::crypto::cCertPEMSize> pemCRT;

    ASSERT_EQ(crypto.CreateCertificate(templ, parent, rsaPK, pemCRT), aos::ErrorEnum::eNone);

    aos::Array<uint8_t>                                 pemBlob(pemCRT.Get(), pemCRT.Size());
    aos::StaticArray<aos::crypto::x509::Certificate, 1> certs;

    auto error = crypto.PEMToX509Certs(pemBlob, certs);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
    ASSERT_EQ(certs.Size(), 1);

    ASSERT_TRUE(certs[0].mSubjectKeyId == certs[0].mAuthorityKeyId);

    aos::StaticString<aos::crypto::cCertSubjSize> subject;
    error = crypto.ASN1DecodeDN(certs[0].mSubject, subject);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
    ASSERT_TRUE(subject == "C=UA, ST=Some-State, L=Kyiv, O=EPAM");

    aos::StaticString<aos::crypto::cCertSubjSize> issuer;
    error = crypto.ASN1DecodeDN(certs[0].mIssuer, issuer);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
    ASSERT_TRUE(issuer == "C=UA, ST=Some-State, L=Kyiv, O=EPAM");

    ASSERT_TRUE(certs[0].mSubject == certs[0].mIssuer);

    aos::StaticArray<uint8_t, aos::crypto::cCertSubjSize> rawSubject;
    error = crypto.ASN1EncodeDN("C=UA, ST=Some-State, L=Kyiv, O=EPAM", rawSubject);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
}

TEST(CryptoTest, CreateCSR)
{
    aos::crypto::MbedTLSCryptoProvider crypto;

    ASSERT_EQ(crypto.Init(), aos::ErrorEnum::eNone);

    aos::crypto::x509::CSR templ;
    const char*            subjectName = "CN=Test Subject,O=Org,C=GB";
    ASSERT_EQ(crypto.ASN1EncodeDN(subjectName, templ.mSubject), aos::ErrorEnum::eNone);

    templ.mDNSNames.Resize(2);
    templ.mDNSNames[0] = "test1.com";
    templ.mDNSNames[1] = "test2.com";

    const unsigned char subject_key_identifier_val[]
        = {0x64, 0xD3, 0x7C, 0x30, 0xA0, 0xE1, 0xDC, 0x0C, 0xFE, 0xA0, 0x06, 0x0A, 0xC3, 0x08, 0xB7, 0x76};

    size_t val_len = sizeof(subject_key_identifier_val);

    templ.mExtraExtensions.Resize(1);

    templ.mExtraExtensions[0].mId    = MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER;
    templ.mExtraExtensions[0].mValue = aos::Array<uint8_t>(subject_key_identifier_val, val_len);

    aos::StaticArray<uint8_t, 4096> pemCSR;

    aos::StaticArray<uint8_t, aos::crypto::cRSAModulusSize>     mN;
    aos::StaticArray<uint8_t, aos::crypto::cRSAPubExponentSize> mE;

    auto rsaPKRet = GenerateRSAPrivateKey();
    ASSERT_EQ(rsaPKRet.first, aos::ErrorEnum::eNone);

    auto ret = ExtractRSAPublicKeyFromPrivateKey((const char*)rsaPKRet.second.data(), mN, mE);
    ASSERT_EQ(ret, 0);
    aos::crypto::RSAPublicKey rsaPublicKey {mN, mE};

    RSAPrivateKey rsaPK {rsaPublicKey, std::move(rsaPKRet.second)};
    auto          error = crypto.CreateCSR(templ, rsaPK, pemCSR);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
    ASSERT_FALSE(pemCSR.IsEmpty());

    mbedtls_x509_csr csr;
    mbedtls_x509_csr_init(&csr);

    std::unique_ptr<mbedtls_x509_csr, decltype(&mbedtls_x509_csr_free)> csrPtr(&csr, mbedtls_x509_csr_free);

    ret = mbedtls_x509_csr_parse(csrPtr.get(), pemCSR.Get(), pemCSR.Size());
    ASSERT_EQ(ret, 0);
}

TEST(CryptoTest, CreateSelfSignedCert)
{
    aos::crypto::MbedTLSCryptoProvider crypto;
    ASSERT_EQ(crypto.Init(), aos::ErrorEnum::eNone);

    aos::crypto::x509::Certificate templ;
    aos::crypto::x509::Certificate parent;

    int64_t now_sec  = static_cast<int64_t>(time(nullptr));
    int64_t now_nsec = 0;

    templ.mNotBefore = aos::Time::Unix(now_sec, now_nsec);

    templ.mNotAfter = aos::Time::Unix(now_sec, now_nsec).Add(aos::Time::cYear);

    const char* subjectName = "CN=Test,O=Org,C=UA";
    ASSERT_EQ(crypto.ASN1EncodeDN(subjectName, templ.mSubject), aos::ErrorEnum::eNone);

    ASSERT_EQ(crypto.ASN1EncodeDN(subjectName, templ.mIssuer), aos::ErrorEnum::eNone);

    aos::StaticArray<uint8_t, aos::crypto::cRSAModulusSize>     mN;
    aos::StaticArray<uint8_t, aos::crypto::cRSAPubExponentSize> mE;

    auto rsaPKRet = GenerateRSAPrivateKey();
    ASSERT_EQ(rsaPKRet.first, aos::ErrorEnum::eNone);

    ASSERT_EQ(ExtractRSAPublicKeyFromPrivateKey((const char*)rsaPKRet.second.data(), mN, mE), 0);

    aos::crypto::RSAPublicKey rsaPublicKey {mN, mE};

    RSAPrivateKey                                        rsaPK {rsaPublicKey, std::move(rsaPKRet.second)};
    aos::StaticArray<uint8_t, aos::crypto::cCertPEMSize> pemCRT;

    ASSERT_EQ(crypto.CreateCertificate(templ, parent, rsaPK, pemCRT), aos::ErrorEnum::eNone);

    ASSERT_EQ(VerifyCertificate(pemCRT), 0);
}

TEST(CryptoTest, CreateCSRUsingECKey)
{
    aos::crypto::MbedTLSCryptoProvider crypto;

    ASSERT_EQ(crypto.Init(), aos::ErrorEnum::eNone);

    aos::crypto::x509::CSR templ;
    const char*            subjectName = "CN=Test Subject,O=Org,C=GB";
    ASSERT_EQ(crypto.ASN1EncodeDN(subjectName, templ.mSubject), aos::ErrorEnum::eNone);

    templ.mDNSNames.Resize(2);
    templ.mDNSNames[0] = "test1.com";
    templ.mDNSNames[1] = "test2.com";

    const unsigned char subject_key_identifier_val[]
        = {0x64, 0xD3, 0x7C, 0x30, 0xA0, 0xE1, 0xDC, 0x0C, 0xFE, 0xA0, 0x06, 0x0A, 0xC3, 0x08, 0xB7, 0x76};

    size_t val_len = sizeof(subject_key_identifier_val);

    templ.mExtraExtensions.Resize(1);

    templ.mExtraExtensions[0].mId    = MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER;
    templ.mExtraExtensions[0].mValue = aos::Array<uint8_t>(subject_key_identifier_val, val_len);

    aos::StaticArray<uint8_t, 4096> pemCSR;

    aos::StaticArray<uint8_t, aos::crypto::cECDSAParamsOIDSize> mParamsOID;
    aos::StaticArray<uint8_t, aos::crypto::cECDSAPointDERSize>  mECPoint;

    auto ecPrivateKey = GenerateECPrivateKey();
    ASSERT_EQ(ecPrivateKey.first, aos::ErrorEnum::eNone);

    auto ret = ExtractECPublicKeyFromPrivate(mParamsOID, mECPoint, ecPrivateKey.second);
    ASSERT_EQ(ret, 0);

    aos::crypto::ECDSAPublicKey ecdsaPublicKey(mParamsOID, mECPoint);

    ECDSAPrivateKey ecdsaPK(ecdsaPublicKey, std::move(ecPrivateKey.second));
    auto            error = crypto.CreateCSR(templ, ecdsaPK, pemCSR);

    ASSERT_EQ(error, aos::ErrorEnum::eNone);
    ASSERT_FALSE(pemCSR.IsEmpty());

    mbedtls_x509_csr csr;
    mbedtls_x509_csr_init(&csr);

    std::unique_ptr<mbedtls_x509_csr, decltype(&mbedtls_x509_csr_free)> csrPtr(&csr, mbedtls_x509_csr_free);

    ret = mbedtls_x509_csr_parse(csrPtr.get(), pemCSR.Get(), pemCSR.Size());
    ASSERT_EQ(ret, 0);
}
