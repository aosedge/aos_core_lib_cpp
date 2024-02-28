/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/tools/buffer.hpp"
#include "mocks/subjectsobservermock.hpp"
#include <gtest/gtest.h>

#include "aos/iam/identhandler/fileidentifier.hpp"

using namespace aos;
using namespace aos::iam::identhandler;
using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class FileIdentifierTest : public Test {
protected:
    static constexpr auto cSystemIdPath  = "systemd-id.txt";
    static constexpr auto cUnitModelPath = "unit-model.txt";
    static constexpr auto cSubjectsPath  = "subjects.txt";

    static constexpr auto cSystemIdContent  = "test-system-id";
    static constexpr auto cUnitModelContent = "test-unit-model";
    static constexpr auto cSubjectsContent  = "test-subject-1\ntest-subject-2";

    static const Config cDefaultConfig;

    void SetUp() override
    {
        FS::WriteStringToFile(cSystemIdPath, cSystemIdContent, 0600);
        FS::WriteStringToFile(cUnitModelPath, cUnitModelContent, 0600);
        FS::WriteStringToFile(cSubjectsPath, cSubjectsContent, 0600);
    }

    void TearDown() override
    {
        FS::Remove(cSystemIdPath);
        FS::Remove(cUnitModelPath);
        FS::Remove(cSubjectsPath);
    }

    SubjectsObserverMock mSubjectsObserver;

    FileIdenentifier mFileIdentifier;
};

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

const Config FileIdentifierTest::cDefaultConfig
    = {FileIdentifierTest::cSystemIdPath, FileIdentifierTest::cUnitModelPath, FileIdentifierTest::cSubjectsPath};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(FileIdentifierTest, ReadSystemIDFails)
{
    EXPECT_CALL(mSubjectsObserver, SubjectsChanged).Times(0);
    FS::Remove(cDefaultConfig.systemIDPath);

    const auto err = mFileIdentifier.Init(cDefaultConfig, mSubjectsObserver);
    ASSERT_EQ(err.Value(), Error::Enum::eRuntime) << err.Message();
    ASSERT_FALSE(err.IsNone()) << err.Message();
}

TEST_F(FileIdentifierTest, ReadUnitModelFails)
{
    EXPECT_CALL(mSubjectsObserver, SubjectsChanged).Times(0);
    FS::Remove(cDefaultConfig.unitModelPath);

    const auto err = mFileIdentifier.Init(cDefaultConfig, mSubjectsObserver);
    ASSERT_EQ(err.Value(), Error::Enum::eRuntime) << err.Message();
    ASSERT_FALSE(err.IsNone()) << err.Message();
}

TEST_F(FileIdentifierTest, ReadSubjectsFails)
{
    EXPECT_CALL(mSubjectsObserver, SubjectsChanged).Times(0);
    FS::Remove(cDefaultConfig.subjectsPath);

    const auto err = mFileIdentifier.Init(cDefaultConfig, mSubjectsObserver);
    ASSERT_EQ(err.Value(), Error::Enum::eNone) << err.Message();
    ASSERT_TRUE(err.IsNone()) << err.Message();
}

TEST_F(FileIdentifierTest, ReadSubjectsContainsMoreElementsThanExpected)
{
    EXPECT_CALL(mSubjectsObserver, SubjectsChanged).Times(0);

    StaticString<cSubjectIDLen * cMaxSubjectIDSize> subjects;
    for (size_t i {0}; i < cMaxSubjectIDSize + 1; ++i) {
        subjects.Append("subject\n");
    }

    FS::WriteStringToFile(cDefaultConfig.subjectsPath, subjects, 0600);

    const auto err = mFileIdentifier.Init(cDefaultConfig, mSubjectsObserver);
    ASSERT_TRUE(err.IsNone()) << err.Message();
}

TEST_F(FileIdentifierTest, GetSystemID)
{
    EXPECT_CALL(mSubjectsObserver, SubjectsChanged).Times(1);
    const auto err = mFileIdentifier.Init(cDefaultConfig, mSubjectsObserver);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    const auto systemIdResult = mFileIdentifier.GetSystemID();
    ASSERT_TRUE(systemIdResult.mError.IsNone()) << systemIdResult.mError.Message();

    ASSERT_EQ(systemIdResult.mValue, String(cSystemIdContent));
}

TEST_F(FileIdentifierTest, GetUnitModel)
{
    EXPECT_CALL(mSubjectsObserver, SubjectsChanged).Times(1);
    const auto err = mFileIdentifier.Init(cDefaultConfig, mSubjectsObserver);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    const auto unitModelResult = mFileIdentifier.GetUnitModel();
    ASSERT_TRUE(unitModelResult.mError.IsNone()) << unitModelResult.mError.Message();

    ASSERT_EQ(unitModelResult.mValue, String(cUnitModelContent));
}

TEST_F(FileIdentifierTest, GetSubjects)
{
    EXPECT_CALL(mSubjectsObserver, SubjectsChanged).Times(1);
    const auto err = mFileIdentifier.Init(cDefaultConfig, mSubjectsObserver);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    StaticArray<StaticString<cSubjectIDLen>, 2> subjects;
    const auto                                  subjectsResult = mFileIdentifier.GetSubjects(subjects);
    ASSERT_TRUE(subjectsResult.IsNone()) << subjectsResult.Message();

    ASSERT_EQ(subjects.Size(), 2);
}

TEST_F(FileIdentifierTest, GetSubjectsNoMemory)
{
    EXPECT_CALL(mSubjectsObserver, SubjectsChanged).Times(1);
    const auto err = mFileIdentifier.Init(cDefaultConfig, mSubjectsObserver);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    StaticArray<StaticString<cSubjectIDLen>, 1> subjects;
    const auto                                  subjectsResult = mFileIdentifier.GetSubjects(subjects);
    ASSERT_FALSE(subjectsResult.IsNone()) << subjectsResult.Message();
    ASSERT_TRUE(subjectsResult.Is(Error::Enum::eNoMemory)) << subjectsResult.Message();

    ASSERT_TRUE(subjects.IsEmpty());
}
