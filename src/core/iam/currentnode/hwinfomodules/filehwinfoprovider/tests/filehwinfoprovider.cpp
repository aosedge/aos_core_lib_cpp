/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>

#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/iam/currentnode/hwinfomodules/filehwinfoprovider/filehwinfoprovider.hpp>

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

class FileHWInfoProviderTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();
        fs::Remove(cTestHwIDFile);

        mConfig.mHWIDPath = cTestHwIDFile;
    }

    void TearDown() override { fs::Remove(cTestHwIDFile); }

    void CreateHwIDFile(const String& content)
    {
        auto err = fs::WriteStringToFile(cTestHwIDFile, content, 0600);
        ASSERT_TRUE(err.IsNone()) << "Failed to create test hw ID file";
    }

    FileHWInfoProviderConfig mConfig;
    FileHWInfoProvider       mProvider;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(FileHWInfoProviderTest, GetHWID)
{
    CreateHwIDFile("hw-id-1234");

    ASSERT_TRUE(mProvider.Init(mConfig).IsNone());

    StaticString<cIDLen> hwID;

    ASSERT_TRUE(mProvider.GetHWID(hwID).IsNone());
    EXPECT_STREQ(hwID.CStr(), "hw-id-1234");
}

TEST_F(FileHWInfoProviderTest, TrailingWhitespacesAreTrimmed)
{
    CreateHwIDFile("hw-id-1234 \t\r\n\n");

    ASSERT_TRUE(mProvider.Init(mConfig).IsNone());

    StaticString<cIDLen> hwID;

    ASSERT_TRUE(mProvider.GetHWID(hwID).IsNone());
    EXPECT_STREQ(hwID.CStr(), "hw-id-1234");
}

TEST_F(FileHWInfoProviderTest, MaxLenHWID)
{
    const auto maxLenHwID = std::string(cIDLen, 'a');

    CreateHwIDFile(maxLenHwID.c_str());

    ASSERT_TRUE(mProvider.Init(mConfig).IsNone());

    StaticString<cIDLen> hwID;

    ASSERT_TRUE(mProvider.GetHWID(hwID).IsNone());
    EXPECT_EQ(maxLenHwID, hwID.CStr());
}

TEST_F(FileHWInfoProviderTest, InitFailsIfFileNotExist)
{
    EXPECT_FALSE(mProvider.Init(mConfig).IsNone());
}

TEST_F(FileHWInfoProviderTest, InitFailsIfFileEmpty)
{
    CreateHwIDFile("");

    EXPECT_TRUE(mProvider.Init(mConfig).Is(ErrorEnum::eInvalidArgument));
}

TEST_F(FileHWInfoProviderTest, InitFailsIfFileContainsOnlyWhitespaces)
{
    CreateHwIDFile(" \t\r\n");

    EXPECT_TRUE(mProvider.Init(mConfig).Is(ErrorEnum::eInvalidArgument));
}

TEST_F(FileHWInfoProviderTest, InitFailsIfHWIDTooLong)
{
    CreateHwIDFile(std::string(cIDLen + 1, 'a').c_str());

    EXPECT_FALSE(mProvider.Init(mConfig).IsNone());
}

TEST_F(FileHWInfoProviderTest, GetHWIDFailsIfOutputBufferTooSmall)
{
    CreateHwIDFile("hw-id-1234");

    ASSERT_TRUE(mProvider.Init(mConfig).IsNone());

    StaticString<4> hwID;

    EXPECT_FALSE(mProvider.GetHWID(hwID).IsNone());
}

} // namespace aos::iam::currentnode
