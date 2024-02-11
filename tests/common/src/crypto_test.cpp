/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <gmock/gmock.h>
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

using namespace testing;

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

static int PemToDer(const char* pemData, size_t pemLen, std::vector<unsigned char>& derData)
{
    mbedtls_x509_crt cert;

    mbedtls_x509_crt_init(&cert);

    std::unique_ptr<mbedtls_x509_crt, decltype(&mbedtls_x509_crt_free)> derDataPtr(&cert, mbedtls_x509_crt_free);

    auto ret = mbedtls_x509_crt_parse(derDataPtr.get(), reinterpret_cast<const uint8_t*>(pemData), pemLen);
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

static int VerifyCertificate(const aos::String& pemCRT)
{
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);

    std::unique_ptr<mbedtls_x509_crt, decltype(&mbedtls_x509_crt_free)> certPtr(&cert, mbedtls_x509_crt_free);

    int ret = mbedtls_x509_crt_parse(&cert, reinterpret_cast<const uint8_t*>(pemCRT.Get()), pemCRT.Size() + 1);
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

    RSAPrivateKey                               rsaPK {rsaPublicKey, std::move(rsaPKRet.second)};
    aos::StaticString<aos::crypto::cCertPEMLen> pemCRT;

    ASSERT_EQ(crypto.CreateCertificate(templ, parent, rsaPK, pemCRT), aos::ErrorEnum::eNone);

    std::vector<unsigned char> derCertificate(aos::crypto::cCertDERSize);

    int ret = PemToDer(pemCRT.CStr(), pemCRT.Size() + 1, derCertificate);
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

    ASSERT_TRUE(certs.mPublicKey->IsEqual(rsaPublicKey));

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

    aos::StaticArray<uint8_t, aos::crypto::cECDSAParamsOIDSize> mParamsOID;
    aos::StaticArray<uint8_t, aos::crypto::cECDSAPointDERSize>  mECPoint;

    auto ecPrivateKey = GenerateECPrivateKey();
    ASSERT_EQ(ecPrivateKey.first, aos::ErrorEnum::eNone);

    auto ret = ExtractECPublicKeyFromPrivate(mParamsOID, mECPoint, ecPrivateKey.second);
    ASSERT_EQ(ret, 0);

    aos::crypto::ECDSAPublicKey ecdsaPublicKey(mParamsOID, mECPoint);

    ECDSAPrivateKey ecdsaPK(ecdsaPublicKey, std::move(ecPrivateKey.second));

    aos::StaticString<aos::crypto::cCertPEMLen> pemCRT;

    ASSERT_EQ(crypto.CreateCertificate(templ, parent, ecdsaPK, pemCRT), aos::ErrorEnum::eNone);

    aos::String                                         pemBlob(pemCRT.Get(), pemCRT.Size());
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

    ASSERT_TRUE(certs[0].mPublicKey->IsEqual(ecdsaPublicKey));

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

    templ.mExtraExtensions[0].mID    = MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER;
    templ.mExtraExtensions[0].mValue = aos::Array<uint8_t>(subject_key_identifier_val, val_len);

    aos::StaticString<4096> pemCSR;

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

    ret = mbedtls_x509_csr_parse(csrPtr.get(), reinterpret_cast<const uint8_t*>(pemCSR.Get()), pemCSR.Size() + 1);
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

    RSAPrivateKey                               rsaPK {rsaPublicKey, std::move(rsaPKRet.second)};
    aos::StaticString<aos::crypto::cCertPEMLen> pemCRT;

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

    templ.mExtraExtensions[0].mID    = MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER;
    templ.mExtraExtensions[0].mValue = aos::Array<uint8_t>(subject_key_identifier_val, val_len);

    aos::StaticString<4096> pemCSR;

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

    ret = mbedtls_x509_csr_parse(csrPtr.get(), reinterpret_cast<const uint8_t*>(pemCSR.Get()), pemCSR.Size() + 1);
    ASSERT_EQ(ret, 0);
}

/***********************************************************************************************************************
 * Tests are written using
 * ASN1 dumper: https://kjur.github.io/jsrsasign/tool/tool_asn1dumper.html
 * ASN1 encoder: https://github.com/andrivet/python-asn1/tree/master
 **********************************************************************************************************************/

/**
 * Python script to check encryption is correct:
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.enter(asn1.Numbers.Sequence)
 * enc.write('1.3.6.1.5.5.7.3.1', asn1.Numbers.ObjectIdentifier)
 * enc.write('1.3.6.1.5.5.7.3.2', asn1.Numbers.ObjectIdentifier)
 * enc.leave()
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x'%b for b in encoded_bytes))
 */
TEST(CryptoTest, ASN1EncodeObjectIds)
{
    aos::crypto::MbedTLSCryptoProvider provider;

    static constexpr auto cOidExtKeyUsageServerAuth = "1.3.6.1.5.5.7.3.1";
    static constexpr auto cOidExtKeyUsageClientAuth = "1.3.6.1.5.5.7.3.2";

    aos::StaticArray<aos::crypto::asn1::ObjectIdentifier, 3> oids;
    aos::StaticArray<uint8_t, 100>                           asn1Value;

    oids.PushBack(cOidExtKeyUsageServerAuth);
    oids.PushBack(cOidExtKeyUsageClientAuth);

    ASSERT_EQ(provider.ASN1EncodeObjectIds(oids, asn1Value), aos::ErrorEnum::eNone);

    const std::vector<uint8_t> actual       = {asn1Value.begin(), asn1Value.end()};
    const std::vector<uint8_t> expectedASN1 = {0x30, 0x14, 0x6, 0x8, 0x2b, 0x6, 0x1, 0x5, 0x5, 0x7, 0x3, 0x1, 0x6, 0x8,
        0x2b, 0x6, 0x1, 0x5, 0x5, 0x7, 0x3, 0x2};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

/**
 * Python script to check encryption is correct:
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.enter(asn1.Numbers.Sequence)
 * enc.leave()
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x'%b for b in encoded_bytes))
 */
TEST(CryptoTest, ASN1EncodeObjectIdsEmptyOIDS)
{
    aos::crypto::MbedTLSCryptoProvider provider;

    aos::StaticArray<aos::crypto::asn1::ObjectIdentifier, 3> oids;
    aos::StaticArray<uint8_t, 100>                           asn1Value;

    ASSERT_EQ(provider.ASN1EncodeObjectIds(oids, asn1Value), aos::ErrorEnum::eNone);

    const std::vector<uint8_t> actual       = {asn1Value.begin(), asn1Value.end()};
    const std::vector<uint8_t> expectedASN1 = {0x30, 0x0};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

/**
 * Python script to check encryption is correct:
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.write(0x17ad4f605cdae79e)
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x'%b for b in encoded_bytes))
 */
TEST(CryptoTest, ASN1EncodeBigInt)
{
    aos::crypto::MbedTLSCryptoProvider provider;

    const uint64_t            bigInt      = 0x17ad4f605cdae79e;
    const aos::Array<uint8_t> inputBigInt = {reinterpret_cast<const uint8_t*>(&bigInt), sizeof(bigInt)};

    aos::StaticArray<uint8_t, 100> asn1Value;

    ASSERT_EQ(provider.ASN1EncodeBigInt(inputBigInt, asn1Value), aos::ErrorEnum::eNone);

    std::vector<uint8_t> actual = {asn1Value.begin(), asn1Value.end()};

    //  Currently BigInt is stored in a little endian format.
    //  It might not be correct, comparing to python asn1 encoder(which uses big endian).
    //  Nevertheless otherwise ECDSA::Sign(PKCS11)/Verify(mbedtls) combination doesn't work.
    //  A topic for future investigation.
    const std::vector<uint8_t> expectedASN1 = {0x2, 0x8, 0x9e, 0xe7, 0xda, 0x5c, 0x60, 0x4f, 0xad, 0x17};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

/**
 * Python script to check encryption is correct:
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.enter(asn1.Numbers.Sequence)
 * enc.write('1.3.6.1.5.5.7.3.1', asn1.Numbers.ObjectIdentifier)
 * enc.write(0x17ad4f605cdae79e)
 * enc.leave()
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x'%b for b in encoded_bytes))
 */
TEST(CryptoTest, ASN1EncodeDERSequence)
{
    aos::crypto::MbedTLSCryptoProvider provider;

    const uint8_t cOidServerAuth[] = {0x6, 0x8, 0x2b, 0x6, 0x1, 0x5, 0x5, 0x7, 0x3, 0x1};
    const uint8_t cBigInt[]        = {0x2, 0x8, 0x17, 0xad, 0x4f, 0x60, 0x5c, 0xda, 0xe7, 0x9e};

    aos::StaticArray<aos::Array<uint8_t>, 2> src;

    src.PushBack(aos::Array<uint8_t>(cOidServerAuth, sizeof(cOidServerAuth)));
    src.PushBack(aos::Array<uint8_t>(cBigInt, sizeof(cBigInt)));

    aos::StaticArray<uint8_t, 100> asn1Value;

    ASSERT_EQ(provider.ASN1EncodeDERSequence(src, asn1Value), aos::ErrorEnum::eNone);

    std::vector<uint8_t>       actual       = {asn1Value.begin(), asn1Value.end()};
    const std::vector<uint8_t> expectedASN1 = {0x30, 0x14, 0x6, 0x8, 0x2b, 0x6, 0x1, 0x5, 0x5, 0x7, 0x3, 0x1, 0x2, 0x8,
        0x17, 0xad, 0x4f, 0x60, 0x5c, 0xda, 0xe7, 0x9e};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

/**
 * Python script to check encryption is correct:
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.enter(asn1.Numbers.Sequence)
 * enc.leave()
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x'%b for b in encoded_bytes))
 */
TEST(CryptoTest, ASN1EncodeDERSequenceEmptySequence)
{
    aos::crypto::MbedTLSCryptoProvider provider;

    aos::StaticArray<aos::Array<uint8_t>, 2> src;
    aos::StaticArray<uint8_t, 100>           asn1Value;

    ASSERT_EQ(provider.ASN1EncodeDERSequence(src, asn1Value), aos::ErrorEnum::eNone);

    std::vector<uint8_t>       actual       = {asn1Value.begin(), asn1Value.end()};
    const std::vector<uint8_t> expectedASN1 = {0x30, 0x0};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

/**
 * Python script to check encryption is correct:
 * import cryptography
 * from cryptography import x509
 *
 * dn = x509.Name.from_rfc4514_string("C=UA,CN=Aos Core")
 * encoded_bytes = dn.public_bytes()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x'%b for b in encoded_bytes))
 */
TEST(CryptoTest, ASN1EncodeDN)
{
    aos::crypto::MbedTLSCryptoProvider provider;

    aos::StaticString<100> src = "C=UA,CN=Aos Core";

    aos::StaticArray<uint8_t, 100> asn1Value;

    ASSERT_EQ(provider.ASN1EncodeDN(src, asn1Value), aos::ErrorEnum::eNone);

    std::vector<uint8_t>       actual = {asn1Value.begin(), asn1Value.end()};
    const std::vector<uint8_t> expectedASN1
        = {0x30, 0x20, 0x31, 0xb, 0x30, 0x9, 0x6, 0x3, 0x55, 0x4, 0x6, 0x13, 0x2, 0x55, 0x41, 0x31, 0x11, 0x30, 0xf,
            0x6, 0x3, 0x55, 0x4, 0x3, 0xc, 0x8, 0x41, 0x6f, 0x73, 0x20, 0x43, 0x6f, 0x72, 0x65};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

/**
 * Python script to check decryption is correct:
 * import cryptography
 * from cryptography import x509
 *
 * dn = x509.Name.from_rfc4514_string("C=UA,CN=Aos Core")
 * encoded_bytes = dn.public_bytes()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x'%b for b in encoded_bytes))
 */
TEST(CryptoTest, ASN1DecodeDN)
{
    aos::crypto::MbedTLSCryptoProvider provider;

    const std::vector<uint8_t> asn1Val
        = {0x30, 0x20, 0x31, 0xb, 0x30, 0x9, 0x6, 0x3, 0x55, 0x4, 0x6, 0x13, 0x2, 0x55, 0x41, 0x31, 0x11, 0x30, 0xf,
            0x6, 0x3, 0x55, 0x4, 0x3, 0xc, 0x8, 0x41, 0x6f, 0x73, 0x20, 0x43, 0x6f, 0x72, 0x65};

    const auto input = aos::Array<uint8_t>(asn1Val.data(), asn1Val.size());

    aos::StaticString<100> result;

    ASSERT_EQ(provider.ASN1DecodeDN(input, result), aos::ErrorEnum::eNone);

    EXPECT_THAT(std::string(result.CStr()), "C=UA, CN=Aos Core");
}
