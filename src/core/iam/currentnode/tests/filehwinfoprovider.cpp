/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>

#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/iam/currentnode/filehwinfoprovider.hpp>

using namespace testing;

namespace aos::iam::currentnode {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestHwIDFile = "/tmp/test_hw_id";

} // namespace

/***********************************************************************************************************************
 * Test fixture
 **********************************************************************************************************************/

class FileHwInfoProviderTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();
        fs::Remove(cTestHwIDFile);
    }

    void TearDown() override { fs::Remove(cTestHwIDFile); }

    void CreateHwIDFile(const String& content)
    {
        auto err = fs::WriteStringToFile(cTestHwIDFile, content, 0600);
        ASSERT_TRUE(err.IsNone()) << "Failed to create test hw ID file";
    }

    FileHwInfoProvider mProvider;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(FileHwInfoProviderTest, GetHardwareID)
{
    CreateHwIDFile("hw-id-1234");

    ASSERT_TRUE(mProvider.Init(cTestHwIDFile).IsNone());

    StaticString<cHardwareIDLen> hwID;

    ASSERT_TRUE(mProvider.GetHardwareID(hwID).IsNone());
    EXPECT_STREQ(hwID.CStr(), "hw-id-1234");
}

TEST_F(FileHwInfoProviderTest, TrailingWhitespacesAreTrimmed)
{
    CreateHwIDFile("hw-id-1234 \t\r\n\n");

    ASSERT_TRUE(mProvider.Init(cTestHwIDFile).IsNone());

    StaticString<cHardwareIDLen> hwID;

    ASSERT_TRUE(mProvider.GetHardwareID(hwID).IsNone());
    EXPECT_STREQ(hwID.CStr(), "hw-id-1234");
}

TEST_F(FileHwInfoProviderTest, MaxLenHardwareID)
{
    const auto maxLenHwID = std::string(cHardwareIDLen, 'a');

    CreateHwIDFile(maxLenHwID.c_str());

    ASSERT_TRUE(mProvider.Init(cTestHwIDFile).IsNone());

    StaticString<cHardwareIDLen> hwID;

    ASSERT_TRUE(mProvider.GetHardwareID(hwID).IsNone());
    EXPECT_EQ(maxLenHwID, hwID.CStr());
}

TEST_F(FileHwInfoProviderTest, InitFailsIfFileNotExist)
{
    EXPECT_FALSE(mProvider.Init(cTestHwIDFile).IsNone());
}

TEST_F(FileHwInfoProviderTest, InitFailsIfFileEmpty)
{
    CreateHwIDFile("");

    EXPECT_TRUE(mProvider.Init(cTestHwIDFile).Is(ErrorEnum::eInvalidArgument));
}

TEST_F(FileHwInfoProviderTest, InitFailsIfFileContainsOnlyWhitespaces)
{
    CreateHwIDFile(" \t\r\n");

    EXPECT_TRUE(mProvider.Init(cTestHwIDFile).Is(ErrorEnum::eInvalidArgument));
}

TEST_F(FileHwInfoProviderTest, InitFailsIfHardwareIDTooLong)
{
    CreateHwIDFile(std::string(cHardwareIDLen + 1, 'a').c_str());

    EXPECT_FALSE(mProvider.Init(cTestHwIDFile).IsNone());
}

TEST_F(FileHwInfoProviderTest, GetHardwareIDFailsIfOutputBufferTooSmall)
{
    CreateHwIDFile("hw-id-1234");

    ASSERT_TRUE(mProvider.Init(cTestHwIDFile).IsNone());

    StaticString<4> hwID;

    EXPECT_FALSE(mProvider.GetHardwareID(hwID).IsNone());
}

} // namespace aos::iam::currentnode
