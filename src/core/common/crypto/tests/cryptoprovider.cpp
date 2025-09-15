/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <core/common/crypto/cryptoutils.hpp>
#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>

#if defined(WITH_MBEDTLS)
#include <core/common/tests/crypto/providers/mbedtlsfactory.hpp>
#endif

#if defined(WITH_OPENSSL)
#include <core/common/tests/crypto/providers/opensslfactory.hpp>
#endif

using namespace testing;

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

        ASSERT_TRUE(mFactory->Init().IsNone());

        mCryptoProvider = &mFactory->GetCryptoProvider();
        mHashProvider   = &mFactory->GetHashProvider();
        mRandomProvider = &mFactory->GetRandomProvider();
    }

protected:
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

template <typename T>
Array<T> ConvertToArray(std::vector<T>& src)
{
    return Array<T> {src.data(), src.size()};
}

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
    ASSERT_EQ(certs[0].mSubjectKeyId, certs[0].mAuthorityKeyId);
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

    ASSERT_TRUE(mCryptoProvider->DERToX509Cert(ConvertToArray(derCert), cert).IsNone());

    ASSERT_TRUE(cert.mSubjectKeyId == cert.mAuthorityKeyId);

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
    ASSERT_EQ(certs[0].mSubjectKeyId, certs[0].mAuthorityKeyId);

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
    aos::crypto::x509::CSR templ;
    const char*            subjectName = "CN=Test Subject, O=Org, C=GB";
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
    Error      err = ErrorEnum::eNone;

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

        StaticString<cSHA256Size * 2> hashStr;
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

        StaticString<cSHA256Size * 2> hashStr;
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

    aos::StaticString<cSHA256Size * 2> hashStr;
    ASSERT_TRUE(hashStr.ByteArrayToHex(result).IsNone());

    EXPECT_EQ(hashStr, String("a98c0eb748fcf3c87b8d231c0866f20dd12202923de5e93696ee4a3ad3da91ec"));

    LOG_DBG() << "SHA256: " << hashStr;
}

TEST_P(CryptoProviderTest, RandInt)
{
    constexpr uint64_t kMaxValue = 100;

    for (int i = 0; i < 100; i++) {
        auto [value, err] = mRandomProvider->RandInt(kMaxValue);
        ASSERT_TRUE(err.IsNone());
        ASSERT_LT(value, kMaxValue);
    }

    auto [value1, err1] = mRandomProvider->RandInt(kMaxValue);
    auto [value2, err2] = mRandomProvider->RandInt(kMaxValue);

    ASSERT_TRUE(err1.IsNone());
    ASSERT_TRUE(err2.IsNone());
    ASSERT_NE(value1, value2);
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

    AESCipherItf::Block      inBlock {};
    AESCipherItf::Block      outBlock {};
    StaticArray<uint8_t, 64> ciphertext;

    size_t offset = 0;
    while (offset < plain.Size()) {
        size_t chunk = std::min<size_t>(16, plain.Size() - offset);

        inBlock = Array<uint8_t>(plain.Get() + offset, chunk);

        ASSERT_TRUE(cipherPtr->EncryptBlock(inBlock, outBlock).IsNone());

        ciphertext.Append(outBlock);
        offset += chunk;
    }

    AESCipherItf::Block finalBlock {};
    ASSERT_TRUE(cipherPtr->Finalize(finalBlock).IsNone());
    ciphertext.Append(finalBlock);

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

    AESCipherItf::Block      inBlock {};
    AESCipherItf::Block      outBlock {};
    StaticArray<uint8_t, 64> plaintext;

    size_t offset = 0;
    while (offset < ciphertext.Size()) {
        inBlock = Array<uint8_t>(ciphertext.Get() + offset, 16);

        ASSERT_TRUE(cipherPtr->DecryptBlock(inBlock, outBlock).IsNone());
        plaintext.Append(outBlock);
        offset += 16;
    }

    AESCipherItf::Block finalBlock {};
    ASSERT_TRUE(cipherPtr->Finalize(finalBlock).IsNone());
    plaintext.Append(finalBlock);

    const char*    expectedRaw = "TWO BLOCK AES RAW MESSAGE";
    Array<uint8_t> expected(reinterpret_cast<const uint8_t*>(expectedRaw), strlen(expectedRaw));

    EXPECT_EQ(plaintext, expected);
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
        mCryptoProvider->Verify(pubKey, HashEnum::eSHA256, PaddingEnum::ePKCS1v1_5, digest, signature).IsNone());
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

    EXPECT_TRUE(mCryptoProvider->Verify(pubKey, HashEnum::eSHA384, PaddingEnum::eNone, digest, signature).IsNone());
}

TEST_P(CryptoProviderTest, VerifyCertChain)
{
    StaticString<crypto::cCertPEMLen> buff;
    StaticArray<x509::Certificate, 1> rootCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/ca.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, rootCerts).IsNone());

    StaticArray<x509::Certificate, 1> intermCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client_int.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, intermCerts).IsNone());

    StaticArray<x509::Certificate, 1> leafCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, leafCerts).IsNone());

    VerifyOptions opts;
    opts.mCurrentTime = Time();

    ASSERT_TRUE(mCryptoProvider->Verify(rootCerts, intermCerts, opts, leafCerts[0]).IsNone());
}

TEST_P(CryptoProviderTest, VerifyCertChainCurTimeExceeds)
{
    StaticString<crypto::cCertPEMLen> buff;
    StaticArray<x509::Certificate, 1> rootCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/ca.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, rootCerts).IsNone());

    StaticArray<x509::Certificate, 1> intermCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client_int.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, intermCerts).IsNone());

    StaticArray<x509::Certificate, 1> leafCerts;

    ASSERT_TRUE(fs::ReadFileToString(TEST_CERTIFICATES_DIR "/client.cer", buff).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(buff, leafCerts).IsNone());

    VerifyOptions opts;
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

} // namespace aos::crypto
