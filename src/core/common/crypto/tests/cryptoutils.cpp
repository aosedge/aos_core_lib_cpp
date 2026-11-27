/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>

#include <gtest/gtest.h>

#include <core/common/crypto/cryptoutils.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/types/common.hpp>

#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>

namespace aos::crypto {
/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

using namespace testing;

class CryptoutilsTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        auto err = mCryptoFactory.Init(mAllocator);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

    DefaultCryptoFactory mCryptoFactory;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CryptoutilsTest, CalculateFileHash)
{
    constexpr auto cExpectedSHA256Str = "27dd1f61b867b6a0f6e9d8a41c43231de52107e53ae424de8f847b821db4b711";

    {
        std::ofstream f("test.txt");
        ASSERT_TRUE(f.is_open());

        f << std::string(10000, 'a');
    }

    StaticArray<uint8_t, cSHA256Size> hash;

    auto err = CalculateFileHash(String("test.txt"), crypto::HashEnum::eSHA256, mCryptoFactory.GetHashProvider(), hash);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<uint8_t, cSHA256Size> expectedHash;

    err = String(cExpectedSHA256Str).HexToByteArray(expectedHash);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(hash, expectedHash);
}

TEST_F(CryptoutilsTest, CalculateFileHashNoFile)
{
    StaticArray<uint8_t, cSHA256Size> hash;

    auto err = CalculateFileHash(
        String("file-not-exists"), crypto::HashEnum::eSHA256, mCryptoFactory.GetHashProvider(), hash);
    ASSERT_FALSE(err.IsNone());
}

TEST_F(CryptoutilsTest, GetSystemIDFromCert)
{
    StaticString<cIDLen> systemID;

    ASSERT_TRUE(GetSystemIDFromCert("urn:aos:unit:test-unit", systemID).IsNone());
    EXPECT_STREQ(systemID.CStr(), "test-unit");
}

TEST_F(CryptoutilsTest, GetSystemIDFromOnlineCertPEM)
{
    StaticString<cCertPEMLen> pem;

    ASSERT_TRUE(fs::ReadFileToString(CRYPTOHELPER_CERTS_DIR "/online.pem", pem).IsNone())
        << "Failed to read generated cert: " << CRYPTOHELPER_CERTS_DIR "/online.pem";

    x509::CertificateChain chain;

    ASSERT_TRUE(mCryptoFactory.GetCryptoProvider().PEMToX509Certs(pem, chain).IsNone());
    ASSERT_FALSE(chain.IsEmpty());
    ASSERT_FALSE(chain[0].mSubjectURLs.IsEmpty()) << "SAN URIs were not parsed from certificate";

    StaticString<cIDLen> systemID;
    Error                err = ErrorEnum::eNotFound;

    for (const auto& uri : chain[0].mSubjectURLs) {
        err = GetSystemIDFromCert(uri, systemID);
        if (err.IsNone()) {
            break;
        }
    }

    ASSERT_TRUE(err.IsNone());
    EXPECT_STREQ(systemID.CStr(), "c183de63-e2b7-4776-90e0-b7c9b8f740e8");
}

TEST_F(CryptoutilsTest, GetSystemIDFromCertNotFound)
{
    StaticString<cIDLen> systemID;

    EXPECT_TRUE(GetSystemIDFromCert("urn:aos:domain:aosedge.io", systemID).Is(ErrorEnum::eNotFound));
    EXPECT_TRUE(GetSystemIDFromCert("prefix:urn:aos:unit:test-unit", systemID).Is(ErrorEnum::eNotFound));
    EXPECT_TRUE(GetSystemIDFromCert(cSystemIDURNPrefix, systemID).Is(ErrorEnum::eNotFound));
}

TEST_F(CryptoutilsTest, GetSystemIDFromCertNoMemory)
{
    StaticString<4> systemID;

    EXPECT_TRUE(GetSystemIDFromCert("urn:aos:unit:test-unit", systemID).Is(ErrorEnum::eNoMemory));
}

TEST_F(CryptoutilsTest, PEMToX509CertsSubjectURLsOverflow)
{
    for (const auto* name : {"onlineSanTooManyURIs", "onlineSanLongURI"}) {
        StaticString<cCertPEMLen> pem;
        const auto                path = std::string(CRYPTOHELPER_CERTS_DIR "/") + name + ".pem";

        ASSERT_TRUE(fs::ReadFileToString(path.c_str(), pem).IsNone()) << "Failed to read generated cert: " << path;

        x509::CertificateChain chain;

        EXPECT_TRUE(mCryptoFactory.GetCryptoProvider().PEMToX509Certs(pem, chain).Is(ErrorEnum::eNoMemory)) << name;
    }
}

namespace {
class HashStub : public HashItf {
public:
    HashStub(const Error& updateErr, const Error& finalizeErr)
        : mUpdateErr(updateErr)
        , mFinalizeErr(finalizeErr)
    {
    }

    Error Update(const Array<uint8_t>& data) override
    {
        mUpdated += data.Size();

        return mUpdateErr;
    }

    Error Finalize(Array<uint8_t>& hash) override
    {
        if (!mFinalizeErr.IsNone()) {
            return mFinalizeErr;
        }

        return hash.Resize(1, static_cast<uint8_t>(mUpdated != 0));
    }

private:
    Error  mUpdateErr;
    Error  mFinalizeErr;
    size_t mUpdated = 0;
};

class HasherStub : public HasherItf {
public:
    explicit HasherStub(AllocatorItf& allocator)
        : mAllocator(allocator)
    {
    }

    RetWithError<UniquePtr<HashItf>> CreateHash(const Hash& algorithm) override
    {
        (void)algorithm;

        if (!mCreateErr.IsNone()) {
            return {{}, mCreateErr};
        }

        auto hash = MakeUnique<HashStub>(&mAllocator, mUpdateErr, mFinalizeErr);

        return {UniquePtr<HashItf>(Move(hash)), ErrorEnum::eNone};
    }

    Error mCreateErr;
    Error mUpdateErr;
    Error mFinalizeErr;

private:
    AllocatorItf& mAllocator;
};

x509::Certificate CreateCACert()
{
    x509::Certificate cert;
    const uint8_t     skid[]     = {0x01, 0x02, 0x03, 0x04};
    const uint8_t     exponent[] = {0x01, 0x00, 0x01};

    std::vector<uint8_t> modulus(256, 0xC5);

    cert.mVersion = x509::cX509Version3;
    cert.mIsCA    = true;
    cert.mKeyUsage.SetValue(x509::keyusage::cKeyCertSign | x509::keyusage::cCRLSign);
    cert.mSubjectKeyID = Array<uint8_t>(skid, sizeof(skid));
    cert.mPublicKey.SetValue<RSAPublicKey>(
        tests::utils::ConvertToArray(modulus), Array<uint8_t>(exponent, sizeof(exponent)));

    return cert;
}

} // namespace

TEST_F(CryptoutilsTest, CalculateFileHashErrors)
{
    {
        std::ofstream f("test.txt");
        ASSERT_TRUE(f.is_open());

        f << "some content";
    }

    HasherStub                       hasher(mAllocator);
    StaticArray<uint8_t, 1>          hash;
    const StaticString<cFilePathLen> path = "test.txt";

    hasher.mCreateErr = ErrorEnum::eNotSupported;
    EXPECT_TRUE(CalculateFileHash(path, HashEnum::eSHA256, hasher, hash).Is(ErrorEnum::eNotSupported));

    hasher.mCreateErr = ErrorEnum::eNone;
    hasher.mUpdateErr = ErrorEnum::eFailed;
    EXPECT_TRUE(CalculateFileHash(path, HashEnum::eSHA256, hasher, hash).Is(ErrorEnum::eFailed));

    hasher.mUpdateErr = ErrorEnum::eEOF;
    ASSERT_TRUE(CalculateFileHash(path, HashEnum::eSHA256, hasher, hash).IsNone());
    EXPECT_EQ(hash[0], 1);

    hasher.mUpdateErr   = ErrorEnum::eNone;
    hasher.mFinalizeErr = ErrorEnum::eWrongState;
    EXPECT_TRUE(CalculateFileHash(path, HashEnum::eSHA256, hasher, hash).Is(ErrorEnum::eWrongState));

    // a directory can be opened but can't be read
    hasher.mFinalizeErr = ErrorEnum::eNone;
    EXPECT_FALSE(CalculateFileHash(String("."), HashEnum::eSHA256, hasher, hash).IsNone());
}

TEST_F(CryptoutilsTest, ValidateCACert)
{
    EXPECT_TRUE(ValidateCACert(CreateCACert()).IsNone());

    {
        auto cert = CreateCACert();

        cert.mVersion = 1;
        EXPECT_TRUE(ValidateCACert(cert).Is(ErrorEnum::eFailed));
    }

    {
        auto cert = CreateCACert();

        cert.mIsCA = false;
        EXPECT_TRUE(ValidateCACert(cert).Is(ErrorEnum::eFailed));
    }

    {
        auto cert = CreateCACert();

        cert.mKeyUsage.SetValue(x509::keyusage::cDigitalSignature);
        EXPECT_TRUE(ValidateCACert(cert).Is(ErrorEnum::eFailed));
    }

    // missing cRLSign and key usage are only warnings
    {
        auto cert = CreateCACert();

        cert.mKeyUsage.SetValue(x509::keyusage::cKeyCertSign);
        EXPECT_TRUE(ValidateCACert(cert).IsNone());

        cert.mKeyUsage.Reset();
        EXPECT_TRUE(ValidateCACert(cert).IsNone());
    }

    {
        auto cert = CreateCACert();

        cert.mSubjectKeyID.Clear();
        EXPECT_TRUE(ValidateCACert(cert).Is(ErrorEnum::eFailed));
    }

    // short RSA key is only a warning
    {
        auto                 cert = CreateCACert();
        std::vector<uint8_t> modulus(128, 0xC5);
        const uint8_t        exponent[] = {0x01, 0x00, 0x01};

        cert.mPublicKey.SetValue<RSAPublicKey>(
            tests::utils::ConvertToArray(modulus), Array<uint8_t>(exponent, sizeof(exponent)));
        EXPECT_TRUE(ValidateCACert(cert).IsNone());
    }
}

TEST_F(CryptoutilsTest, ValidateCACertECCurves)
{
    const uint8_t point[] = {0x04, 0x01, 0x02};

    const struct {
        std::vector<uint8_t> mOID;
        bool                 mSupported;
    } testCases[] = {
        {{0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07}, true},
        {{0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07}, true},
        {{0x2B, 0x81, 0x04, 0x00, 0x22}, true},
        {{0x2B, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01, 0x0D}, true},
        {{0x2B, 0x81, 0x04, 0x00, 0x01}, false},
        {{0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x01}, false},
        {{0x06}, false},
    };

    for (const auto& testCase : testCases) {
        auto cert = CreateCACert();

        cert.mPublicKey.SetValue<ECDSAPublicKey>(
            tests::utils::ConvertToArray(testCase.mOID), Array<uint8_t>(point, sizeof(point)));

        auto err = ValidateCACert(cert);

        if (testCase.mSupported) {
            EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
        } else {
            EXPECT_TRUE(err.Is(ErrorEnum::eNotSupported)) << tests::utils::ErrorToStr(err);
        }
    }
}

} // namespace aos::crypto
