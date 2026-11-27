/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <core/common/crypto/cryptoutils.hpp>
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/stubs/failingallocator.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/fs.hpp>

#if defined(WITH_MBEDTLS)
#include <core/common/tests/crypto/providers/mbedtlsfactory.hpp>
#endif

#if defined(WITH_OPENSSL)
#include <core/common/tests/crypto/providers/opensslfactory.hpp>
#endif

#include "gcmtestvector.hpp"

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#endif

using namespace testing;
using aos::tests::FailingAllocator;

namespace aos::crypto {
/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CryptoProviderTest : public TestWithParam<std::shared_ptr<CryptoFactoryItf>> {
public:
    void SetUp() override
    {
        tests::utils::InitLog();

        mFactory = GetParam();

        ASSERT_TRUE(mFactory->Init(mAllocator).IsNone());

        mCryptoProvider = &mFactory->GetCryptoProvider();
        mHashProvider   = &mFactory->GetHashProvider();
        mRandomProvider = &mFactory->GetRandomProvider();
    }

protected:
    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    FailingAllocator mAllocator;

    std::shared_ptr<CryptoFactoryItf> mFactory;
    CryptoProviderItf*                mCryptoProvider;
    HasherItf*                        mHashProvider;
    RandomItf*                        mRandomProvider;
};

static auto GenFactories()
{
    std::shared_ptr<CryptoFactoryItf> values[] = {
#ifdef WITH_MBEDTLS
        std::make_shared<MBedTLSCryptoFactory>(),
#endif

#ifdef WITH_OPENSSL
        std::make_shared<OpenSSLCryptoFactory>(),
#endif
    };

    return ValuesIn(values);
}

// Required to beautify "GetParam() =" ctest reporting
static void PrintTo(const std::shared_ptr<CryptoFactoryItf>& param, std::ostream* os)
{
    *os << param->GetName();
}

INSTANTIATE_TEST_SUITE_P(Crypto, CryptoProviderTest, GenFactories(),
    [](const TestParamInfo<CryptoProviderTest::ParamType>& info) { return info.param->GetName(); });

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CryptoProviderTest);

/***********************************************************************************************************************
 * Statics
 **********************************************************************************************************************/

RetWithError<x509::Certificate> CreateCertTemplate(const char* name, x509::ProviderItf& mCryptoProvider)
{
    x509::Certificate templ;

    int64_t nowSec  = static_cast<int64_t>(time(nullptr));
    int64_t nowNSec = 0;

    templ.mNotBefore = Time::Unix(nowSec, nowNSec);
    templ.mNotAfter  = Time::Unix(nowSec, nowNSec).Add(Time::cYear);
    templ.mPublicKey.SetValue(RSAPublicKey({}, {}));

    auto err = mCryptoProvider.ASN1EncodeDN(name, templ.mSubject);
    if (!err.IsNone()) {
        return {templ, err};
    }

    err = mCryptoProvider.ASN1EncodeDN(name, templ.mIssuer);
    if (!err.IsNone()) {
        return {templ, err};
    }

    return {templ, ErrorEnum::eNone};
}

void CreateCertificate(CryptoFactoryItf& factory, CryptoProviderItf& provider, const char* subjectName, KeyType keyType,
    std::shared_ptr<PrivateKeyItf>& privKey, Array<x509::Certificate>& certs)
{
    x509::Certificate templ;
    x509::Certificate parent;

    int64_t nowSec = static_cast<int64_t>(time(nullptr));

    templ.mNotBefore = Time::Unix(nowSec, 0);
    templ.mNotAfter  = Time::Unix(nowSec, 0).Add(Time::cYear);

    ASSERT_TRUE(provider.ASN1EncodeDN(subjectName, templ.mSubject).IsNone());
    ASSERT_TRUE(provider.ASN1EncodeDN(subjectName, templ.mIssuer).IsNone());

    Error genErr;

    switch (keyType.GetValue()) {
    case KeyTypeEnum::eECDSA:
        Tie(privKey, genErr) = factory.GenerateECDSAPrivKey();
        ASSERT_TRUE(genErr.IsNone());
        break;
    case KeyTypeEnum::eRSA:
        Tie(privKey, genErr) = factory.GenerateRSAPrivKey();
        ASSERT_TRUE(genErr.IsNone());
        break;
    default:
        ASSERT_TRUE(false) << "Unknown key type";
    }

    StaticString<cCertPEMLen> pemCRT;
    ASSERT_TRUE(provider.CreateCertificate(templ, parent, *privKey, pemCRT).IsNone());

    ASSERT_TRUE(provider.PEMToX509Certs(pemCRT, certs).IsNone());
    ASSERT_EQ(certs.Size(), 1);
    ASSERT_EQ(certs[0].mSubjectKeyID, certs[0].mAuthorityKeyID);
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_P(CryptoProviderTest, DERToX509Certs)
{
    // Create certificate in PEM
    Error             err;
    x509::Certificate parent, templ;

    const char* subjectName = "C=UA, ST=Some-State, L=Kyiv, O=EPAM";
    Tie(templ, err)         = CreateCertTemplate(subjectName, *mCryptoProvider);
    ASSERT_TRUE(err.IsNone());

    std::shared_ptr<PrivateKeyItf> rsaPrivKey;

    Tie(rsaPrivKey, err) = mFactory->GenerateRSAPrivKey();
    ASSERT_TRUE(err.IsNone());

    StaticString<cCertPEMLen> pemCRT;

    err = mCryptoProvider->CreateCertificate(templ, parent, *rsaPrivKey, pemCRT);
    LOG_ERR() << "Create cert error=" << err;
    ASSERT_EQ(err, ErrorEnum::eNone);

    std::vector<uint8_t> derCert;

    // Convert certificate to DER
    Tie(derCert, err) = mFactory->PemCertToDer(pemCRT.CStr());
    ASSERT_TRUE(err.IsNone());

    // Convert DER to aos x509 certificates
    x509::Certificate cert;

    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(tests::utils::ConvertToArray(derCert), cert).IsNone());

    ASSERT_TRUE(cert.mSubjectKeyID == cert.mAuthorityKeyID);

    StaticString<cCertSubjSize> subject;
    StaticString<cCertSubjSize> issuer;

    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN(cert.mSubject, subject).IsNone());
    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN(cert.mIssuer, issuer).IsNone());

    ASSERT_EQ(std::string(subject.CStr()), std::string(subjectName));
    ASSERT_EQ(std::string(issuer.CStr()), std::string(subjectName));

    const auto& rsaPubKey = static_cast<const RSAPublicKey&>(rsaPrivKey->GetPublic());
    ASSERT_TRUE(GetBase<PublicKeyItf>(cert.mPublicKey).IsEqual(rsaPubKey));
}

TEST_P(CryptoProviderTest, PEMToX509Certs)
{
    x509::Certificate templ;
    x509::Certificate parent;

    int64_t nowSec = static_cast<int64_t>(time(nullptr));

    templ.mNotBefore = Time::Unix(nowSec, 0);
    templ.mNotAfter  = Time::Unix(nowSec, 0).Add(Time::cYear);

    const char* subjectName = "C=UA, ST=Some-State, L=Kyiv, O=EPAM";
    ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN(subjectName, templ.mSubject).IsNone());
    ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN(subjectName, templ.mIssuer).IsNone());

    auto [ecdsaPK, genErr] = mFactory->GenerateECDSAPrivKey();
    ASSERT_TRUE(genErr.IsNone());

    StaticString<cCertPEMLen> pemCRT;
    ASSERT_TRUE(mCryptoProvider->CreateCertificate(templ, parent, *ecdsaPK, pemCRT).IsNone());

    String                            pemBlob(pemCRT.Get(), pemCRT.Size());
    StaticArray<x509::Certificate, 1> certs;

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(pemBlob, certs).IsNone());
    ASSERT_EQ(certs.Size(), 1);
    ASSERT_EQ(certs[0].mSubjectKeyID, certs[0].mAuthorityKeyID);

    StaticString<cCertSubjSize> subject;
    StaticString<cCertSubjSize> issuer;

    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN(certs[0].mSubject, subject).IsNone());
    ASSERT_EQ(subject, "C=UA, ST=Some-State, L=Kyiv, O=EPAM");

    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN(certs[0].mIssuer, issuer).IsNone());
    ASSERT_EQ(issuer, "C=UA, ST=Some-State, L=Kyiv, O=EPAM");

    ASSERT_EQ(certs[0].mSubject, certs[0].mIssuer);

    ASSERT_TRUE(GetBase<PublicKeyItf>(certs[0].mPublicKey).IsEqual(ecdsaPK->GetPublic()));
}

TEST_P(CryptoProviderTest, CreateCSR)
{
    x509::CSR   templ;
    const char* subjectName = "CN=Test, O=Org, C=GB";

    ASSERT_EQ(mCryptoProvider->ASN1EncodeDN(subjectName, templ.mSubject), ErrorEnum::eNone);

    templ.mDNSNames.Resize(2);
    templ.mDNSNames[0] = "test1.com";
    templ.mDNSNames[1] = "test2.com";

    const unsigned char clientAuth[] = {0x30, 0xa, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02};

    templ.mExtraExtensions.Resize(1);

    templ.mExtraExtensions[0].mID    = "2.5.29.37";
    templ.mExtraExtensions[0].mValue = Array<uint8_t>(clientAuth, sizeof(clientAuth));

    StaticString<4096> pemCSR;

    auto [rsaPrivKey, err] = mFactory->GenerateRSAPrivKey();
    ASSERT_TRUE(err.IsNone());

    ASSERT_TRUE(mCryptoProvider->CreateCSR(templ, *rsaPrivKey, pemCSR).IsNone());
    ASSERT_FALSE(pemCSR.IsEmpty());

    // Check CSR is valid.
    ASSERT_TRUE(mFactory->VerifyCSR(pemCSR.CStr()));
}

TEST_P(CryptoProviderTest, CreateSelfSignedCert)
{
    x509::Certificate templ;
    x509::Certificate parent;

    int64_t now_sec  = static_cast<int64_t>(time(nullptr));
    int64_t now_nsec = 0;

    templ.mNotBefore = Time::Unix(now_sec, now_nsec);
    templ.mNotAfter  = Time::Unix(now_sec, now_nsec).Add(Time::cYear);

    const char* subjectName = "CN=Test, O=Org, C=UA";
    ASSERT_EQ(mCryptoProvider->ASN1EncodeDN(subjectName, templ.mSubject), ErrorEnum::eNone);
    ASSERT_EQ(mCryptoProvider->ASN1EncodeDN(subjectName, templ.mIssuer), ErrorEnum::eNone);

    auto [rsaPrivKey, err] = mFactory->GenerateRSAPrivKey();
    ASSERT_TRUE(err.IsNone());

    StaticString<cCertPEMLen> pemCRT;

    ASSERT_EQ(mCryptoProvider->CreateCertificate(templ, parent, *rsaPrivKey, pemCRT), ErrorEnum::eNone);

    ASSERT_TRUE(mFactory->VerifyCertificate(pemCRT.CStr()));
}

TEST_P(CryptoProviderTest, CreateCSRUsingECKey)
{
    x509::CSR   templ;
    const char* subjectName = "CN=Test Subject, O=Org, C=GB";
    ASSERT_EQ(mCryptoProvider->ASN1EncodeDN(subjectName, templ.mSubject), aos::ErrorEnum::eNone);

    templ.mDNSNames.Resize(2);
    templ.mDNSNames[0] = "test1.com";
    templ.mDNSNames[1] = "test2.com";

    const unsigned char clientAuth[] = {0x30, 0xa, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02};

    templ.mExtraExtensions.Resize(1);

    templ.mExtraExtensions[0].mID    = "2.5.29.37";
    templ.mExtraExtensions[0].mValue = Array<uint8_t>(clientAuth, sizeof(clientAuth));

    StaticString<4096> pemCSR;

    auto [ecdsaPrivKey, err] = mFactory->GenerateECDSAPrivKey();
    ASSERT_TRUE(err.IsNone());

    err = mCryptoProvider->CreateCSR(templ, *ecdsaPrivKey, pemCSR);
    ASSERT_TRUE(err.IsNone());

    ASSERT_FALSE(pemCSR.IsEmpty());

    ASSERT_TRUE(mFactory->VerifyCSR(pemCSR.CStr()));
}

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
TEST_P(CryptoProviderTest, ASN1EncodeObjectIds)
{
    static constexpr auto cOidExtKeyUsageServerAuth = "1.3.6.1.5.5.7.3.1";
    static constexpr auto cOidExtKeyUsageClientAuth = "1.3.6.1.5.5.7.3.2";

    StaticArray<asn1::ObjectIdentifier, 3> oids;
    StaticArray<uint8_t, 100>              asn1Value;

    oids.PushBack(cOidExtKeyUsageServerAuth);
    oids.PushBack(cOidExtKeyUsageClientAuth);

    ASSERT_EQ(mCryptoProvider->ASN1EncodeObjectIds(oids, asn1Value), aos::ErrorEnum::eNone);

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
TEST_P(CryptoProviderTest, ASN1EncodeObjectIdsEmptyOIDS)
{
    StaticArray<asn1::ObjectIdentifier, 3> oids;
    StaticArray<uint8_t, 100>              asn1Value;

    ASSERT_EQ(mCryptoProvider->ASN1EncodeObjectIds(oids, asn1Value), aos::ErrorEnum::eNone);

    const std::vector<uint8_t> actual       = {asn1Value.begin(), asn1Value.end()};
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
TEST_P(CryptoProviderTest, ASN1EncodeDN)
{
    StaticString<100>         src = "C=UA, CN=Aos Core";
    StaticArray<uint8_t, 100> asn1Value;

    ASSERT_EQ(mCryptoProvider->ASN1EncodeDN(src, asn1Value), aos::ErrorEnum::eNone);

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
TEST_P(CryptoProviderTest, ASN1DecodeDN)
{
    const std::vector<uint8_t> asn1Val
        = {0x30, 0x20, 0x31, 0xb, 0x30, 0x9, 0x6, 0x3, 0x55, 0x4, 0x6, 0x13, 0x2, 0x55, 0x41, 0x31, 0x11, 0x30, 0xf,
            0x6, 0x3, 0x55, 0x4, 0x3, 0xc, 0x8, 0x41, 0x6f, 0x73, 0x20, 0x43, 0x6f, 0x72, 0x65};

    const auto input = Array<uint8_t>(asn1Val.data(), asn1Val.size());

    StaticString<100> result;

    ASSERT_EQ(mCryptoProvider->ASN1DecodeDN(input, result), aos::ErrorEnum::eNone);

    EXPECT_THAT(std::string(result.CStr()), "C=UA, CN=Aos Core");
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
TEST_P(CryptoProviderTest, ASN1EncodeBigInt)
{
    const uint64_t            bigInt      = 0x17ad4f605cdae79e;
    const Array<uint8_t>      inputBigInt = {reinterpret_cast<const uint8_t*>(&bigInt), sizeof(bigInt)};
    StaticArray<uint8_t, 100> asn1Value;

    ASSERT_EQ(mCryptoProvider->ASN1EncodeBigInt(inputBigInt, asn1Value), aos::ErrorEnum::eNone);

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
TEST_P(CryptoProviderTest, ASN1EncodeDERSequence)
{
    const uint8_t cOidServerAuth[] = {0x6, 0x8, 0x2b, 0x6, 0x1, 0x5, 0x5, 0x7, 0x3, 0x1};
    const uint8_t cBigInt[]        = {0x2, 0x8, 0x17, 0xad, 0x4f, 0x60, 0x5c, 0xda, 0xe7, 0x9e};

    StaticArray<Array<uint8_t>, 2> src;

    src.PushBack(Array<uint8_t>(cOidServerAuth, sizeof(cOidServerAuth)));
    src.PushBack(Array<uint8_t>(cBigInt, sizeof(cBigInt)));

    StaticArray<uint8_t, 100> asn1Value;

    ASSERT_EQ(mCryptoProvider->ASN1EncodeDERSequence(src, asn1Value), aos::ErrorEnum::eNone);

    std::vector<uint8_t>       actual       = {asn1Value.begin(), asn1Value.end()};
    const std::vector<uint8_t> expectedASN1 = {0x30, 0x14, 0x6, 0x8, 0x2b, 0x6, 0x1, 0x5, 0x5, 0x7, 0x3, 0x1, 0x2, 0x8,
        0x17, 0xad, 0x4f, 0x60, 0x5c, 0xda, 0xe7, 0x9e};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

TEST_P(CryptoProviderTest, ASN1DecodeOctetString)
{
    const uint8_t cSrcString[] = {0x04, 0x0a, 0x1e, 0x08, 0x00, 0x55, 0x00, 0x73, 0x00, 0x65, 0x00, 0x72};

    StaticArray<uint8_t, 100> asn1Value;
    auto err = mCryptoProvider->ASN1DecodeOctetString(Array<uint8_t>(cSrcString, sizeof(cSrcString)), asn1Value);

    ASSERT_EQ(err, aos::ErrorEnum::eNone);

    std::vector<uint8_t>       actual       = {asn1Value.begin(), asn1Value.end()};
    const std::vector<uint8_t> expectedASN1 = {0x1e, 0x08, 0x00, 0x55, 0x00, 0x73, 0x00, 0x65, 0x00, 0x72};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

TEST_P(CryptoProviderTest, ASN1DecodeOID)
{
    const uint8_t cOID[] = {0x06, 0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x14, 0x02};

    StaticArray<uint8_t, 100> asn1Value;
    auto                      err = mCryptoProvider->ASN1DecodeOID(Array<uint8_t>(cOID, sizeof(cOID)), asn1Value);

    ASSERT_EQ(err, aos::ErrorEnum::eNone);

    std::vector<uint8_t>       actual       = {asn1Value.begin(), asn1Value.end()};
    const std::vector<uint8_t> expectedASN1 = {0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x14, 0x02};

    EXPECT_THAT(actual, ElementsAreArray(expectedASN1));
}

/**
 * Python script to verify the implementation:
 *
 * import uuid
 * uuid.uuid5(uuid.UUID('58ac9ca0-2086-4683-a1b8-ec4bc08e01b6'), 'uid=42')
 */
TEST_P(CryptoProviderTest, CreateUUIDv5)
{
    uuid::UUID space;
    Error      err;
    Tie(space, err) = uuid::StringToUUID("58ac9ca0-2086-4683-a1b8-ec4bc08e01b6");
    ASSERT_TRUE(err.IsNone());

    uuid::UUID sha1;

    Tie(sha1, err) = mCryptoProvider->CreateUUIDv5(space, String("uid=42").AsByteArray());
    ASSERT_TRUE(err.IsNone());
    EXPECT_EQ(uuid::UUIDToString(sha1), "31d10f2b-ae42-531d-a158-d9359245d171");
}

TEST_P(CryptoProviderTest, SHA256)
{
    struct {
        const char* mString;
        const char* mExpectedHash;
    } testCases[] = {
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"string to test has works", "559519b77fd7e43a34ad0d95b5cfda81572849ab40f665165256cb52b5576150"},
        {"12345678901234567890123456", "5adcb5971681274f04187f2ebb0d69e09df67c8fc23ea13ee7b09c3d59ff5582"},
    };

    for (const auto& testCase : testCases) {
        auto [hasherPtr, err] = mHashProvider->CreateHash(HashEnum::eSHA256);
        LOG_ERR() << err;

        ASSERT_TRUE(err.IsNone());
        ASSERT_NE(hasherPtr.Get(), nullptr);

        const Array<uint8_t> data(reinterpret_cast<const uint8_t*>(testCase.mString), strlen(testCase.mString));

        ASSERT_TRUE(hasherPtr->Update(data).IsNone());

        StaticArray<uint8_t, cSHA256Size> result;

        ASSERT_TRUE(hasherPtr->Finalize(result).IsNone());

        StaticString<cSHA256HexLen> hashStr;
        ASSERT_TRUE(hashStr.ByteArrayToHex(result).IsNone());

        EXPECT_EQ(hashStr, String(testCase.mExpectedHash));
        LOG_DBG() << "SHA256: " << hashStr;
    }
}

TEST_P(CryptoProviderTest, SHA3_256)
{
    struct {
        const char* mString;
        const char* mExpectedHash;
    } testCases[] = {
        {"", "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"},
        {"abc", "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532"},
        {"string to test hash works", "220941491180a0e859654930be610f7ddf2c9e7307c7127f2bc1eb440b6ebfaf"},
        {"12345678901234567890123456", "d40b0546ca03c77f13cf28ef7c547aeea41fd6ae272bdfb3007eab5ce23f8aa7"},
    };

    for (const auto& testCase : testCases) {
        auto [hasherPtr, err] = mHashProvider->CreateHash(HashEnum::eSHA3_256);

        ASSERT_TRUE(err.IsNone());
        ASSERT_NE(hasherPtr.Get(), nullptr);

        const Array<uint8_t> data(reinterpret_cast<const uint8_t*>(testCase.mString), strlen(testCase.mString));

        ASSERT_TRUE(hasherPtr->Update(data).IsNone());

        StaticArray<uint8_t, cSHA256Size> result;

        ASSERT_TRUE(hasherPtr->Finalize(result).IsNone());

        StaticString<cSHA256HexLen> hashStr;
        ASSERT_TRUE(hashStr.ByteArrayToHex(result).IsNone());

        EXPECT_EQ(hashStr, String(testCase.mExpectedHash));
        LOG_DBG() << "SHA3_256: " << hashStr;
    }
}

TEST_P(CryptoProviderTest, SHA256ByChunks)
{
    auto [hasherPtr, err] = mHashProvider->CreateHash(HashEnum::eSHA256);
    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(hasherPtr);

    const char* chunks[] = {
        "",
        "abc",
        "string to test has works",
    };

    for (const auto& chunk : chunks) {
        const Array<uint8_t> data(reinterpret_cast<const uint8_t*>(chunk), strlen(chunk));

        ASSERT_TRUE(hasherPtr->Update(data).IsNone());
    }

    StaticArray<uint8_t, cSHA256Size> result;

    ASSERT_TRUE(hasherPtr->Finalize(result).IsNone());

    aos::StaticString<cSHA256HexLen> hashStr;
    ASSERT_TRUE(hashStr.ByteArrayToHex(result).IsNone());

    EXPECT_EQ(hashStr, String("a98c0eb748fcf3c87b8d231c0866f20dd12202923de5e93696ee4a3ad3da91ec"));

    LOG_DBG() << "SHA256: " << hashStr;
}

TEST_P(CryptoProviderTest, RandInt)
{
    constexpr uint64_t cMaxValue           = 100;
    constexpr int      cNumSamples         = 105;
    constexpr int      cMinUnique          = 40; // Expect ratio of unique values
    constexpr int      cMaxConsecutiveSame = 4; // Max allowed consecutive identical values
    constexpr int      cMaxFirstZeros      = 2; // Max allowed consecutive zeros at start

    std::set<uint64_t> uniqueValues;
    int                maxConsecutiveSeen = 1;
    int                consecutiveCount   = 1;
    uint64_t           previousValue      = 0;
    int                firstZeroCount     = 0;
    bool               foundNonZero       = false;

    // Track unique values
    auto trackUniqueValues = [&](uint64_t value) { uniqueValues.insert(value); };

    // Track consecutive identical values
    auto trackConsecutiveValues = [&](uint64_t value, int index) {
        if (index > 0 && value == previousValue) {
            consecutiveCount++;
            maxConsecutiveSeen = std::max(maxConsecutiveSeen, consecutiveCount);
        } else {
            consecutiveCount = 1;
        }
        previousValue = value;
    };

    // Track consecutive zeros at the beginning
    auto trackFirstZeros = [&](uint64_t value) {
        if (!foundNonZero) {
            if (value == 0) {
                firstZeroCount++;
            } else {
                foundNonZero = true;
            }
        }
    };

    // Generate random numbers and gather statistics
    for (int i = 0; i < cNumSamples; i++) {
        auto [value, err] = mRandomProvider->RandInt(cMaxValue);
        ASSERT_TRUE(err.IsNone());
        ASSERT_LT(value, cMaxValue);

        trackUniqueValues(value);
        trackConsecutiveValues(value, i);
        trackFirstZeros(value);
    }

    EXPECT_GE(uniqueValues.size(), cMinUnique);
    EXPECT_LT(maxConsecutiveSeen, cMaxConsecutiveSame);
    EXPECT_LT(firstZeroCount, cMaxFirstZeros);
}

TEST_P(CryptoProviderTest, RandBuffer)
{
    constexpr size_t                       kBufferSize = 16;
    aos::StaticArray<uint8_t, kBufferSize> buffer1;
    aos::StaticArray<uint8_t, kBufferSize> buffer2;

    ASSERT_EQ(mRandomProvider->RandBuffer(buffer1, kBufferSize), aos::ErrorEnum::eNone);
    ASSERT_EQ(mRandomProvider->RandBuffer(buffer2, kBufferSize), aos::ErrorEnum::eNone);

    ASSERT_EQ(buffer1.Size(), kBufferSize);
    ASSERT_EQ(buffer2.Size(), kBufferSize);

    ASSERT_NE(buffer1, buffer2);
}

TEST_P(CryptoProviderTest, GenerateRandomString)
{
    constexpr size_t             kSize = 4;
    aos::StaticString<kSize * 2> result1;
    aos::StaticString<kSize * 2> result2;

    ASSERT_EQ(GenerateRandomString<kSize>(result1, *mRandomProvider), aos::ErrorEnum::eNone);
    ASSERT_EQ(GenerateRandomString<kSize>(result2, *mRandomProvider), aos::ErrorEnum::eNone);

    ASSERT_FALSE(result1.IsEmpty());
    ASSERT_FALSE(result2.IsEmpty());
    ASSERT_EQ(result1.Size(), kSize * 2);
    ASSERT_EQ(result2.Size(), kSize * 2);
    ASSERT_NE(result1, result2);

    for (const auto& c : result1) {
        ASSERT_TRUE(isxdigit(c));
    }
    for (const auto& c : result2) {
        ASSERT_TRUE(isxdigit(c));
    }
}

TEST_P(CryptoProviderTest, AES_CBC_Encryption)
{
    const uint8_t keyRaw[32] = {0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d,
        0x77, 0x81, 0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4};

    const uint8_t ivRaw[16]
        = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    const char* plainRaw = "TWO BLOCK AES RAW MESSAGE"; // length = 26

    const Array<uint8_t> key(const_cast<uint8_t*>(keyRaw), sizeof(keyRaw));
    const Array<uint8_t> iv(const_cast<uint8_t*>(ivRaw), sizeof(ivRaw));
    const Array<uint8_t> plain(reinterpret_cast<const uint8_t*>(plainRaw), strlen(plainRaw));

    auto [cipherPtr, err] = mCryptoProvider->CreateAESEncoder("CBC", key, iv);
    ASSERT_TRUE(err.IsNone());
    ASSERT_NE(cipherPtr.Get(), nullptr);

    auto                     inBuf    = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();
    auto                     outBuf   = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();
    auto                     finalBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();
    StaticArray<uint8_t, 64> ciphertext;

    size_t offset = 0;
    while (offset < plain.Size()) {
        size_t chunk = std::min<size_t>(16, plain.Size() - offset);

        *inBuf = Array<uint8_t>(plain.Get() + offset, chunk);

        ASSERT_TRUE(cipherPtr->EncryptBlock(*inBuf, *outBuf).IsNone());

        ciphertext.Append(*outBuf);
        offset += chunk;
    }

    ASSERT_TRUE(cipherPtr->Finalize(*finalBuf).IsNone());
    ciphertext.Append(*finalBuf);

    const uint8_t expectedCiphertext[]
        = {0x01, 0x9e, 0x49, 0x04, 0x91, 0x6a, 0x71, 0x84, 0x72, 0xdf, 0xf5, 0x8a, 0x94, 0x2a, 0x18, 0xa7, 0x11, 0xe1,
            0x0d, 0x65, 0x00, 0x9b, 0x86, 0x03, 0x2f, 0xc2, 0x97, 0xcd, 0xab, 0xc2, 0x8b, 0xed};
    auto expected = Array<uint8_t>(const_cast<uint8_t*>(expectedCiphertext), sizeof(expectedCiphertext));

    EXPECT_EQ(ciphertext, expected);
}

TEST_P(CryptoProviderTest, AES_CBC_Decryption)
{
    const uint8_t keyRaw[32] = {0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d,
        0x77, 0x81, 0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4};

    const uint8_t ivRaw[16]
        = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    const uint8_t ciphertextRaw[] = {0x01, 0x9e, 0x49, 0x04, 0x91, 0x6a, 0x71, 0x84, 0x72, 0xdf, 0xf5, 0x8a, 0x94, 0x2a,
        0x18, 0xa7, 0x11, 0xe1, 0x0d, 0x65, 0x00, 0x9b, 0x86, 0x03, 0x2f, 0xc2, 0x97, 0xcd, 0xab, 0xc2, 0x8b, 0xed};

    Array<uint8_t> key(const_cast<uint8_t*>(keyRaw), sizeof(keyRaw));
    Array<uint8_t> iv(const_cast<uint8_t*>(ivRaw), sizeof(ivRaw));
    Array<uint8_t> ciphertext(const_cast<uint8_t*>(ciphertextRaw), sizeof(ciphertextRaw));

    auto [cipherPtr, err] = mCryptoProvider->CreateAESDecoder("CBC", key, iv);
    ASSERT_TRUE(err.IsNone());
    ASSERT_NE(cipherPtr.Get(), nullptr);

    auto                     inBuf    = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();
    auto                     outBuf   = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();
    auto                     finalBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();
    StaticArray<uint8_t, 64> plaintext;

    size_t offset = 0;
    while (offset < ciphertext.Size()) {
        *inBuf = Array<uint8_t>(ciphertext.Get() + offset, 16);

        ASSERT_TRUE(cipherPtr->DecryptBlock(*inBuf, *outBuf).IsNone());
        plaintext.Append(*outBuf);
        offset += 16;
    }

    ASSERT_TRUE(cipherPtr->Finalize(*finalBuf).IsNone());
    plaintext.Append(*finalBuf);

    const char*    expectedRaw = "TWO BLOCK AES RAW MESSAGE";
    Array<uint8_t> expected(reinterpret_cast<const uint8_t*>(expectedRaw), strlen(expectedRaw));

    EXPECT_EQ(plaintext, expected);
}

namespace {
using testvectors::cGCMCipher;
using testvectors::cGCMIV;
using testvectors::cGCMKey;
using testvectors::cGCMPlain;
using testvectors::cGCMTag;

template <size_t size>
Array<uint8_t> AsArray(const uint8_t (&data)[size])
{
    return Array<uint8_t>(data, size);
}

} // namespace

TEST_P(CryptoProviderTest, AES_GCM_Encryption)
{
    auto [cipher, err] = mCryptoProvider->CreateAESEncoder("GCM", AsArray(cGCMKey), AsArray(cGCMIV));
    ASSERT_TRUE(err.IsNone());
    ASSERT_NE(cipher.Get(), nullptr);

    auto                     outBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();
    StaticArray<uint8_t, 64> ciphertext;

    // GCM is a stream mode: any chunk size is accepted
    constexpr size_t cChunk = 7;

    for (size_t offset = 0; offset < sizeof(cGCMPlain); offset += cChunk) {
        const auto chunk = std::min(cChunk, sizeof(cGCMPlain) - offset);

        ASSERT_TRUE(cipher->EncryptBlock(Array<uint8_t>(cGCMPlain + offset, chunk), *outBuf).IsNone());
        EXPECT_EQ(outBuf->Size(), chunk);

        ciphertext.Append(*outBuf);
    }

    ASSERT_TRUE(cipher->Finalize(*outBuf).IsNone());
    EXPECT_TRUE(outBuf->IsEmpty());

    EXPECT_EQ(ciphertext, AsArray(cGCMCipher));

    StaticArray<uint8_t, AESCipherItf::cGCMTagSize> tag;

    ASSERT_TRUE(cipher->GetTag(tag).IsNone());
    EXPECT_EQ(tag, AsArray(cGCMTag));
}

TEST_P(CryptoProviderTest, AES_GCM_Decryption)
{
    auto [cipher, err] = mCryptoProvider->CreateAESDecoder("GCM", AsArray(cGCMKey), AsArray(cGCMIV));
    ASSERT_TRUE(err.IsNone());
    ASSERT_NE(cipher.Get(), nullptr);

    auto                     outBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();
    StaticArray<uint8_t, 64> plaintext;

    constexpr size_t cChunk = 13;

    for (size_t offset = 0; offset < sizeof(cGCMCipher); offset += cChunk) {
        const auto chunk = std::min(cChunk, sizeof(cGCMCipher) - offset);

        ASSERT_TRUE(cipher->DecryptBlock(Array<uint8_t>(cGCMCipher + offset, chunk), *outBuf).IsNone());

        plaintext.Append(*outBuf);
    }

    ASSERT_TRUE(cipher->SetTag(AsArray(cGCMTag)).IsNone());
    ASSERT_TRUE(cipher->Finalize(*outBuf).IsNone());
    EXPECT_TRUE(outBuf->IsEmpty());

    EXPECT_EQ(plaintext, AsArray(cGCMPlain));
}

TEST_P(CryptoProviderTest, AES_GCM_DecryptionDetectsTamperedCiphertext)
{
    uint8_t tampered[sizeof(cGCMCipher)];
    memcpy(tampered, cGCMCipher, sizeof(tampered));
    tampered[10] ^= 0x01;

    auto [cipher, err] = mCryptoProvider->CreateAESDecoder("GCM", AsArray(cGCMKey), AsArray(cGCMIV));
    ASSERT_TRUE(err.IsNone());

    auto outBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();

    ASSERT_TRUE(cipher->DecryptBlock(AsArray(tampered), *outBuf).IsNone());
    ASSERT_TRUE(cipher->SetTag(AsArray(cGCMTag)).IsNone());

    EXPECT_FALSE(cipher->Finalize(*outBuf).IsNone());
}

TEST_P(CryptoProviderTest, AES_GCM_DecryptionDetectsWrongTagAndKey)
{
    auto outBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();

    {
        uint8_t tampered[sizeof(cGCMTag)];
        memcpy(tampered, cGCMTag, sizeof(tampered));
        tampered[15] ^= 0x80;

        auto [cipher, err] = mCryptoProvider->CreateAESDecoder("GCM", AsArray(cGCMKey), AsArray(cGCMIV));
        ASSERT_TRUE(err.IsNone());

        ASSERT_TRUE(cipher->DecryptBlock(AsArray(cGCMCipher), *outBuf).IsNone());
        ASSERT_TRUE(cipher->SetTag(AsArray(tampered)).IsNone());

        EXPECT_FALSE(cipher->Finalize(*outBuf).IsNone());
    }

    {
        uint8_t otherKey[sizeof(cGCMKey)];
        memcpy(otherKey, cGCMKey, sizeof(otherKey));
        otherKey[0] ^= 0x01;

        auto [cipher, err] = mCryptoProvider->CreateAESDecoder("GCM", AsArray(otherKey), AsArray(cGCMIV));
        ASSERT_TRUE(err.IsNone());

        ASSERT_TRUE(cipher->DecryptBlock(AsArray(cGCMCipher), *outBuf).IsNone());
        ASSERT_TRUE(cipher->SetTag(AsArray(cGCMTag)).IsNone());

        EXPECT_FALSE(cipher->Finalize(*outBuf).IsNone());
    }
}

TEST_P(CryptoProviderTest, AES_GCM_TagHandling)
{
    auto outBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();

    // a decoder cannot finish without the tag
    {
        auto [cipher, err] = mCryptoProvider->CreateAESDecoder("GCM", AsArray(cGCMKey), AsArray(cGCMIV));
        ASSERT_TRUE(err.IsNone());

        ASSERT_TRUE(cipher->DecryptBlock(AsArray(cGCMCipher), *outBuf).IsNone());

        EXPECT_FALSE(cipher->Finalize(*outBuf).IsNone());
    }

    // the tag must have the full size
    {
        auto [cipher, err] = mCryptoProvider->CreateAESDecoder("GCM", AsArray(cGCMKey), AsArray(cGCMIV));
        ASSERT_TRUE(err.IsNone());

        EXPECT_FALSE(cipher->SetTag(Array<uint8_t>(cGCMTag, 8)).IsNone());
    }

    // an encoder has no tag before it is finalized and cannot be given one
    {
        auto [cipher, err] = mCryptoProvider->CreateAESEncoder("GCM", AsArray(cGCMKey), AsArray(cGCMIV));
        ASSERT_TRUE(err.IsNone());

        StaticArray<uint8_t, AESCipherItf::cGCMTagSize> tag;

        EXPECT_FALSE(cipher->GetTag(tag).IsNone());
        EXPECT_FALSE(cipher->SetTag(AsArray(cGCMTag)).IsNone());
    }
}

TEST_P(CryptoProviderTest, AES_GCM_RequiresGCMSizedIV)
{
    const uint8_t iv16[16] = {};

    EXPECT_FALSE(mCryptoProvider->CreateAESEncoder("GCM", AsArray(cGCMKey), AsArray(iv16)).mError.IsNone());
    EXPECT_FALSE(mCryptoProvider->CreateAESDecoder("GCM", AsArray(cGCMKey), AsArray(iv16)).mError.IsNone());
}

TEST_P(CryptoProviderTest, AES_CBC_TagAccessorsAreNotSupported)
{
    const uint8_t iv16[16] = {};

    auto [cipher, err] = mCryptoProvider->CreateAESEncoder("CBC", AsArray(cGCMKey), AsArray(iv16));
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, AESCipherItf::cGCMTagSize> tag;

    EXPECT_FALSE(cipher->GetTag(tag).IsNone());
    EXPECT_FALSE(cipher->SetTag(AsArray(cGCMTag)).IsNone());
}

TEST_P(CryptoProviderTest, AES_UnsupportedModeIsNotSupported)
{
    const uint8_t iv16[16] = {};

    EXPECT_TRUE(
        mCryptoProvider->CreateAESEncoder("CTR", AsArray(cGCMKey), AsArray(iv16)).mError.Is(ErrorEnum::eNotSupported));
    EXPECT_TRUE(
        mCryptoProvider->CreateAESDecoder("CTR", AsArray(cGCMKey), AsArray(iv16)).mError.Is(ErrorEnum::eNotSupported));
}

TEST_P(CryptoProviderTest, AES_KeySizes)
{
    const uint8_t key[33] = {};

    for (const auto* mode : {"CBC", "GCM"}) {
        SCOPED_TRACE(mode);

        const uint8_t iv[16] = {};
        const auto    ivSize = String(mode) == "GCM" ? AESCipherItf::cGCMIVSize : AESCipherItf::cBlockSize;

        for (const size_t keySize : {16, 24, 32}) {
            SCOPED_TRACE(keySize);

            EXPECT_TRUE(
                mCryptoProvider->CreateAESEncoder(mode, Array<uint8_t>(key, keySize), Array<uint8_t>(iv, ivSize))
                    .mError.IsNone());
            EXPECT_TRUE(
                mCryptoProvider->CreateAESDecoder(mode, Array<uint8_t>(key, keySize), Array<uint8_t>(iv, ivSize))
                    .mError.IsNone());
        }

        for (const size_t keySize : {0, 15, 20, 33}) {
            SCOPED_TRACE(keySize);

            EXPECT_TRUE(
                mCryptoProvider->CreateAESEncoder(mode, Array<uint8_t>(key, keySize), Array<uint8_t>(iv, ivSize))
                    .mError.Is(ErrorEnum::eInvalidArgument));
            EXPECT_TRUE(
                mCryptoProvider->CreateAESDecoder(mode, Array<uint8_t>(key, keySize), Array<uint8_t>(iv, ivSize))
                    .mError.Is(ErrorEnum::eInvalidArgument));
        }
    }
}

TEST_P(CryptoProviderTest, AES_CipherStateIsChecked)
{
    const uint8_t iv[16] = {};
    const auto    input  = Array<uint8_t>(cGCMPlain, AESCipherItf::cBlockSize);

    auto outBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();

    for (const auto* mode : {"CBC", "GCM"}) {
        SCOPED_TRACE(mode);

        const auto ivSize = String(mode) == "GCM" ? AESCipherItf::cGCMIVSize : AESCipherItf::cBlockSize;

        auto [encoder, encErr] = mCryptoProvider->CreateAESEncoder(mode, AsArray(cGCMKey), Array<uint8_t>(iv, ivSize));
        ASSERT_TRUE(encErr.IsNone());

        auto [decoder, decErr] = mCryptoProvider->CreateAESDecoder(mode, AsArray(cGCMKey), Array<uint8_t>(iv, ivSize));
        ASSERT_TRUE(decErr.IsNone());

        // wrong direction
        EXPECT_TRUE(encoder->DecryptBlock(input, *outBuf).Is(ErrorEnum::eWrongState));
        EXPECT_TRUE(decoder->EncryptBlock(input, *outBuf).Is(ErrorEnum::eWrongState));

        // nothing to encrypt
        EXPECT_TRUE(encoder->EncryptBlock(Array<uint8_t>(), *outBuf).Is(ErrorEnum::eInvalidArgument));

        // the tag is not available on the wrong side
        StaticArray<uint8_t, AESCipherItf::cGCMTagSize> tag;

        EXPECT_FALSE(decoder->GetTag(tag).IsNone());

        // a finalized cipher cannot be used any more
        ASSERT_TRUE(encoder->EncryptBlock(input, *outBuf).IsNone());
        ASSERT_TRUE(encoder->Finalize(*outBuf).IsNone());

        EXPECT_TRUE(encoder->EncryptBlock(input, *outBuf).Is(ErrorEnum::eWrongState));
        EXPECT_TRUE(encoder->Finalize(*outBuf).Is(ErrorEnum::eWrongState));

        ASSERT_TRUE(decoder->DecryptBlock(input, *outBuf).IsNone());

        if (String(mode) == "GCM") {
            ASSERT_TRUE(decoder->SetTag(AsArray(cGCMTag)).IsNone());
        }

        (void)decoder->Finalize(*outBuf);

        EXPECT_TRUE(decoder->DecryptBlock(input, *outBuf).Is(ErrorEnum::eWrongState));
        EXPECT_TRUE(decoder->Finalize(*outBuf).Is(ErrorEnum::eWrongState));

        if (String(mode) == "GCM") {
            EXPECT_TRUE(decoder->SetTag(AsArray(cGCMTag)).Is(ErrorEnum::eWrongState));
        }
    }
}

TEST_P(CryptoProviderTest, AES_CreateFailsWithoutMemory)
{
    const uint8_t iv[16] = {};

    for (const auto* mode : {"CBC", "GCM"}) {
        SCOPED_TRACE(mode);

        const auto ivSize = String(mode) == "GCM" ? AESCipherItf::cGCMIVSize : AESCipherItf::cBlockSize;

        mAllocator.FailAll();

        EXPECT_TRUE(mCryptoProvider->CreateAESEncoder(mode, AsArray(cGCMKey), Array<uint8_t>(iv, ivSize))
                        .mError.Is(ErrorEnum::eNoMemory));
        EXPECT_TRUE(mCryptoProvider->CreateAESDecoder(mode, AsArray(cGCMKey), Array<uint8_t>(iv, ivSize))
                        .mError.Is(ErrorEnum::eNoMemory));

        mAllocator.Reset();

        // the provider is still usable once the allocator recovers
        EXPECT_TRUE(
            mCryptoProvider->CreateAESEncoder(mode, AsArray(cGCMKey), Array<uint8_t>(iv, ivSize)).mError.IsNone());
    }
}

TEST_P(CryptoProviderTest, AES_CBC_InvalidInput)
{
    const uint8_t iv[16] = {};

    auto outBuf = std::make_unique<StaticArray<uint8_t, cFileChunkSize>>();

    // input is not a multiple of the block size
    {
        auto [decoder, err] = mCryptoProvider->CreateAESDecoder("CBC", AsArray(cGCMKey), AsArray(iv));
        ASSERT_TRUE(err.IsNone());

        EXPECT_TRUE(decoder->DecryptBlock(Array<uint8_t>(cGCMCipher, AESCipherItf::cBlockSize + 1), *outBuf)
                        .Is(ErrorEnum::eInvalidArgument));
    }

    // corrupted padding
    {
        auto [decoder, err] = mCryptoProvider->CreateAESDecoder("CBC", AsArray(cGCMKey), AsArray(iv));
        ASSERT_TRUE(err.IsNone());

        ASSERT_TRUE(decoder->DecryptBlock(Array<uint8_t>(cGCMCipher, AESCipherItf::cBlockSize), *outBuf).IsNone());

        EXPECT_FALSE(decoder->Finalize(*outBuf).IsNone());
    }
}

TEST_P(CryptoProviderTest, VerifyRSASignature)
{
    StaticArray<x509::Certificate, 1> certs;

    // Generate certificate
    std::shared_ptr<PrivateKeyItf> privKey;
    CreateCertificate(*mFactory, *mCryptoProvider, "CN=Test Subject, O=Org, C=GB", KeyTypeEnum::eRSA, privKey, certs);

    // Generate digest
    const char messageRaw[] = "Hello world";
    const auto message      = Array<uint8_t>(reinterpret_cast<const uint8_t*>(messageRaw), sizeof(messageRaw));

    auto [hasher, err] = mHashProvider->CreateHash(HashEnum::eSHA256);
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, cSHA256Size> digest;

    ASSERT_TRUE(hasher->Update(message).IsNone());
    ASSERT_TRUE(hasher->Finalize(digest).IsNone());

    StaticArray<uint8_t, cSignatureSize> signature;

    ASSERT_TRUE(privKey->Sign(digest, {HashEnum::eSHA256}, signature).IsNone());

    Variant<ECDSAPublicKey, RSAPublicKey> pubKey;
    pubKey.SetValue(static_cast<const RSAPublicKey&>(privKey->GetPublic()));

    EXPECT_TRUE(
        mCryptoProvider->Verify(pubKey, HashEnum::eSHA256, x509::PaddingEnum::ePKCS1v1_5, digest, signature).IsNone());
}

TEST_P(CryptoProviderTest, VerifyECDSASignature)
{
    StaticArray<x509::Certificate, 1> certs;

    // Generate certificate
    std::shared_ptr<PrivateKeyItf> privKey;
    CreateCertificate(*mFactory, *mCryptoProvider, "CN=Test Subject, O=Org, C=GB", KeyTypeEnum::eECDSA, privKey, certs);

    // Generate digest
    const char messageRaw[] = "Hello world";
    const auto message      = Array<uint8_t>(reinterpret_cast<const uint8_t*>(messageRaw), sizeof(messageRaw));

    auto [hasher, err] = mHashProvider->CreateHash(HashEnum::eSHA384);
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, cSHA384Size> digest;

    ASSERT_TRUE(hasher->Update(message).IsNone());
    ASSERT_TRUE(hasher->Finalize(digest).IsNone());

    StaticArray<uint8_t, cSignatureSize> signature;

    ASSERT_TRUE(privKey->Sign(digest, {HashEnum::eSHA384}, signature).IsNone());

    Variant<ECDSAPublicKey, RSAPublicKey> pubKey;
    pubKey.SetValue(static_cast<const ECDSAPublicKey&>(privKey->GetPublic()));

    EXPECT_TRUE(
        mCryptoProvider->Verify(pubKey, HashEnum::eSHA384, x509::PaddingEnum::eNone, digest, signature).IsNone());
}

TEST_P(CryptoProviderTest, VerifyCACert)
{
    StaticString<cCertPEMLen>         buff;
    StaticArray<x509::Certificate, 1> certs;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/ca.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, certs).IsNone());
    ASSERT_EQ(certs.Size(), 1u);

    const auto& cert = certs[0];

    EXPECT_EQ(cert.mVersion, x509::cX509Version3);
    EXPECT_TRUE(cert.mIsCA);
    ASSERT_TRUE(cert.mKeyUsage.HasValue());
    EXPECT_NE(cert.mKeyUsage.GetValue() & x509::keyusage::cKeyCertSign, 0u);
    EXPECT_NE(cert.mKeyUsage.GetValue() & x509::keyusage::cCRLSign, 0u);
    EXPECT_FALSE(cert.mSubjectKeyID.IsEmpty());
    EXPECT_EQ(cert.mIssuer, cert.mSubject);
    ASSERT_TRUE(ValidateCACert(cert).IsNone());
}

TEST_P(CryptoProviderTest, VerifyLeafCert)
{
    StaticString<cCertPEMLen>         buff;
    StaticArray<x509::Certificate, 1> certs;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, certs).IsNone());
    ASSERT_EQ(certs.Size(), 1u);

    const auto& cert = certs[0];

    EXPECT_EQ(cert.mVersion, x509::cX509Version3);
    EXPECT_FALSE(cert.mIsCA);
    ASSERT_TRUE(cert.mKeyUsage.HasValue());
    EXPECT_EQ(cert.mKeyUsage.GetValue() & x509::keyusage::cKeyCertSign, 0u);
    EXPECT_EQ(cert.mKeyUsage.GetValue() & x509::keyusage::cCRLSign, 0u);
    EXPECT_FALSE(ValidateCACert(cert).IsNone());
}

TEST_P(CryptoProviderTest, VerifyCertChain)
{
    StaticString<cCertPEMLen>         buff;
    StaticArray<x509::Certificate, 1> rootCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/ca.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, rootCerts).IsNone());

    StaticArray<x509::Certificate, 1> intermCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client_int.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, intermCerts).IsNone());

    StaticArray<x509::Certificate, 1> leafCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, leafCerts).IsNone());

    x509::VerifyOptions opts;
    opts.mCurrentTime = Time();

    ASSERT_TRUE(mCryptoProvider->Verify(rootCerts, intermCerts, opts, leafCerts[0]).IsNone());
}

TEST_P(CryptoProviderTest, VerifyCertChainCurTimeExceeds)
{
    StaticString<cCertPEMLen>         buff;
    StaticArray<x509::Certificate, 1> rootCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/ca.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, rootCerts).IsNone());

    StaticArray<x509::Certificate, 1> intermCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client_int.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, intermCerts).IsNone());

    StaticArray<x509::Certificate, 1> leafCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, leafCerts).IsNone());

    x509::VerifyOptions opts;
    opts.mCurrentTime = Time::Now().Add(Years(2));

    ASSERT_FALSE(mCryptoProvider->Verify(rootCerts, intermCerts, opts, leafCerts[0]).IsNone());
}

/***********************************************************************************************************************
 * Tests for ASN1Parser interface
 **********************************************************************************************************************/

class MockASN1Reader : public asn1::ASN1ReaderItf {
public:
    MOCK_METHOD(Error, OnASN1Element, (const asn1::ASN1Value& value), (override));
};

/**
 * Python script to verify the implementation:
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.enter(asn1.Numbers.Sequence)  # Start SEQUENCE
 * enc.write(5)  # INTEGER with value 5
 * enc.leave()
 *
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */
TEST_P(CryptoProviderTest, ReadStruct)
{
    // DER encoding of:
    // SEQUENCE(0x30 0x03) {
    //   INTEGER 42(0x02 0x01 0x2A)
    // }
    const uint8_t derData[] = {0x30, 0x03, 0x02, 0x01, 0x2A};
    auto          data      = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));

    MockASN1Reader mockReader;

    StaticArray<uint8_t, 3> structContent;
    structContent.PushBack(0x02);
    structContent.PushBack(0x01);
    structContent.PushBack(0x2A);

    EXPECT_CALL(mockReader,
        OnASN1Element(asn1::ASN1Value {0 /*UNIVERSAL*/, 16 /*SEQUENCE*/, true /* CONSTRUCTED */, structContent}))
        .WillOnce(Return(ErrorEnum::eNone));

    auto result = mCryptoProvider->ReadStruct(data, {}, mockReader);

    EXPECT_TRUE(result.mError.IsNone());
    EXPECT_TRUE(result.mRemaining.IsEmpty());
}

/**
 * Python script to verify the implementation for SET:
 *
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.enter(asn1.Numbers.Set)  # Start SET
 * enc.write(10)  # INTEGER 10
 * enc.write(20)  # INTEGER 20
 * enc.leave()
 *
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */
TEST_P(CryptoProviderTest, ReadSet)
{
    // DER encoding of:
    // SET(0x31 0x06) {
    //   INTEGER 10(0x02 0x01 0x0A)
    //   INTEGER 20(0x02 0x01 0x14)
    // }
    const uint8_t derData[] = {0x31, 0x06, 0x02, 0x01, 0x0A, 0x02, 0x01, 0x14};
    auto          data      = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));

    MockASN1Reader mockReader;

    StaticArray<uint8_t, 3> item1;
    item1.PushBack(0x0A); // INTEGER value 10

    StaticArray<uint8_t, 3> item2;
    item2.PushBack(0x14); // INTEGER value 20

    EXPECT_CALL(mockReader,
        OnASN1Element(asn1::ASN1Value {
            0 /* UNIVERSAL */, 2 /* INTEGER */, false /* PRIMITIVE */, Array<uint8_t>(item1.Get(), item1.Size())}))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mockReader,
        OnASN1Element(asn1::ASN1Value {
            0 /* UNIVERSAL */, 2 /* INTEGER */, false /* PRIMITIVE */, Array<uint8_t>(item2.Get(), item2.Size())}))
        .WillOnce(Return(ErrorEnum::eNone));

    auto result = mCryptoProvider->ReadSet(data, {}, mockReader);

    EXPECT_TRUE(result.mError.IsNone());
    EXPECT_TRUE(result.mRemaining.IsEmpty());
}

/**
 * Test python script:
 *
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.enter(asn1.Numbers.Sequence)  # Start SEQUENCE
 * enc.write(15)  # INTEGER 15
 * enc.write(25)  # INTEGER 25
 * enc.leave()
 *
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */
TEST_P(CryptoProviderTest, ReadSequence)
{
    // DER encoding of:
    // SEQUENCE(0x30 0x06) {
    //   INTEGER 15(0x02 0x01 0x0F)
    //   INTEGER 25(0x02 0x01 0x19)
    // }
    const uint8_t derData[] = {0x30, 0x06, 0x02, 0x01, 0x0F, 0x02, 0x01, 0x19};
    auto          data      = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));

    MockASN1Reader mockReader;

    StaticArray<uint8_t, 1> item1;
    item1.PushBack(0x0F); // INTEGER value 15

    StaticArray<uint8_t, 1> item2;
    item2.PushBack(0x19); // INTEGER value 25

    EXPECT_CALL(mockReader,
        OnASN1Element(asn1::ASN1Value {
            0 /* UNIVERSAL */, 2 /* INTEGER */, false /* PRIMITIVE */, Array<uint8_t>(item1.Get(), item1.Size())}))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mockReader,
        OnASN1Element(asn1::ASN1Value {
            0 /* UNIVERSAL */, 2 /* INTEGER */, false /* PRIMITIVE */, Array<uint8_t>(item2.Get(), item2.Size())}))
        .WillOnce(Return(ErrorEnum::eNone));

    auto result = mCryptoProvider->ReadSequence(data, {}, mockReader);

    EXPECT_TRUE(result.mError.IsNone());
    EXPECT_TRUE(result.mRemaining.IsEmpty());
}

/**
 * Test python script:
 *
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * enc.write(12345)  # INTEGER with value 12345
 * encoded_bytes = enc.output()
 *
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */
TEST_P(CryptoProviderTest, ReadInteger)
{
    // DER encoding of INTEGER 12345 (0x30 0x39):
    // Tag: 0x02 (INTEGER), Length: 0x02, Value: 0x30 0x39
    const uint8_t          derData[] = {0x02, 0x02, 0x30, 0x39};
    auto                   data      = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));
    asn1::ASN1ParseOptions opts {};

    int value = 0;

    auto result = mCryptoProvider->ReadInteger(data, opts, value);

    EXPECT_EQ(value, 12345);

    EXPECT_TRUE(result.mError.IsNone());
    EXPECT_TRUE(result.mRemaining.IsEmpty());
}

/**
 * Test python script:
 *
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * # Big INTEGER value (hex): 0x0102030405060708090a
 * big_int_bytes = bytes.fromhex("0102030405060708090a")
 * enc.write(big_int_bytes, asn1.Numbers.Integer)
 *
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */
TEST_P(CryptoProviderTest, ReadBigInt)
{
    // DER encoding of INTEGER: 0x0102030405060708090a
    // Tag: 0x02 (INTEGER), Length: 0x0A, Value: 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A
    const uint8_t derData[] = {0x02, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    auto          data      = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));

    StaticArray<uint8_t, 10> result;

    auto parseResult = mCryptoProvider->ReadBigInt(data, {}, result);

    const uint8_t derExpected[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    auto          expected      = Array<uint8_t>(const_cast<uint8_t*>(derExpected), sizeof(derExpected));

    EXPECT_EQ(result, expected);

    EXPECT_TRUE(parseResult.mError.IsNone());
    EXPECT_TRUE(parseResult.mRemaining.IsEmpty());
}

/**
 * Test python script:
 *
 * import asn1
 * enc = asn1.Encoder()
 * enc.start()
 * enc.write('1.2.840.113549', asn1.Numbers.ObjectIdentifier)
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */
TEST_P(CryptoProviderTest, ReadOID)
{
    // DER encoding of OBJECT IDENTIFIER 1.2.840.113549
    const uint8_t derData[] = {0x06, 0x06, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d};
    auto          data      = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));

    asn1::ObjectIdentifier oid;

    auto parseResult = mCryptoProvider->ReadOID(data, {}, oid);

    const String expected = "1.2.840.113549";

    EXPECT_EQ(oid, expected) << "actual:" << oid.CStr() << ", expected: " << expected.CStr();

    EXPECT_TRUE(parseResult.mError.IsNone());
    EXPECT_TRUE(parseResult.mRemaining.IsEmpty());
}

/**
 * Test python script:
 *
 * import asn1

 * enc = asn1.Encoder()
 * enc.start()
 * enc.enter(asn1.Numbers.Sequence)
 *
 * enc.write('2.16.840.1.101.3.4.1.2', asn1.Numbers.ObjectIdentifier)
 *
 * iv_bytes = bytes.fromhex('00112233445566778899aabbccddeeff')
 * enc.write(iv_bytes, asn1.Numbers.OctetString)
 *
 * enc.leave()
 *
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */

TEST_P(CryptoProviderTest, ReadAID)
{
    // SEQUENCE (0x30 0x1D)
    //   OID (0x06 0x09) "2.16.840.1.101.3.4.1.2" (aes-128-CBC)
    //   OCTET STRING (0x04 0x10) IV bytes (16 bytes)
    const uint8_t derData[] = {0x30, 0x1D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02, 0x04, 0x10,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    auto data = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));

    asn1::AlgorithmIdentifier aid;

    auto parseResult = mCryptoProvider->ReadAID(data, {}, aid);

    const String expectedOID = "2.16.840.1.101.3.4.1.2"; // OID for aes-128-cbc

    // Only the OCTET STRING content bytes, NOT including tag (0x04) or length (0x10)
    const uint8_t expectedParams[]
        = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    auto expectedParamsArr = Array<uint8_t>(const_cast<uint8_t*>(expectedParams), sizeof(expectedParams));

    EXPECT_EQ(aid.mOID, expectedOID) << "actual:" << aid.mOID.CStr() << ", expected: " << expectedOID.CStr();
    EXPECT_EQ(aid.mParams.mValue, expectedParamsArr);

    EXPECT_TRUE(parseResult.mError.IsNone());
    EXPECT_TRUE(parseResult.mRemaining.IsEmpty());
}

/**
 * Python script to generate the test data:
 *
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * # OCTET STRING containing 4 bytes: 0x01 0x02 0x03 0x04
 * enc.write(b'\x01\x02\x03\x04', asn1.Numbers.OctetString)
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */
TEST_P(CryptoProviderTest, ReadOctetString)
{
    // DER encoding of OCTET STRING 0x01 0x02 0x03 0x04:
    // Tag: 0x04 (OCTET STRING), Length: 0x04, Value: 0x01 0x02 0x03 0x04
    const uint8_t derData[] = {0x04, 0x04, 0x01, 0x02, 0x03, 0x04};
    auto          data      = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));

    StaticArray<uint8_t, 4> result;

    auto parseResult = mCryptoProvider->ReadOctetString(data, {}, result);

    const uint8_t expectedBytes[] = {0x01, 0x02, 0x03, 0x04};
    auto          expected        = Array<uint8_t>(const_cast<uint8_t*>(expectedBytes), sizeof(expectedBytes));

    EXPECT_EQ(result, expected);

    EXPECT_TRUE(parseResult.mError.IsNone());
    EXPECT_TRUE(parseResult.mRemaining.IsEmpty());
}

/**
 * Python script to generate the test data:
 *
 * import asn1
 *
 * enc = asn1.Encoder()
 * enc.start()
 * # Write INTEGER with value 42
 * enc.write(42)
 * encoded_bytes = enc.output()
 * print(", ".join(hex(b) for b in encoded_bytes))
 * print("".join('%02x' % b for b in encoded_bytes))
 */
TEST_P(CryptoProviderTest, ReadRawValue)
{
    // Tag: 0x02 (INTEGER), Length: 0x01, Value: 0x2A (42 decimal)
    const uint8_t derData[] = {0x02, 0x01, 0x2A};
    auto          data      = Array<uint8_t>(const_cast<uint8_t*>(derData), sizeof(derData));

    asn1::ASN1Value value;

    auto parseResult = mCryptoProvider->ReadRawValue(data, {}, value);

    const uint8_t expectedBytes[] = {0x2A};
    auto          expected        = Array<uint8_t>(const_cast<uint8_t*>(expectedBytes), sizeof(expectedBytes));

    EXPECT_EQ(value.mTagClass, 0 /* UNIVERSAL */);
    EXPECT_EQ(value.mTagNumber, 2 /* INTEGER */);
    EXPECT_EQ(value.mValue, expected);

    EXPECT_TRUE(parseResult.mError.IsNone());
    EXPECT_TRUE(parseResult.mRemaining.IsEmpty());
}

/***********************************************************************************************************************
 * Error handling and less common paths
 **********************************************************************************************************************/

namespace {
class FakePrivateKey : public PrivateKeyItf {
public:
    FakePrivateKey(const RSAPublicKey& pubKey, size_t signatureSize, const Error& signErr = ErrorEnum::eNone)
        : mSignatureSize(signatureSize)
        , mSignErr(signErr)
    {
        mPubKey.SetValue<RSAPublicKey>(pubKey);
    }

    FakePrivateKey(const ECDSAPublicKey& pubKey, size_t signatureSize, const Error& signErr = ErrorEnum::eNone)
        : mSignatureSize(signatureSize)
        , mSignErr(signErr)
    {
        mPubKey.SetValue<ECDSAPublicKey>(pubKey);
    }

    const PublicKeyItf& GetPublic() const override { return GetBase<PublicKeyItf>(mPubKey); }

    Error Sign(const Array<uint8_t>& digest, const SignOptions& options, Array<uint8_t>& signature) const override
    {
        (void)digest;
        (void)options;

        if (!mSignErr.IsNone()) {
            return mSignErr;
        }

        if (auto err = signature.Resize(mSignatureSize); !err.IsNone()) {
            return err;
        }

        for (auto& byte : signature) {
            byte = 0x5A;
        }

        return ErrorEnum::eNone;
    }

    Error Decrypt(const Array<uint8_t>&, const DecryptionOptions&, Array<uint8_t>&) const override
    {
        return ErrorEnum::eNotSupported;
    }

private:
    Variant<ECDSAPublicKey, RSAPublicKey> mPubKey;
    size_t                                mSignatureSize;
    Error                                 mSignErr;
};

std::vector<uint8_t> HexToBytes(const char* hex)
{
    std::vector<uint8_t> bytes;
    const std::string    str(hex);

    for (size_t i = 0; i + 1 < str.size(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(str.substr(i, 2), nullptr, 16)));
    }

    return bytes;
}

RSAPublicKey CreateFakeRSAPublicKey(size_t modulusSize)
{
    std::vector<uint8_t> modulus(modulusSize, 0xC5);
    const uint8_t        exponent[] = {0x01, 0x00, 0x01};

    modulus.back() = 0xC7;

    return RSAPublicKey(tests::utils::ConvertToArray(modulus), Array<uint8_t>(exponent, sizeof(exponent)));
}

template <size_t cSize>
void ReadTestFile(const std::string& path, StaticString<cSize>& content)
{
    ASSERT_TRUE(fs::ReadFileToString(path.c_str(), content).IsNone()) << "can't read file: " << path;
}

class ASN1ReaderStub : public asn1::ASN1ReaderItf {
public:
    explicit ASN1ReaderStub(const Error& err = ErrorEnum::eNone)
        : mErr(err)
    {
    }

    Error OnASN1Element(const asn1::ASN1Value& value) override
    {
        mValues.push_back(value.mTagNumber);

        return mErr;
    }

    std::vector<int32_t> mValues;

private:
    Error mErr;
};

} // namespace

TEST_P(CryptoProviderTest, X509CertToPEM)
{
    StaticString<cCertPEMLen>         pem;
    StaticArray<x509::Certificate, 1> certs;

    ReadTestFile(TEST_CERTIFICATES_DIR "/ca.cer", pem);
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(pem, certs).IsNone());

    StaticString<cCertPEMLen> result;

    // MbedTLS implementation writes into the current string size
    ASSERT_TRUE(result.Resize(result.MaxSize()).IsNone());
    ASSERT_TRUE(mCryptoProvider->X509CertToPEM(certs[0], result).IsNone());

    StaticArray<x509::Certificate, 1> parsed;

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(result, parsed).IsNone());
    ASSERT_EQ(parsed.Size(), 1u);
    EXPECT_EQ(parsed[0].mRaw, certs[0].mRaw);

    StaticString<16> small;

    ASSERT_TRUE(small.Resize(small.MaxSize()).IsNone());
    EXPECT_FALSE(mCryptoProvider->X509CertToPEM(certs[0], small).IsNone());
}

TEST_P(CryptoProviderTest, PEMToX509CertsInvalidInput)
{
    StaticArray<x509::Certificate, 1> certs;

    EXPECT_FALSE(
        mCryptoProvider->PEMToX509Certs("-----BEGIN CERTIFICATE-----\nAAAA\n-----END CERTIFICATE-----\n", certs)
            .IsNone());

    StaticString<cCertPEMLen> ca;
    StaticString<cCertPEMLen> client;

    ReadTestFile(TEST_CERTIFICATES_DIR "/ca.cer", ca);
    ReadTestFile(TEST_CERTIFICATES_DIR "/client.cer", client);

    StaticString<cCertPEMLen * 2> chain;

    chain.Append(ca).Append(client);

    certs.Clear();

    EXPECT_TRUE(mCryptoProvider->PEMToX509Certs(chain, certs).Is(ErrorEnum::eNoMemory));

    const uint8_t     invalidDER[] = {0x30, 0x03, 0x02, 0x01};
    x509::Certificate cert;

    EXPECT_FALSE(mCryptoProvider->DERToX509Cert(Array<uint8_t>(invalidDER, sizeof(invalidDER)), cert).IsNone());
}

TEST_P(CryptoProviderTest, PEMToX509CertsSubjectAltNameURIs)
{
    StaticString<cCertPEMLen> pem;
    x509::CertificateChain    chain;

    ReadTestFile(CRYPTOHELPER_CERTS_DIR "/online.pem", pem);

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(pem, chain).IsNone());
    ASSERT_EQ(chain.Size(), 1u);
    ASSERT_EQ(chain[0].mSubjectURLs.Size(), 1u);
    EXPECT_EQ(chain[0].mSubjectURLs[0], "urn:aos:unit:c183de63-e2b7-4776-90e0-b7c9b8f740e8");
    ASSERT_EQ(chain[0].mIssuerURLs.Size(), 1u);
    EXPECT_EQ(chain[0].mIssuerURLs[0], "https://www.mytest.com");

    for (const auto* name : {"onlineSanTooManyURIs", "onlineSanLongURI"}) {
        ReadTestFile(std::string(CRYPTOHELPER_CERTS_DIR "/") + name + ".pem", pem);

        chain.Clear();

        EXPECT_TRUE(mCryptoProvider->PEMToX509Certs(pem, chain).Is(ErrorEnum::eNoMemory)) << name;
    }
}

TEST_P(CryptoProviderTest, PEMToX509CertsIssuerAltNameAfterKeyIDs)
{
    StaticString<cCertPEMLen> pem;
    x509::CertificateChain    chain;

    ReadTestFile(CRYPTOHELPER_CERTS_DIR "/onlineIssuerAltNameLast.pem", pem);

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(pem, chain).IsNone());
    ASSERT_EQ(chain.Size(), 1u);
    EXPECT_FALSE(chain[0].mSubjectKeyID.IsEmpty());
    EXPECT_FALSE(chain[0].mAuthorityKeyID.IsEmpty());
    ASSERT_EQ(chain[0].mIssuerURLs.Size(), 1u);
    EXPECT_EQ(chain[0].mIssuerURLs[0], "https://www.mytest.com");
}

TEST_P(CryptoProviderTest, PEMToX509CertsKeyIDLikeOID)
{
    StaticString<cCertPEMLen> pem;
    x509::CertificateChain    chain;

    ReadTestFile(CRYPTOHELPER_CERTS_DIR "/onlineKeyIdLikeOID.pem", pem);

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(pem, chain).IsNone());
    ASSERT_EQ(chain.Size(), 1u);
    EXPECT_EQ(chain[0].mSubjectKeyID.Size(), crypto::cSHA1DigestSize);
}

TEST_P(CryptoProviderTest, PEMToX509CertsLongKeyIDs)
{
    StaticString<cCertPEMLen> pem;
    x509::CertificateChain    chain;

    for (const auto* name : {"longKeyIdCA", "onlineLongAuthorityKeyId"}) {
        ReadTestFile(std::string(CRYPTOHELPER_CERTS_DIR "/") + name + ".pem", pem);

        chain.Clear();

        EXPECT_TRUE(mCryptoProvider->PEMToX509Certs(pem, chain).Is(ErrorEnum::eNoMemory)) << name;
    }
}

TEST_P(CryptoProviderTest, CreateClientCert)
{
    StaticString<cCertPEMLen>    caCert;
    StaticString<cPrivKeyPEMLen> caKey;

    ReadTestFile(TEST_CERTIFICATES_DIR "/ca.cer", caCert);
    ReadTestFile(TEST_CERTIFICATES_DIR "/ca.key", caKey);

    x509::CSR templ;

    ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN("CN=Client, O=Org, C=UA", templ.mSubject).IsNone());

    auto [privKey, err] = mFactory->GenerateECDSAPrivKey();
    ASSERT_TRUE(err.IsNone());

    StaticString<cCSRPEMLen> csr;

    ASSERT_TRUE(mCryptoProvider->CreateCSR(templ, *privKey, csr).IsNone());

    const uint8_t             serial[] = {0x01, 0x02, 0x03, 0x04};
    StaticString<cCertPEMLen> clientCert;

    err = mCryptoProvider->CreateClientCert(csr, caKey, caCert, Array<uint8_t>(serial, sizeof(serial)), clientCert);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<x509::Certificate, 1> certs;
    StaticArray<x509::Certificate, 1> caCerts;

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(clientCert, certs).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(caCert, caCerts).IsNone());

    EXPECT_EQ(certs[0].mSerial, Array<uint8_t>(serial, sizeof(serial)));
    EXPECT_EQ(certs[0].mSubject, templ.mSubject);
    EXPECT_EQ(certs[0].mIssuer, caCerts[0].mSubject);
    EXPECT_TRUE(GetBase<PublicKeyItf>(certs[0].mPublicKey).IsEqual(privKey->GetPublic()));

    const char* cInvalidPEM = "invalid PEM";

    EXPECT_FALSE(mCryptoProvider
                     ->CreateClientCert(cInvalidPEM, caKey, caCert, Array<uint8_t>(serial, sizeof(serial)), clientCert)
                     .IsNone());
    EXPECT_FALSE(
        mCryptoProvider->CreateClientCert(csr, cInvalidPEM, caCert, Array<uint8_t>(serial, sizeof(serial)), clientCert)
            .IsNone());
    EXPECT_FALSE(
        mCryptoProvider->CreateClientCert(csr, caKey, cInvalidPEM, Array<uint8_t>(serial, sizeof(serial)), clientCert)
            .IsNone());
}

TEST_P(CryptoProviderTest, PEMToX509PrivKeyDecrypt)
{
    StaticString<cPrivKeyPEMLen> keyPEM;
    StaticString<cCertPEMLen>    certPEM;

    ReadTestFile(TEST_CERTIFICATES_DIR "/client.key", keyPEM);
    ReadTestFile(TEST_CERTIFICATES_DIR "/client.cer", certPEM);

    auto [privKey, err] = mCryptoProvider->PEMToX509PrivKey(keyPEM);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_NE(privKey.Get(), nullptr);

    StaticArray<x509::Certificate, 1> certs;

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(certPEM, certs).IsNone());

    const char* msgRaw = "secret message";
    const auto  msg    = Array<uint8_t>(reinterpret_cast<const uint8_t*>(msgRaw), strlen(msgRaw));

    StaticArray<uint8_t, cSignatureSize> cipher;

    ASSERT_TRUE(mFactory->Encrypt(certs[0].mPublicKey.GetValue<RSAPublicKey>(), msg, cipher).IsNone());

    StaticArray<uint8_t, cSignatureSize> result;
    DecryptionOptions                    options;

    options.SetValue<PKCS1v15DecryptionOptions>();

    ASSERT_TRUE(privKey->Decrypt(cipher, options, result).IsNone());
    EXPECT_EQ(result, msg);

    PKCS1v15DecryptionOptions keySizeOpts;

    keySizeOpts.mKeySize = 256;
    options.SetValue<PKCS1v15DecryptionOptions>(keySizeOpts);

    EXPECT_TRUE(privKey->Decrypt(cipher, options, result).Is(ErrorEnum::eNotSupported));

    OAEPDecryptionOptions oaepOpts;

    oaepOpts.mHash = HashEnum::eSHA256;
    options.SetValue<OAEPDecryptionOptions>(oaepOpts);

    EXPECT_FALSE(privKey->Decrypt(cipher, options, result).IsNone());

    StaticArray<uint8_t, cSignatureSize> signature;

    EXPECT_TRUE(privKey->Sign(msg, {HashEnum::eSHA256}, signature).Is(ErrorEnum::eNotSupported));
}

TEST_P(CryptoProviderTest, PEMToX509PrivKeyErrors)
{
    EXPECT_FALSE(mCryptoProvider->PEMToX509PrivKey("invalid PEM").mError.IsNone());

    StaticString<cPrivKeyPEMLen> keyPEM;

    ReadTestFile(TEST_CERTIFICATES_DIR "/client.key", keyPEM);

    mAllocator.FailAll();

    EXPECT_TRUE(mCryptoProvider->PEMToX509PrivKey(keyPEM).mError.Is(ErrorEnum::eNoMemory));

    mAllocator.Reset();
}

TEST_P(CryptoProviderTest, CreateHashAllAlgorithms)
{
    const struct {
        HashEnum    mHash;
        const char* mExpected;
    } testCases[] = {
        {HashEnum::eSHA1, "a9993e364706816aba3e25717850c26c9cd0d89d"},
        {HashEnum::eSHA224, "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"},
        {HashEnum::eSHA384,
            "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7"},
        {HashEnum::eSHA512,
            "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d44236"
            "43ce80e2a9ac94fa54ca49f"},
        {HashEnum::eSHA512_224, "4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa"},
        {HashEnum::eSHA512_256, "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"},
        {HashEnum::eSHA3_224, "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf"},
    };

    for (const auto& testCase : testCases) {
        SCOPED_TRACE(Hash(testCase.mHash).ToString().CStr());

        auto [hasher, err] = mHashProvider->CreateHash(testCase.mHash);

        // PSA build of MbedTLS doesn't support truncated SHA512 variants
        if (mFactory->GetName() == "MBedTLS"
            && (testCase.mHash == HashEnum::eSHA512_224 || testCase.mHash == HashEnum::eSHA512_256)) {
            EXPECT_FALSE(err.IsNone());

            continue;
        }

        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        ASSERT_TRUE(hasher->Update(String("abc").AsByteArray()).IsNone());

        StaticArray<uint8_t, cSHA512Size> result;

        ASSERT_TRUE(hasher->Finalize(result).IsNone());

        EXPECT_EQ(std::vector<uint8_t>(result.begin(), result.end()), HexToBytes(testCase.mExpected));
    }
}

TEST_P(CryptoProviderTest, CreateHashErrors)
{
    EXPECT_FALSE(mHashProvider->CreateHash(HashEnum::eNone).mError.IsNone());

    mAllocator.FailAll();

    EXPECT_TRUE(mHashProvider->CreateHash(HashEnum::eSHA256).mError.Is(ErrorEnum::eNoMemory));

    mAllocator.Reset();

    {
        auto [hasher, err] = mHashProvider->CreateHash(HashEnum::eSHA256);
        ASSERT_TRUE(err.IsNone());

        StaticArray<uint8_t, 16> result;

        EXPECT_FALSE(hasher->Finalize(result).IsNone());
    }

    {
        auto [hasher, err] = mHashProvider->CreateHash(HashEnum::eSHA256);
        ASSERT_TRUE(err.IsNone());

        StaticArray<uint8_t, cSHA256Size> result;

        ASSERT_TRUE(hasher->Finalize(result).IsNone());

        EXPECT_FALSE(hasher->Update(String("abc").AsByteArray()).IsNone());
        EXPECT_FALSE(hasher->Finalize(result).IsNone());
    }
}

TEST_P(CryptoProviderTest, CreateUUIDv4)
{
    auto [uuid1, err1] = mCryptoProvider->CreateUUIDv4();
    ASSERT_TRUE(err1.IsNone());

    auto [uuid2, err2] = mCryptoProvider->CreateUUIDv4();
    ASSERT_TRUE(err2.IsNone());

    ASSERT_EQ(uuid1.Size(), uuid::cUUIDSize);
    EXPECT_NE(uuid1, uuid2);

    for (const auto& uuid : {uuid1, uuid2}) {
        EXPECT_EQ(uuid[6] & 0xf0, 0x40);
        EXPECT_EQ(uuid[8] & 0xc0, 0x80);
    }
}

TEST_P(CryptoProviderTest, CreateUUIDv5NameTooLong)
{
    std::vector<uint8_t> name(cSHA1InputDataSize + 1, 'a');

    EXPECT_FALSE(mCryptoProvider->CreateUUIDv5(uuid::UUID(), tests::utils::ConvertToArray(name)).mError.IsNone());
}

TEST_P(CryptoProviderTest, RandBufferDefaultSize)
{
    StaticArray<uint8_t, 8> buffer;

    ASSERT_TRUE(mRandomProvider->RandBuffer(buffer).IsNone());
    EXPECT_EQ(buffer.Size(), buffer.MaxSize());
}

TEST_P(CryptoProviderTest, ASN1EncodeDNErrors)
{
    StaticArray<uint8_t, 100> result;

    EXPECT_FALSE(mCryptoProvider->ASN1EncodeDN("CN", result).IsNone());
    EXPECT_FALSE(mCryptoProvider->ASN1EncodeDN("UNKNOWNKEY=value", result).IsNone());

    StaticArray<uint8_t, 4> small;

    EXPECT_FALSE(mCryptoProvider->ASN1EncodeDN("C=UA, CN=Aos Core", small).IsNone());

    ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN("C=UA, CN=Aos Core", result).IsNone());

    // OpenSSL implementation skips empty entries
    if (mFactory->GetName() == "OpenSSL") {
        StaticArray<uint8_t, 100> withEmpty;

        ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN("C=UA, , CN=Aos Core", withEmpty).IsNone());
        EXPECT_EQ(withEmpty, result);
    }

    StaticString<100> decoded;

    ASSERT_TRUE(mCryptoProvider->ASN1DecodeDN(result, decoded).IsNone());
    EXPECT_EQ(decoded, "C=UA, CN=Aos Core");

    StaticString<5> smallStr;

    EXPECT_FALSE(mCryptoProvider->ASN1DecodeDN(result, smallStr).IsNone());
}

TEST_P(CryptoProviderTest, ASN1EncodeErrors)
{
    StaticArray<uint8_t, 4> small;

    StaticArray<asn1::ObjectIdentifier, 1> oids;

    oids.PushBack("1.3.6.1.5.5.7.3.1");

    EXPECT_FALSE(mCryptoProvider->ASN1EncodeObjectIds(oids, small).IsNone());

    const uint8_t bigInt[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    EXPECT_FALSE(mCryptoProvider->ASN1EncodeBigInt(Array<uint8_t>(bigInt, sizeof(bigInt)), small).IsNone());

    const uint8_t                  oid[] = {0x6, 0x8, 0x2b, 0x6, 0x1, 0x5, 0x5, 0x7, 0x3, 0x1};
    StaticArray<Array<uint8_t>, 1> items;

    items.PushBack(Array<uint8_t>(oid, sizeof(oid)));

    EXPECT_FALSE(mCryptoProvider->ASN1EncodeDERSequence(items, small).IsNone());
}

TEST_P(CryptoProviderTest, ASN1DecodeErrors)
{
    StaticArray<uint8_t, 100> result;

    const uint8_t octetString[] = {0x04, 0x04, 0x01, 0x02, 0x03, 0x04};
    const uint8_t oid[]         = {0x06, 0x03, 0x2b, 0x06, 0x01};
    const uint8_t truncated[]   = {0x04, 0x10, 0x01};

    EXPECT_FALSE(mCryptoProvider->ASN1DecodeOctetString(Array<uint8_t>(oid, sizeof(oid)), result).IsNone());
    EXPECT_FALSE(mCryptoProvider->ASN1DecodeOID(Array<uint8_t>(octetString, sizeof(octetString)), result).IsNone());

    EXPECT_FALSE(mCryptoProvider->ASN1DecodeOctetString(Array<uint8_t>(truncated, sizeof(truncated)), result).IsNone());
    EXPECT_FALSE(mCryptoProvider->ASN1DecodeOID(Array<uint8_t>(truncated, sizeof(truncated)), result).IsNone());

    StaticArray<uint8_t, 2> small;

    EXPECT_FALSE(
        mCryptoProvider->ASN1DecodeOctetString(Array<uint8_t>(octetString, sizeof(octetString)), small).IsNone());
    EXPECT_FALSE(mCryptoProvider->ASN1DecodeOID(Array<uint8_t>(oid, sizeof(oid)), small).IsNone());
}

TEST_P(CryptoProviderTest, VerifySignatureErrors)
{
    StaticArray<x509::Certificate, 1> certs;
    std::shared_ptr<PrivateKeyItf>    rsaKey;

    CreateCertificate(*mFactory, *mCryptoProvider, "CN=Test Subject, O=Org, C=GB", KeyTypeEnum::eRSA, rsaKey, certs);

    StaticArray<uint8_t, cSHA256Size> digest;

    {
        auto [hasher, err] = mHashProvider->CreateHash(HashEnum::eSHA256);
        ASSERT_TRUE(err.IsNone());
        ASSERT_TRUE(hasher->Update(String("Hello world").AsByteArray()).IsNone());
        ASSERT_TRUE(hasher->Finalize(digest).IsNone());
    }

    StaticArray<uint8_t, cSignatureSize> signature;

    ASSERT_TRUE(rsaKey->Sign(digest, {HashEnum::eSHA256}, signature).IsNone());

    Variant<ECDSAPublicKey, RSAPublicKey> rsaPubKey;
    rsaPubKey.SetValue(static_cast<const RSAPublicKey&>(rsaKey->GetPublic()));

    ASSERT_TRUE(mCryptoProvider->Verify(rsaPubKey, HashEnum::eSHA256, x509::PaddingEnum::ePKCS1v1_5, digest, signature)
                    .IsNone());

    EXPECT_FALSE(
        mCryptoProvider->Verify(rsaPubKey, HashEnum::eSHA256, x509::PaddingEnum::eNone, digest, signature).IsNone());

    EXPECT_FALSE(
        mCryptoProvider->Verify(rsaPubKey, HashEnum::eSHA256, x509::PaddingEnum::ePSS, digest, signature).IsNone());

    EXPECT_FALSE(
        mCryptoProvider->Verify(rsaPubKey, HashEnum::eNone, x509::PaddingEnum::ePKCS1v1_5, digest, signature).IsNone());

    EXPECT_FALSE(mCryptoProvider
                     ->Verify(rsaPubKey, HashEnum::eSHA256, x509::PaddingEnum::ePKCS1v1_5, Array<uint8_t>(), signature)
                     .IsNone());

    std::shared_ptr<PrivateKeyItf> ecKey;

    certs.Clear();
    CreateCertificate(*mFactory, *mCryptoProvider, "CN=Test Subject, O=Org, C=GB", KeyTypeEnum::eECDSA, ecKey, certs);

    const auto& ecPubKey = static_cast<const ECDSAPublicKey&>(ecKey->GetPublic());

    EXPECT_FALSE(rsaKey->GetPublic().IsEqual(ecPubKey));
    EXPECT_FALSE(ecPubKey.IsEqual(rsaKey->GetPublic()));
    EXPECT_STREQ(x509::Padding(x509::PaddingEnum::ePSS).ToString().CStr(), "PSS");

    Variant<ECDSAPublicKey, RSAPublicKey> pubKey;
    pubKey.SetValue(ecPubKey);

    StaticArray<uint8_t, 96> wrongSignature;

    wrongSignature.Resize(wrongSignature.MaxSize(), 0x11);

    EXPECT_FALSE(
        mCryptoProvider->Verify(pubKey, HashEnum::eSHA256, x509::PaddingEnum::eNone, digest, wrongSignature).IsNone());

    const uint8_t unknownOID[] = {0x2a, 0x03, 0x04};

    pubKey.SetValue<ECDSAPublicKey>(Array<uint8_t>(unknownOID, sizeof(unknownOID)), ecPubKey.GetECPoint());

    EXPECT_FALSE(
        mCryptoProvider->Verify(pubKey, HashEnum::eSHA256, x509::PaddingEnum::eNone, digest, wrongSignature).IsNone());

    const uint8_t invalidPoint[] = {0x04, 0x01, 0x02, 0x03};

    pubKey.SetValue<ECDSAPublicKey>(ecPubKey.GetECParamsOID(), Array<uint8_t>(invalidPoint, sizeof(invalidPoint)));

    EXPECT_FALSE(
        mCryptoProvider->Verify(pubKey, HashEnum::eSHA256, x509::PaddingEnum::eNone, digest, wrongSignature).IsNone());
}

TEST_P(CryptoProviderTest, VerifyCertChainErrors)
{
    StaticString<cCertPEMLen>         buff;
    StaticArray<x509::Certificate, 1> rootCerts;
    StaticArray<x509::Certificate, 1> intermCerts;
    StaticArray<x509::Certificate, 1> leafCerts;

    ReadTestFile(TEST_CERTIFICATES_DIR "/ca.cer", buff);
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, rootCerts).IsNone());

    ReadTestFile(TEST_CERTIFICATES_DIR "/client_int.cer", buff);
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, intermCerts).IsNone());

    ReadTestFile(TEST_CERTIFICATES_DIR "/client.cer", buff);
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, leafCerts).IsNone());

    x509::VerifyOptions opts;

    EXPECT_FALSE(mCryptoProvider->Verify(Array<x509::Certificate>(), intermCerts, opts, leafCerts[0]).IsNone());

    EXPECT_FALSE(mCryptoProvider->Verify(rootCerts, Array<x509::Certificate>(), opts, leafCerts[0]).IsNone());

    const uint8_t invalidDER[] = {0x30, 0x03, 0x02, 0x01, 0x01};

    {
        auto invalidCerts = rootCerts;

        invalidCerts[0].mRaw = Array<uint8_t>(invalidDER, sizeof(invalidDER));

        EXPECT_FALSE(mCryptoProvider->Verify(invalidCerts, intermCerts, opts, leafCerts[0]).IsNone());
    }

    {
        auto invalidCerts = intermCerts;

        invalidCerts[0].mRaw = Array<uint8_t>(invalidDER, sizeof(invalidDER));

        EXPECT_FALSE(mCryptoProvider->Verify(rootCerts, invalidCerts, opts, leafCerts[0]).IsNone());
    }

    {
        auto invalidCert = leafCerts[0];

        invalidCert.mRaw = Array<uint8_t>(invalidDER, sizeof(invalidDER));

        EXPECT_FALSE(mCryptoProvider->Verify(rootCerts, intermCerts, opts, invalidCert).IsNone());
    }
}

TEST_P(CryptoProviderTest, CreateCertificateWithTemplateValues)
{
    Error             err;
    x509::Certificate templ;
    x509::Certificate parent;

    Tie(templ, err) = CreateCertTemplate("CN=Template, O=Org, C=UA", *mCryptoProvider);
    ASSERT_TRUE(err.IsNone());

    const uint8_t serial[] = {0x12, 0x34, 0x56, 0x78};
    const uint8_t skid[]   = {0x04, 0x04, 0x0a, 0x0b, 0x0c, 0x0d};

    templ.mSerial       = Array<uint8_t>(serial, sizeof(serial));
    templ.mSubjectKeyID = Array<uint8_t>(skid, sizeof(skid));

    templ.mIssuerURLs.PushBack("https://issuer.test");

    auto [privKey, genErr] = mFactory->GenerateECDSAPrivKey();
    ASSERT_TRUE(genErr.IsNone());

    StaticString<cCertPEMLen> pem;

    err = mCryptoProvider->CreateCertificate(templ, parent, *privKey, pem);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<x509::Certificate, 1> certs;

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(pem, certs).IsNone());

    const uint8_t expectedSKID[] = {0x0a, 0x0b, 0x0c, 0x0d};

    EXPECT_EQ(certs[0].mSerial, Array<uint8_t>(serial, sizeof(serial)));
    EXPECT_EQ(certs[0].mSubjectKeyID, Array<uint8_t>(expectedSKID, sizeof(expectedSKID)));

    if (mFactory->GetName() == "OpenSSL") {
        ASSERT_EQ(certs[0].mIssuerURLs.Size(), 1u);
        EXPECT_EQ(certs[0].mIssuerURLs[0], "https://issuer.test");
    }
}

TEST_P(CryptoProviderTest, CreateCertificateErrors)
{
    Error             err;
    x509::Certificate templ;
    x509::Certificate parent;

    Tie(templ, err) = CreateCertTemplate("CN=Template, O=Org, C=UA", *mCryptoProvider);
    ASSERT_TRUE(err.IsNone());

    auto [privKey, genErr] = mFactory->GenerateECDSAPrivKey();
    ASSERT_TRUE(genErr.IsNone());

    StaticString<cCertPEMLen> pem;

    {
        auto          invalidTempl = templ;
        const uint8_t invalidDN[]  = {0x30, 0x05, 0x01};

        invalidTempl.mSubject = Array<uint8_t>(invalidDN, sizeof(invalidDN));

        EXPECT_FALSE(mCryptoProvider->CreateCertificate(invalidTempl, parent, *privKey, pem).IsNone());
    }

    {
        FakePrivateKey failingKey(
            static_cast<const ECDSAPublicKey&>(privKey->GetPublic()), 96, Error(ErrorEnum::eFailed, "sign failed"));

        EXPECT_FALSE(mCryptoProvider->CreateCertificate(templ, parent, failingKey, pem).IsNone());

        x509::CSR csrTempl;

        csrTempl.mSubject = templ.mSubject;

        StaticString<cCSRPEMLen> csr;

        EXPECT_FALSE(mCryptoProvider->CreateCSR(csrTempl, failingKey, csr).IsNone());
    }
}

TEST_P(CryptoProviderTest, CreateCSRErrors)
{
    auto [privKey, err] = mFactory->GenerateRSAPrivKey();
    ASSERT_TRUE(err.IsNone());

    StaticString<cCSRPEMLen> pemCSR;

    {
        x509::CSR     templ;
        const uint8_t invalidDN[] = {0x30, 0x05, 0x01};

        templ.mSubject = Array<uint8_t>(invalidDN, sizeof(invalidDN));

        EXPECT_FALSE(mCryptoProvider->CreateCSR(templ, *privKey, pemCSR).IsNone());
    }

    {
        x509::CSR     templ;
        const uint8_t value[] = {0x30, 0x00};

        ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN("CN=Test", templ.mSubject).IsNone());

        templ.mExtraExtensions.Resize(1);
        templ.mExtraExtensions[0].mID    = "invalid";
        templ.mExtraExtensions[0].mValue = Array<uint8_t>(value, sizeof(value));

        EXPECT_FALSE(mCryptoProvider->CreateCSR(templ, *privKey, pemCSR).IsNone());
    }
}

TEST_P(CryptoProviderTest, CreateCSRWithDifferentKeys)
{
    struct TestCase {
        const char* mName;
        const char* mOID;
        const char* mPoint;
        size_t      mSignatureSize;
        bool        mOpenSSLSupported;
        bool        mMbedTLSSupported;
    };

    const TestCase ecTestCases[] = {
        {"prime192v1", "2a8648ce3d030101",
            "04fff826be81e5ce3028b44a763dd629dacaf303ac35c8ab3be63a79655b8894a19432739fca33d6d92984014062c36a22", 48,
            true, true},
        {"secp224r1", "2b81040021",
            "049bb4ec3c8995abb924ed6897f9ac6f2a6b1024bccff8406b64d46aab33120dbf1db84816723ae8b9ff1612b141211fde1c94fbf5"
            "10954f8d",
            56, true, true},
        {"prime256v1", "2a8648ce3d030107",
            "0449d2e537c544b70072cbebcb27e307d75d2fba5d85236bd4eaad159168b7338eea7b0ee026f0ef962a8eb22bc10ba650e1bd6c88"
            "db8b459aa4051004cee6e56a",
            64, true, true},
        {"secp192k1", "2b8104001f",
            "048deb0f6871d44692abaf7b619dc0cebd1e3295be2a6f416335c4011e8bcb42818a02573ce8dc97f2b4c0a2b636d6de08", 48,
            true, true},
        {"secp224k1", "2b81040020",
            "041a63807ec53532654898470df3aacd438027ece3c7c87f9876a7a6108d03882aa426c5eeab5fcfab556ef0b286ee9f43fd7076d2"
            "5e640145",
            56, true, false},
        {"secp256k1", "2b8104000a",
            "04bf2cc0eff65230c34c06368b630ab7ad61a2146f08717c54d0ef3e7d04e6b1718da4acd4ec4c4b055503025eb026203fd6960a26"
            "8f5db8ae3df032e0c8997a76",
            64, true, true},
        {"brainpoolP256r1", "2b2403030208010107",
            "046d5e20930b1f9636e76a83a5fa1e70579c85245840e59106fe20d7fb76ece485a36f74d52ef9178a46784314209514708e0e1040"
            "4c65f295bb561fb9c1cbbfa6",
            64, false, true},
        {"brainpoolP384r1", "2b240303020801010b",
            "0449e3e793ab62229ba9c1662161a74e973c7e9ac2ce326a4810ed72c3e5a5bddbcde55feec41559ed70246d34a2e57ca74b053d6"
            "2bb9a2fff4e373711801bfee2229f94ef3a698db219605cfba98d202f5371e45b367c11f2c240c38ddec1d17c",
            96, false, true},
    };

    x509::CSR templ;

    ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN("CN=Test, O=Org, C=UA", templ.mSubject).IsNone());

    for (const auto& testCase : ecTestCases) {
        SCOPED_TRACE(testCase.mName);

        const auto oid   = HexToBytes(testCase.mOID);
        const auto point = HexToBytes(testCase.mPoint);

        FakePrivateKey key(ECDSAPublicKey(tests::utils::ConvertToArray(oid), tests::utils::ConvertToArray(point)),
            testCase.mSignatureSize);

        StaticString<cCSRPEMLen> pemCSR;

        // OpenSSL provider can't hold brainpool OIDs: they don't fit into cECDSAParamsOIDSize with tag and length,
        // the PSA build of MbedTLS doesn't support secp224k1.
        const auto supported
            = mFactory->GetName() == "OpenSSL" ? testCase.mOpenSSLSupported : testCase.mMbedTLSSupported;

        auto err = mCryptoProvider->CreateCSR(templ, key, pemCSR);
        EXPECT_EQ(err.IsNone(), supported) << tests::utils::ErrorToStr(err);
    }

    for (const size_t modulusSize : {128, 512}) {
        SCOPED_TRACE(modulusSize);

        FakePrivateKey key(CreateFakeRSAPublicKey(modulusSize), modulusSize);

        StaticString<cCSRPEMLen> pemCSR;

        auto err = mCryptoProvider->CreateCSR(templ, key, pemCSR);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }
}

TEST_P(CryptoProviderTest, ReadStructErrors)
{
    asn1::ASN1ParseOptions optional;
    optional.mOptional = true;

    ASN1ReaderStub reader;

    EXPECT_TRUE(mCryptoProvider->ReadStruct(Array<uint8_t>(), optional, reader).mError.IsNone());

    const uint8_t truncated[] = {0x30};

    EXPECT_FALSE(mCryptoProvider->ReadStruct(Array<uint8_t>(truncated, sizeof(truncated)), {}, reader).mError.IsNone());

    const uint8_t integer[] = {0x02, 0x01, 0x05};
    const auto    intData   = Array<uint8_t>(integer, sizeof(integer));

    EXPECT_FALSE(mCryptoProvider->ReadStruct(intData, {}, reader).mError.IsNone());

    auto result = mCryptoProvider->ReadStruct(intData, optional, reader);
    EXPECT_TRUE(result.mError.Is(ErrorEnum::eNotFound));
    EXPECT_EQ(result.mRemaining, intData);

    asn1::ASN1ParseOptions tagged;
    tagged.mTag.SetValue(17);

    const uint8_t seq[]   = {0x30, 0x03, 0x02, 0x01, 0x05};
    const auto    seqData = Array<uint8_t>(seq, sizeof(seq));

    EXPECT_FALSE(mCryptoProvider->ReadStruct(seqData, tagged, reader).mError.IsNone());

    tagged.mOptional = true;

    EXPECT_TRUE(mCryptoProvider->ReadStruct(seqData, tagged, reader).mError.Is(ErrorEnum::eNotFound));

    asn1::ASN1ParseOptions intTag;
    intTag.mTag.SetValue(2);

    EXPECT_FALSE(mCryptoProvider->ReadStruct(intData, intTag, reader).mError.IsNone());

    const uint8_t short_[] = {0x30, 0x05, 0x02, 0x01};

    EXPECT_FALSE(mCryptoProvider->ReadStruct(Array<uint8_t>(short_, sizeof(short_)), {}, reader).mError.IsNone());

    ASN1ReaderStub failingReader(ErrorEnum::eFailed);

    EXPECT_TRUE(mCryptoProvider->ReadStruct(seqData, {}, failingReader).mError.Is(ErrorEnum::eFailed));
}

TEST_P(CryptoProviderTest, ReadContainerErrors)
{
    asn1::ASN1ParseOptions optional;
    optional.mOptional = true;

    ASN1ReaderStub reader;

    EXPECT_TRUE(mCryptoProvider->ReadSequence(Array<uint8_t>(), optional, reader).mError.IsNone());
    EXPECT_TRUE(mCryptoProvider->ReadSet(Array<uint8_t>(), optional, reader).mError.IsNone());

    const uint8_t truncated[] = {0x30};

    EXPECT_FALSE(
        mCryptoProvider->ReadSequence(Array<uint8_t>(truncated, sizeof(truncated)), {}, reader).mError.IsNone());

    const uint8_t seq[]   = {0x30, 0x03, 0x02, 0x01, 0x05};
    const auto    seqData = Array<uint8_t>(seq, sizeof(seq));

    EXPECT_FALSE(mCryptoProvider->ReadSet(seqData, {}, reader).mError.IsNone());

    auto result = mCryptoProvider->ReadSet(seqData, optional, reader);
    EXPECT_TRUE(result.mError.Is(ErrorEnum::eNotFound));
    EXPECT_EQ(result.mRemaining, seqData);

    asn1::ASN1ParseOptions tagged;
    tagged.mTag.SetValue(17);

    EXPECT_FALSE(mCryptoProvider->ReadSequence(seqData, tagged, reader).mError.IsNone());

    tagged.mOptional = true;

    EXPECT_TRUE(mCryptoProvider->ReadSequence(seqData, tagged, reader).mError.Is(ErrorEnum::eNotFound));

    const uint8_t          integer[] = {0x02, 0x01, 0x05};
    asn1::ASN1ParseOptions intTag;
    intTag.mTag.SetValue(2);

    EXPECT_FALSE(
        mCryptoProvider->ReadSequence(Array<uint8_t>(integer, sizeof(integer)), intTag, reader).mError.IsNone());

    const uint8_t short_[] = {0x30, 0x05, 0x02, 0x01};

    EXPECT_FALSE(mCryptoProvider->ReadSequence(Array<uint8_t>(short_, sizeof(short_)), {}, reader).mError.IsNone());

    const uint8_t invalidElem[] = {0x30, 0x02, 0x02, 0x05};

    EXPECT_FALSE(
        mCryptoProvider->ReadSequence(Array<uint8_t>(invalidElem, sizeof(invalidElem)), {}, reader).mError.IsNone());

    const uint8_t truncatedLongTag[] = {0x30, 0x02, 0x1f, 0x81};

    EXPECT_FALSE(mCryptoProvider->ReadSequence(Array<uint8_t>(truncatedLongTag, sizeof(truncatedLongTag)), {}, reader)
                     .mError.IsNone());

    const uint8_t longTag[] = {0x30, 0x04, 0x9f, 0x81, 0x01, 0x00};

    reader.mValues.clear();

    ASSERT_TRUE(mCryptoProvider->ReadSequence(Array<uint8_t>(longTag, sizeof(longTag)), {}, reader).mError.IsNone());
    ASSERT_EQ(reader.mValues.size(), 1u);
    EXPECT_EQ(reader.mValues[0], 129);

    ASN1ReaderStub failingReader(ErrorEnum::eFailed);

    EXPECT_TRUE(mCryptoProvider->ReadSequence(seqData, {}, failingReader).mError.Is(ErrorEnum::eFailed));
}

TEST_P(CryptoProviderTest, ReadPrimitiveErrors)
{
    asn1::ASN1ParseOptions optional;
    optional.mOptional = true;

    const uint8_t invalid[]   = {0x05, 0x00};
    const auto    invalidData = Array<uint8_t>(invalid, sizeof(invalid));

    {
        int32_t value = 1;

        EXPECT_TRUE(mCryptoProvider->ReadInteger(Array<uint8_t>(), optional, value).mError.Is(ErrorEnum::eNotFound));
        EXPECT_FALSE(mCryptoProvider->ReadInteger(invalidData, {}, value).mError.IsNone());

        auto result = mCryptoProvider->ReadInteger(invalidData, optional, value);
        EXPECT_TRUE(result.mError.Is(ErrorEnum::eNotFound));
        EXPECT_EQ(result.mRemaining, invalidData);
    }

    {
        StaticArray<uint8_t, 2> value;

        EXPECT_TRUE(mCryptoProvider->ReadBigInt(Array<uint8_t>(), optional, value).mError.Is(ErrorEnum::eNotFound));
        EXPECT_FALSE(mCryptoProvider->ReadBigInt(invalidData, {}, value).mError.IsNone());
        EXPECT_TRUE(mCryptoProvider->ReadBigInt(invalidData, optional, value).mError.Is(ErrorEnum::eNotFound));

        const uint8_t bigInt[] = {0x02, 0x04, 0x01, 0x02, 0x03, 0x04};

        EXPECT_FALSE(mCryptoProvider->ReadBigInt(Array<uint8_t>(bigInt, sizeof(bigInt)), {}, value).mError.IsNone());
    }

    {
        asn1::ObjectIdentifier oid;

        EXPECT_TRUE(mCryptoProvider->ReadOID(Array<uint8_t>(), optional, oid).mError.Is(ErrorEnum::eNotFound));
        EXPECT_FALSE(mCryptoProvider->ReadOID(invalidData, {}, oid).mError.IsNone());
        EXPECT_TRUE(mCryptoProvider->ReadOID(invalidData, optional, oid).mError.Is(ErrorEnum::eNotFound));
    }

    {
        StaticArray<uint8_t, 2> value;

        EXPECT_TRUE(
            mCryptoProvider->ReadOctetString(Array<uint8_t>(), optional, value).mError.Is(ErrorEnum::eNotFound));
        EXPECT_FALSE(mCryptoProvider->ReadOctetString(invalidData, {}, value).mError.IsNone());
        EXPECT_TRUE(mCryptoProvider->ReadOctetString(invalidData, optional, value).mError.Is(ErrorEnum::eNotFound));

        const uint8_t octetString[] = {0x04, 0x04, 0x01, 0x02, 0x03, 0x04};

        EXPECT_FALSE(mCryptoProvider->ReadOctetString(Array<uint8_t>(octetString, sizeof(octetString)), {}, value)
                         .mError.IsNone());
    }

    {
        asn1::ASN1Value value;

        EXPECT_TRUE(mCryptoProvider->ReadRawValue(Array<uint8_t>(), optional, value).mError.Is(ErrorEnum::eNotFound));

        const uint8_t truncated[] = {0x04, 0x05, 0x01};

        EXPECT_FALSE(
            mCryptoProvider->ReadRawValue(Array<uint8_t>(truncated, sizeof(truncated)), {}, value).mError.IsNone());

        asn1::ASN1ParseOptions tagged;
        tagged.mTag.SetValue(4);

        EXPECT_FALSE(mCryptoProvider->ReadRawValue(invalidData, tagged, value).mError.IsNone());
    }
}

TEST_P(CryptoProviderTest, ReadAIDVariants)
{
    asn1::ASN1ParseOptions optional;
    optional.mOptional = true;

    asn1::AlgorithmIdentifier aid;

    EXPECT_TRUE(mCryptoProvider->ReadAID(Array<uint8_t>(), optional, aid).mError.Is(ErrorEnum::eNotFound));

    const uint8_t noParams[] = {0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02};

    ASSERT_TRUE(mCryptoProvider->ReadAID(Array<uint8_t>(noParams, sizeof(noParams)), {}, aid).mError.IsNone());
    EXPECT_EQ(aid.mOID, "2.16.840.1.101.3.4.1.2");
    EXPECT_TRUE(aid.mParams.mValue.IsEmpty());

    const uint8_t trailing[]
        = {0x30, 0x0f, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02, 0x05, 0x00, 0x05, 0x00};

    EXPECT_FALSE(mCryptoProvider->ReadAID(Array<uint8_t>(trailing, sizeof(trailing)), {}, aid).mError.IsNone());

    const uint8_t truncatedParams[]
        = {0x30, 0x0c, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02, 0x04};

    EXPECT_FALSE(
        mCryptoProvider->ReadAID(Array<uint8_t>(truncatedParams, sizeof(truncatedParams)), {}, aid).mError.IsNone());

    const uint8_t notOID[] = {0x30, 0x03, 0x02, 0x01, 0x05};

    EXPECT_FALSE(mCryptoProvider->ReadAID(Array<uint8_t>(notOID, sizeof(notOID)), {}, aid).mError.IsNone());

    // context specific tag with sequence number
    const uint8_t contextTag[] = {0xb0, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02};

    EXPECT_FALSE(mCryptoProvider->ReadAID(Array<uint8_t>(contextTag, sizeof(contextTag)), {}, aid).mError.IsNone());
}

namespace {
const char* const cEd25519CertPEM = R"(-----BEGIN CERTIFICATE-----
MIIBGTCBzKADAgECAhRkbztAI8dMVxYOJzUVg53GlX/VtTAFBgMrZXAwEjEQMA4G
A1UEAwwHZWQyNTUxOTAgFw0yNjExMTkxMzQ4NDVaGA8yMTI2MTAyNjEzNDg0NVow
EjEQMA4GA1UEAwwHZWQyNTUxOTAqMAUGAytlcAMhAKUK3ZQl+HSTurVQekBisahv
hoD1hMPdZVhJBmRgWUCNozIwMDAdBgNVHQ4EFgQUXSAZyRMTt5BpGswrh3s3PLMk
FTswDwYDVR0TAQH/BAUwAwEB/zAFBgMrZXADQQCCo3smvZCvrHxxy6CO/Xe9yxTd
bN4EwBu4tB8Y5Ik8kiLjbs2yS3joJ8vRkT4HpGs2+R1npyMj+9E1DXsQ1fMO
-----END CERTIFICATE-----
)";

const HashEnum cAllHashes[]
    = {HashEnum::eSHA1, HashEnum::eSHA224, HashEnum::eSHA256, HashEnum::eSHA384, HashEnum::eSHA512,
        HashEnum::eSHA512_224, HashEnum::eSHA512_256, HashEnum::eSHA3_224, HashEnum::eSHA3_256, HashEnum::eNone};

const char* const cECPrivKeyPEM = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgZXK/LQJH0IG+UEze
Zl982xo0eJdAus8GJWhiTofwoXqhRANCAARXmTvj1/SYft05SSOHrlwFScy4dBZS
b7E4JP94WH+v3DwvZc/pwphEzZ1lKcAO6BN1uMgAs5ufOWMmqjVmj86F
-----END PRIVATE KEY-----
)";

} // namespace

TEST_P(CryptoProviderTest, VerifySignatureWithAllHashes)
{
    for (const auto keyType : {KeyTypeEnum::eRSA, KeyTypeEnum::eECDSA}) {
        StaticArray<x509::Certificate, 1> certs;
        std::shared_ptr<PrivateKeyItf>    privKey;

        CreateCertificate(*mFactory, *mCryptoProvider, "CN=Test Subject, O=Org, C=GB", keyType, privKey, certs);

        for (const auto hash : cAllHashes) {
            SCOPED_TRACE(std::string(KeyType(keyType).ToString().CStr()) + " " + Hash(hash).ToString().CStr());

            StaticArray<uint8_t, cSHA2DigestSize> digest;

            if (auto [hasher, err] = mHashProvider->CreateHash(hash); err.IsNone()) {
                ASSERT_TRUE(hasher->Update(String("Hello world").AsByteArray()).IsNone());
                ASSERT_TRUE(hasher->Finalize(digest).IsNone());
            } else {
                digest.Resize(32, 0x11);
            }

            StaticArray<uint8_t, cSignatureSize> signature;

            // test keys sign digests of a single hash only
            const auto signed_ = privKey->Sign(digest, {hash}, signature).IsNone();
            if (!signed_) {
                signature.Resize(64, 0x22);
            }

            const auto padding
                = keyType == KeyTypeEnum::eRSA ? x509::PaddingEnum::ePKCS1v1_5 : x509::PaddingEnum::eNone;

            // a signature is verified with the hash it's made with
            EXPECT_EQ(mCryptoProvider->Verify(certs[0].mPublicKey, hash, padding, digest, signature).IsNone(), signed_);
        }

        if (keyType == KeyTypeEnum::eECDSA) {
            StaticArray<uint8_t, cSHA2DigestSize> digest;
            StaticArray<uint8_t, cSignatureSize>  signature;

            digest.Resize(cSHA384Size, 0x11);
            ASSERT_TRUE(privKey->Sign(digest, {HashEnum::eSHA384}, signature).IsNone());
            ASSERT_TRUE(
                mCryptoProvider
                    ->Verify(certs[0].mPublicKey, HashEnum::eSHA384, x509::PaddingEnum::eNone, digest, signature)
                    .IsNone());

            EXPECT_FALSE(mCryptoProvider
                             ->Verify(certs[0].mPublicKey, HashEnum::eSHA384, x509::PaddingEnum::eNone,
                                 Array<uint8_t>(), signature)
                             .IsNone());

            // RSA padding doesn't apply to ECDSA: OpenSSL rejects it, MbedTLS ignores it
            EXPECT_EQ(mCryptoProvider
                          ->Verify(certs[0].mPublicKey, HashEnum::eSHA384, x509::PaddingEnum::ePSS, digest, signature)
                          .IsNone(),
                mFactory->GetName() != "OpenSSL");
        }
    }
}

TEST_P(CryptoProviderTest, CreateCSRWithUnsupportedCurves)
{
    struct TestCase {
        const char* mName;
        const char* mOID;
        size_t      mPointSize;
    };

    const TestCase testCases[] = {
        {"x25519", "2b656e", 32},
        {"x448", "2b656f", 56},
        {"unknown OID", "2a0304", 65},
        {"not a curve", "2b0e03021a", 65},
        {"invalid OID", "80", 65},
        {"long OID", "2b0601040182370a0301", 65},
        {"point of a wrong size", "2a8648ce3d030107", 64},
    };

    x509::CSR templ;

    ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN("CN=Test, O=Org, C=UA", templ.mSubject).IsNone());

    for (const auto& testCase : testCases) {
        SCOPED_TRACE(testCase.mName);

        const auto           oid = HexToBytes(testCase.mOID);
        std::vector<uint8_t> point(testCase.mPointSize, 0x09);

        point[0] = 0x04;

        FakePrivateKey key(ECDSAPublicKey(tests::utils::ConvertToArray(oid), tests::utils::ConvertToArray(point)), 64);

        StaticString<cCSRPEMLen> pemCSR;

        auto err = mCryptoProvider->CreateCSR(templ, key, pemCSR);
        EXPECT_FALSE(err.IsNone());
    }
}

TEST_P(CryptoProviderTest, CreateCSRWithUnsupportedExtensions)
{
    auto [privKey, err] = mFactory->GenerateRSAPrivKey();
    ASSERT_TRUE(err.IsNone());

    struct TestCase {
        const char*          mOID;
        std::vector<uint8_t> mValue;
    };

    const TestCase testCases[] = {{"2.5.29.17", {0x30, 0x00}}, {"2.5.29.37", {0x01, 0x02}}};

    for (const auto& testCase : testCases) {
        SCOPED_TRACE(testCase.mOID);

        x509::CSR templ;

        ASSERT_TRUE(mCryptoProvider->ASN1EncodeDN("CN=Test", templ.mSubject).IsNone());

        templ.mExtraExtensions.Resize(1);
        templ.mExtraExtensions[0].mID    = testCase.mOID;
        templ.mExtraExtensions[0].mValue = Array<uint8_t>(testCase.mValue.data(), testCase.mValue.size());

        StaticString<cCSRPEMLen> pemCSR;

        // MbedTLS adds extra extensions as is
        auto csrErr = mCryptoProvider->CreateCSR(templ, *privKey, pemCSR);
        EXPECT_EQ(csrErr.IsNone(), mFactory->GetName() != "OpenSSL") << tests::utils::ErrorToStr(csrErr);
    }
}

TEST_P(CryptoProviderTest, CreateCertificateWithInvalidTemplate)
{
    Error             err;
    x509::Certificate templ;

    Tie(templ, err) = CreateCertTemplate("CN=Template, O=Org, C=UA", *mCryptoProvider);
    ASSERT_TRUE(err.IsNone());

    auto [privKey, genErr] = mFactory->GenerateECDSAPrivKey();
    ASSERT_TRUE(genErr.IsNone());

    const bool    isOpenSSL        = mFactory->GetName() == "OpenSSL";
    const uint8_t notOctetString[] = {0x01, 0x02};

    StaticString<cCertPEMLen> pem;

    // OpenSSL expects DER encoded key identifiers, MbedTLS adds them as is
    {
        auto invalid = templ;

        invalid.mSubjectKeyID = Array<uint8_t>(notOctetString, sizeof(notOctetString));

        EXPECT_EQ(mCryptoProvider->CreateCertificate(invalid, x509::Certificate(), *privKey, pem).IsNone(), !isOpenSSL);
    }

    {
        auto invalid = templ;

        invalid.mAuthorityKeyID = Array<uint8_t>(notOctetString, sizeof(notOctetString));

        EXPECT_EQ(mCryptoProvider->CreateCertificate(invalid, x509::Certificate(), *privKey, pem).IsNone(), !isOpenSSL);
    }

    // OpenSSL parses the parent certificate
    {
        x509::Certificate parent;

        parent.mRaw = Array<uint8_t>(notOctetString, sizeof(notOctetString));

        EXPECT_EQ(mCryptoProvider->CreateCertificate(templ, parent, *privKey, pem).IsNone(), !isOpenSSL);
    }

    // MbedTLS requires the validity period
    {
        auto invalid = templ;

        invalid.mNotBefore = Time();

        err = mCryptoProvider->CreateCertificate(invalid, x509::Certificate(), *privKey, pem);
        EXPECT_TRUE(isOpenSSL ? err.IsNone() : err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);
    }

    {
        auto invalid = templ;

        invalid.mNotAfter = Time();

        err = mCryptoProvider->CreateCertificate(invalid, x509::Certificate(), *privKey, pem);
        EXPECT_TRUE(isOpenSSL ? err.IsNone() : err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);
    }
}

TEST_P(CryptoProviderTest, VerifyCertChainCurTimeBeforeValidity)
{
    StaticString<cCertPEMLen>         buff;
    StaticArray<x509::Certificate, 1> rootCerts;
    StaticArray<x509::Certificate, 1> intermCerts;
    StaticArray<x509::Certificate, 1> leafCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/ca.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, rootCerts).IsNone());
    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client_int.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, intermCerts).IsNone());
    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, leafCerts).IsNone());

    x509::VerifyOptions opts;

    opts.mCurrentTime = Time::Unix(1000);

    EXPECT_FALSE(mCryptoProvider->Verify(rootCerts, intermCerts, opts, leafCerts[0]).IsNone());
}

TEST_P(CryptoProviderTest, PEMToX509CertsUnsupportedKeyType)
{
    StaticArray<x509::Certificate, 1> certs;

    EXPECT_FALSE(mCryptoProvider->PEMToX509Certs(cEd25519CertPEM, certs).IsNone());
}

TEST_P(CryptoProviderTest, PrivKeyDecryptErrors)
{
    StaticString<cPrivKeyPEMLen> keyPEM;

    ReadTestFile(TEST_CERTIFICATES_DIR "/client.key", keyPEM);

    auto [privKey, err] = mCryptoProvider->PEMToX509PrivKey(keyPEM);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<uint8_t, cSignatureSize> cipher;
    StaticArray<uint8_t, cSignatureSize> result;
    DecryptionOptions                    options;

    // cipher of the key size that is greater than the modulus: the key's own public part isn't available
    StaticString<cCertPEMLen>         certPEM;
    StaticArray<x509::Certificate, 1> certs;

    ReadTestFile(TEST_CERTIFICATES_DIR "/client.cer", certPEM);
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(certPEM, certs).IsNone());

    cipher.Resize(certs[0].mPublicKey.GetValue<RSAPublicKey>().GetN().Size(), 0xff);

    options.SetValue<PKCS1v15DecryptionOptions>();

    EXPECT_FALSE(privKey->Decrypt(cipher, options, result).IsNone());

    // OAEP with any hash, including none
    for (const auto hash : cAllHashes) {
        SCOPED_TRACE(Hash(hash).ToString().CStr());

        OAEPDecryptionOptions oaepOpts;

        oaepOpts.mHash = hash;
        options.SetValue<OAEPDecryptionOptions>(oaepOpts);

        EXPECT_FALSE(privKey->Decrypt(cipher, options, result).IsNone());
    }
}

#ifdef WITH_OPENSSL

TEST_P(CryptoProviderTest, PrivKeyDecryptOAEP)
{
    StaticString<cPrivKeyPEMLen> keyPEM;

    ReadTestFile(TEST_CERTIFICATES_DIR "/client.key", keyPEM);

    auto [privKey, err] = mCryptoProvider->PEMToX509PrivKey(keyPEM);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    const std::string    msg = "secret message";
    std::vector<uint8_t> cipher;

    {
        auto* file = fopen(TEST_CERTIFICATES_DIR "/client.cer", "r");
        ASSERT_NE(file, nullptr);

        auto* cert = PEM_read_X509(file, nullptr, nullptr, nullptr);
        fclose(file);
        ASSERT_NE(cert, nullptr);

        auto*  ctx = EVP_PKEY_CTX_new(X509_get0_pubkey(cert), nullptr);
        size_t len = 0;

        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(EVP_PKEY_encrypt_init(ctx), 1);
        EXPECT_EQ(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING), 1);
        EXPECT_EQ(EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()), 1);
        EXPECT_EQ(EVP_PKEY_encrypt(ctx, nullptr, &len, reinterpret_cast<const uint8_t*>(msg.data()), msg.size()), 1);

        cipher.resize(len);

        EXPECT_EQ(
            EVP_PKEY_encrypt(ctx, cipher.data(), &len, reinterpret_cast<const uint8_t*>(msg.data()), msg.size()), 1);

        cipher.resize(len);

        EVP_PKEY_CTX_free(ctx);
        X509_free(cert);
    }

    StaticArray<uint8_t, cSignatureSize> result;
    DecryptionOptions                    options;
    OAEPDecryptionOptions                oaepOpts;

    oaepOpts.mHash = HashEnum::eSHA256;
    options.SetValue<OAEPDecryptionOptions>(oaepOpts);

    ASSERT_TRUE(privKey->Decrypt(Array<uint8_t>(cipher.data(), cipher.size()), options, result).IsNone());
    EXPECT_EQ(std::string(result.begin(), result.end()), msg);
}

#endif

TEST_P(CryptoProviderTest, PEMToX509PrivKeyNotRSA)
{
    auto [privKey, err] = mCryptoProvider->PEMToX509PrivKey(cECPrivKeyPEM);

    // OpenSSL provider loads RSA keys only
    if (mFactory->GetName() == "OpenSSL") {
        EXPECT_TRUE(err.Is(ErrorEnum::eNotSupported)) << tests::utils::ErrorToStr(err);
    } else {
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
        ASSERT_NE(privKey.Get(), nullptr);

        // RSA decryption isn't supported by an EC key
        const uint8_t                        cipher[] = {0x01, 0x02};
        StaticArray<uint8_t, cSignatureSize> result;
        DecryptionOptions                    options;

        options.SetValue<PKCS1v15DecryptionOptions>();

        EXPECT_TRUE(
            privKey->Decrypt(Array<uint8_t>(cipher, sizeof(cipher)), options, result).Is(ErrorEnum::eNotSupported));

        OAEPDecryptionOptions oaepOpts;

        oaepOpts.mHash = HashEnum::eSHA256;
        options.SetValue<OAEPDecryptionOptions>(oaepOpts);

        EXPECT_TRUE(
            privKey->Decrypt(Array<uint8_t>(cipher, sizeof(cipher)), options, result).Is(ErrorEnum::eNotSupported));
    }
}

TEST_P(CryptoProviderTest, ASN1EncodeDNLongEntry)
{
    StaticArray<uint8_t, 256> result;

    const auto err = mCryptoProvider->ASN1EncodeDN(("CN=" + std::string(60, 'a')).c_str(), result);

    // OpenSSL provider limits the size of a DN entry
    EXPECT_EQ(err.IsNone(), mFactory->GetName() != "OpenSSL") << tests::utils::ErrorToStr(err);
}

TEST_P(CryptoProviderTest, CreateCertificateWithInvalidTime)
{
    Error             err;
    x509::Certificate templ;

    Tie(templ, err) = CreateCertTemplate("CN=Template, O=Org, C=UA", *mCryptoProvider);
    ASSERT_TRUE(err.IsNone());

    auto [privKey, genErr] = mFactory->GenerateECDSAPrivKey();
    ASSERT_TRUE(genErr.IsNone());

    StaticString<cCertPEMLen> pem;

    for (const auto field : {0, 1}) {
        auto invalid = templ;

        (field == 0 ? invalid.mNotBefore : invalid.mNotAfter) = Time::Unix(300000000000);

        err = mCryptoProvider->CreateCertificate(invalid, x509::Certificate(), *privKey, pem);
        // year doesn't fit ASN.1 time
        EXPECT_FALSE(err.IsNone());
    }
}

} // namespace aos::crypto
