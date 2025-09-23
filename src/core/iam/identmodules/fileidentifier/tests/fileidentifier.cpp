/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>

#include <gmock/gmock.h>

#include <core/common/tests/mocks/identprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include <core/iam/identmodules/fileidentifier/fileidentifier.hpp>

using namespace testing;

namespace aos::iam::identmodules::fileidentifier {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

constexpr auto cSystemIDPath  = "systemID";
constexpr auto cUnitModelPath = "unitModel";
constexpr auto cSubjectsPath  = "subjects";
constexpr auto cSystemID      = "systemID";
constexpr auto cUnitModel     = "unitModel";
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
        aos::tests::utils::InitLog();

        if (std::ofstream f(cSystemIDPath); f) {
            f << cSystemID;
        }

        if (std::ofstream f(cUnitModelPath); f) {
            f << cUnitModel;
        }

        if (std::ofstream f(cSubjectsPath); f) {
            f << cSubjects;
        }

        mConfig.mUnitModelPath = cUnitModelPath;
        mConfig.mSystemIDPath  = cSystemIDPath;
        mConfig.mSubjectsPath  = cSubjectsPath;
    }

    Config mConfig;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(FileIdentifierTest, InitFailsOnEmptyConfig)
{
    FileIdentifier identifier;

    const auto err = identifier.Init(Config {});
    ASSERT_FALSE(err.IsNone()) << err.Message();
}

TEST_F(FileIdentifierTest, InitFailsOnSystemIDFileMissing)
{
    FileIdentifier identifier;

    fs::Remove(cSystemIDPath);

    auto err = identifier.Init(mConfig);
    ASSERT_EQ(err.Value(), ErrorEnum::eRuntime) << tests::utils::ErrorToStr(err);
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

TEST_F(FileIdentifierTest, GetSystemID)
{
    FileIdentifier identifier;

    const auto err = identifier.Init(mConfig);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    const auto [systemID, systemIDErr] = identifier.GetSystemID();
    ASSERT_TRUE(systemIDErr.IsNone()) << tests::utils::ErrorToStr(systemIDErr);
    ASSERT_STREQ(systemID.CStr(), cSystemID);
}

TEST_F(FileIdentifierTest, GetUnitModel)
{
    FileIdentifier identifier;

    const auto err = identifier.Init(mConfig);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    const auto [unitModel, unitModelErr] = identifier.GetUnitModel();
    ASSERT_TRUE(unitModelErr.IsNone()) << tests::utils::ErrorToStr(unitModelErr);
    ASSERT_STREQ(unitModel.CStr(), cUnitModel);
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

} // namespace aos::iam::identmodules::fileidentifier
