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

    ASSERT_TRUE(fs::ReadFileToString(UNIT_ONLINE_CERT_PATH, pem).IsNone())
        << "Failed to read generated cert: " << UNIT_ONLINE_CERT_PATH;

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
    EXPECT_STREQ(systemID.CStr(), "test-unit");
}

TEST_F(CryptoutilsTest, GetSystemIDFromCertNotFound)
{
    StaticString<cIDLen> systemID;

    EXPECT_TRUE(GetSystemIDFromCert("urn:aos:domain:aosedge.io", systemID).Is(ErrorEnum::eNotFound));
}

} // namespace aos::crypto
