/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>

#include <gtest/gtest.h>

#include <core/common/crypto/cryptoutils.hpp>

#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

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
        auto err = mCryptoFactory.Init();
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

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

} // namespace aos::crypto
