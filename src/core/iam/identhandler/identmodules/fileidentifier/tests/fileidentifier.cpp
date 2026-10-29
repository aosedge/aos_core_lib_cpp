/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>
#include <vector>

#include <gmock/gmock.h>

#include <core/common/tests/mocks/identprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/fs.hpp>

#include <core/iam/identhandler/identmodules/fileidentifier/fileidentifier.hpp>

using namespace testing;

namespace aos::iam::identhandler {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

constexpr auto cUnitModelPath = "unitModel";
constexpr auto cVersionPath   = "unitVersion";
constexpr auto cSubjectsPath  = "subjects";
constexpr auto cUnitModel     = "unitModel";
constexpr auto cUnitVersion   = "1.0.0";
constexpr auto cSubjects      = R"(subject1
subject2
subject3)";

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class FileIdentifierTest : public testing::Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        if (std::ofstream f(cUnitModelPath); f) {
            f << cUnitModel << ";" << cUnitVersion;
        }

        if (std::ofstream f(cSubjectsPath); f) {
            f << cSubjects;
        }

        mConfig.mUnitModelPath = cUnitModelPath;
        mConfig.mSubjectsPath  = cSubjectsPath;
    }

    FileIdentifierConfig mConfig;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(FileIdentifierTest, InitFailsOnEmptyConfig)
{
    FileIdentifier identifier;

    const auto err = identifier.Init(FileIdentifierConfig {});
    ASSERT_FALSE(err.IsNone()) << err.Message();
}

TEST_F(FileIdentifierTest, InitFailsOnUnitVersionFileError)
{
    const std::vector<std::string> cUnitModelFileContents = {
        {std::string("")},
        {std::string("noVersion")},
        {std::string("unitModel;").append(std::string(cVersionLen + 1, 'a'))},
    };

    for (const auto& fileContents : cUnitModelFileContents) {
        FileIdentifier identifier;

        if (std::ofstream f(cUnitModelPath); f) {
            f << fileContents;
        }

        const auto err = identifier.Init(mConfig);
        EXPECT_FALSE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }
}

TEST_F(FileIdentifierTest, InitFailsOnUnitModelFileMissing)
{
    FileIdentifier identifier;

    fs::Remove(cUnitModelPath);

    auto err = identifier.Init(mConfig);
    ASSERT_EQ(err.Value(), ErrorEnum::eRuntime) << tests::utils::ErrorToStr(err);
}

TEST_F(FileIdentifierTest, InitSucceedsOnSubjectsFileMissing)
{
    FileIdentifier identifier;

    fs::Remove(cSubjectsPath);

    auto err = identifier.Init(mConfig);
    ASSERT_EQ(err.Value(), ErrorEnum::eNone) << tests::utils::ErrorToStr(err);
}

TEST_F(FileIdentifierTest, EmptySubjectsOnSubjectsCountExceedsAppLimit)
{
    FileIdentifier identifier;

    if (std::ofstream f(cSubjectsPath); f) {
        for (size_t i = 0; i < cMaxNumSubjects + 1; ++i) {
            f << "subject" << i << std::endl;
        }
    }

    auto err = identifier.Init(mConfig);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> subjects;

    err = identifier.GetSubjects(subjects);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(subjects.IsEmpty()) << "Expected empty subjects array, but got size: " << subjects.Size();
}

TEST_F(FileIdentifierTest, EmptySubjectsOnSubjectLenExceedsAppLimit)
{
    FileIdentifier identifier;

    if (std::ofstream f(cSubjectsPath); f) {
        f << "subject" << std::string(cIDLen, 'a') << std::endl;
    }

    auto err = identifier.Init(mConfig);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> subjects;

    err = identifier.GetSubjects(subjects);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(subjects.IsEmpty()) << "Expected empty subjects array, but got size: " << subjects.Size();
}

TEST_F(FileIdentifierTest, GetUnitModelInfo)
{
    FileIdentifier identifier;

    auto err = identifier.Init(mConfig);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    UnitModelInfo unitModelInfo;

    err = identifier.GetUnitModelInfo(unitModelInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_STREQ(unitModelInfo.mUnitModel.CStr(), cUnitModel);
    ASSERT_STREQ(unitModelInfo.mVersion.CStr(), cUnitVersion);
}

TEST_F(FileIdentifierTest, GetSubjects)
{
    FileIdentifier identifier;

    const auto err = identifier.Init(mConfig);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> subjects;

    const auto subjectsErr = identifier.GetSubjects(subjects);
    ASSERT_TRUE(subjectsErr.IsNone()) << tests::utils::ErrorToStr(subjectsErr);

    ASSERT_EQ(subjects.Size(), 3);
    ASSERT_STREQ(subjects[0].CStr(), "subject1");
    ASSERT_STREQ(subjects[1].CStr(), "subject2");
    ASSERT_STREQ(subjects[2].CStr(), "subject3");
}

TEST_F(FileIdentifierTest, SubscribeUnsubscribeListener)
{
    FileIdentifier identifier;

    auto err = identifier.Init(mConfig);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    iamclient::SubjectsListenerMock listener;

    err = identifier.SubscribeListener(listener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = identifier.SubscribeListener(listener);
    EXPECT_TRUE(err.Is(ErrorEnum::eAlreadyExist)) << tests::utils::ErrorToStr(err);

    err = identifier.UnsubscribeListener(listener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = identifier.UnsubscribeListener(listener);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

} // namespace aos::iam::identhandler
