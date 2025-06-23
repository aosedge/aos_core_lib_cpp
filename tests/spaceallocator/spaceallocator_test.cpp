/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/spaceallocator/spaceallocator.hpp"
#include "mocks/filesystemmock.hpp"
#include "mocks/spaceallocatormock.hpp"

using namespace testing;

namespace aos::spaceallocator {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class SpaceallocatorTest : public Test {
protected:
    void TearDown() { fs::RemoveAll(mPath); }

    StrictMock<FSPlatformMock>       mPlatformFS;
    StrictMock<ItemRemoverMock>      mRemover;
    const String                     mPath       = "path";
    const size_t                     mLimit      = 0;
    static constexpr auto            cKilobyte   = 1024;
    const size_t                     mTotalSize  = 1 * cKilobyte * cKilobyte;
    const StaticString<cFilePathLen> mMountPoint = "/mnt/test";
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(SpaceallocatorTest, AllocateSuccess)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(mTotalSize, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mPath, mPlatformFS, mLimit).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(mTotalSize, ErrorEnum::eNone)));

    const size_t size1 = 256 * cKilobyte;

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(size1);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    const size_t size2  = 512 * cKilobyte;
    auto [space2, err2] = mSpaceAllocator.AllocateSpace(size2);
    ASSERT_TRUE(err2.IsNone());
    ASSERT_NE(space2.Get(), nullptr);

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(512 * cKilobyte);
    ASSERT_FALSE(err3.IsNone());

    EXPECT_TRUE(space2->Release().IsNone());

    auto [space4, err4] = mSpaceAllocator.AllocateSpace(512 * cKilobyte);
    EXPECT_TRUE(err4.IsNone());
    ASSERT_NE(space4.Get(), nullptr);

    EXPECT_TRUE(space4->Accept().IsNone());
    EXPECT_TRUE(space1->Accept().IsNone());

    EXPECT_EQ(space1->Accept(), ErrorEnum::eNotFound);
    EXPECT_EQ(space1->Release(), ErrorEnum::eNotFound);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, MultipleAllocators)
{
    SpaceAllocator<1> allocator1;
    SpaceAllocator<1> allocator2;
    SpaceAllocator<2> allocator3;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .Times(3)
        .WillRepeatedly(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(mTotalSize, ErrorEnum::eNone)));

    ASSERT_TRUE(allocator1.Init(mPath, mPlatformFS).IsNone());
    ASSERT_TRUE(allocator2.Init(mPath, mPlatformFS).IsNone());
    ASSERT_TRUE(allocator3.Init(mPath, mPlatformFS).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(mTotalSize, ErrorEnum::eNone)));

    const size_t size1  = 256 * cKilobyte;
    auto [space1, err1] = allocator1.AllocateSpace(size1);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    const size_t size2  = 512 * cKilobyte;
    auto [space2, err2] = allocator2.AllocateSpace(size2);
    ASSERT_TRUE(err2.IsNone());
    ASSERT_NE(space2.Get(), nullptr);

    auto [space3, err3] = allocator3.AllocateSpace(512 * cKilobyte);
    ASSERT_FALSE(err3.IsNone());
    ASSERT_EQ(space3.Get(), nullptr);

    ASSERT_TRUE(space2->Release().IsNone());

    auto [space3New, err3New] = allocator3.AllocateSpace(512 * cKilobyte);
    ASSERT_TRUE(err3New.IsNone());
    ASSERT_NE(space3New.Get(), nullptr);

    ASSERT_TRUE(space3New->Accept().IsNone());
    ASSERT_TRUE(space1->Accept().IsNone());

    ASSERT_TRUE(allocator1.Close().IsNone());
    ASSERT_TRUE(allocator2.Close().IsNone());
    ASSERT_TRUE(allocator3.Close().IsNone());
}

TEST_F(SpaceallocatorTest, OutdatedItems)
{
    struct TestFile {
        String name;
        size_t size;
        Time   timestamp;
    };

    const Time now = Time::Now();

    // Total outdated files size 576 Kb
    std::vector<TestFile> outdatedFiles = {{"file1.data", 128 * cKilobyte, now.Add(-1 * Time::cHours)},
        {"file2.data", 32 * cKilobyte, now.Add(-6 * Time::cHours)},
        {"file3.data", 64 * cKilobyte, now.Add(-5 * Time::cHours)},
        {"file4.data", 256 * cKilobyte, now.Add(-4 * Time::cHours)},
        {"file5.data", 32 * cKilobyte, now.Add(-2 * Time::cHours)},
        {"file6.data", 256 * cKilobyte, now.Add(-3 * Time::cHours)}};

    size_t totalOutdatedSize = 0;
    for (const auto& file : outdatedFiles) {
        totalOutdatedSize += file.size;
    }

    // Add ~5% overhead for filesystem metadata
    totalOutdatedSize += totalOutdatedSize / 20;

    SpaceAllocator<2> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    // Reduce total size to account for filesystem overhead
    const size_t effectiveTotalSize = 1 * cKilobyte * cKilobyte - (50 * cKilobyte); // 1MB minus 50KB for fs overhead
    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(effectiveTotalSize, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mPath, mPlatformFS, 100, &mRemover).IsNone());

    std::vector<std::string> removedFiles;
    EXPECT_CALL(mRemover, RemoveItem(testing::_)).WillRepeatedly(testing::Invoke([&removedFiles](const String& id) {
        removedFiles.push_back(id.CStr());
        return ErrorEnum::eNone;
    }));

    for (const auto& file : outdatedFiles) {
        ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem(file.name, file.size, file.timestamp).IsNone());
    }

    EXPECT_CALL(mPlatformFS, GetDirSize(mPath))
        .WillOnce(Return(RetWithError<size_t>(totalOutdatedSize, ErrorEnum::eNone)));

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(effectiveTotalSize - totalOutdatedSize, ErrorEnum::eNone)));

    auto [space, err] = mSpaceAllocator.AllocateSpace(256 * cKilobyte);
    ASSERT_TRUE(err.IsNone());
    ASSERT_NE(space.Get(), nullptr);

    std::vector<String> expectedRemovedFiles = {outdatedFiles[1].name, outdatedFiles[2].name, outdatedFiles[3].name};
    EXPECT_EQ(removedFiles.size(), expectedRemovedFiles.size());
    for (size_t i = 0; i < removedFiles.size(); i++) {
        EXPECT_STREQ(removedFiles[i].c_str(), expectedRemovedFiles[i].CStr());
    }

    ASSERT_TRUE(space->Release().IsNone());

    size_t remainingSize = totalOutdatedSize;
    for (const auto& file : outdatedFiles) {
        if (file.name == "file2.data" || file.name == "file3.data" || file.name == "file4.data") {
            remainingSize -= file.size;
        }
    }

    // Add ~5% overhead for filesystem metadata
    remainingSize += remainingSize / 20;

    removedFiles.clear();

    EXPECT_CALL(mPlatformFS, GetDirSize(mPath)).WillOnce(Return(RetWithError<size_t>(remainingSize, ErrorEnum::eNone)));

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(1024 * cKilobyte);
    ASSERT_FALSE(err2.IsNone());
    ASSERT_EQ(space2.Get(), nullptr);
    EXPECT_TRUE(removedFiles.empty());

    ASSERT_TRUE(mSpaceAllocator.RestoreOutdatedItem(outdatedFiles[0].name).IsNone());
    ASSERT_TRUE(mSpaceAllocator.RestoreOutdatedItem(outdatedFiles[4].name).IsNone());
    ASSERT_TRUE(mSpaceAllocator.RestoreOutdatedItem(outdatedFiles[5].name).IsNone());

    EXPECT_CALL(mPlatformFS, GetDirSize(mPath)).WillOnce(Return(RetWithError<size_t>(remainingSize, ErrorEnum::eNone)));

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(512 * cKilobyte);
    ASSERT_FALSE(err3.IsNone());
    ASSERT_EQ(space3.Get(), nullptr);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, PartLimit)
{
    struct TestFile {
        String name;
        size_t size;
    };

    // Total exists files 224 Kb
    std::vector<TestFile> existFiles
        = {{"file1.data", 96 * cKilobyte}, {"file2.data", 32 * cKilobyte}, {"file3.data", 64 * cKilobyte}};

    size_t totalExistSize = 0;
    for (const auto& file : existFiles) {
        totalExistSize += file.size;
    }

    // Add ~5% overhead for filesystem metadata
    totalExistSize += totalExistSize / 20;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    // Total size is 1MB minus filesystem overhead
    const size_t effectiveTotalSize = 1 * cKilobyte * cKilobyte - (50 * cKilobyte);
    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(effectiveTotalSize, ErrorEnum::eNone)));

    SpaceAllocator<2> mSpaceAllocator;

    // Initialize allocator with 50% limit
    ASSERT_TRUE(mSpaceAllocator.Init(mPath, mPlatformFS, 50).IsNone());

    EXPECT_CALL(mPlatformFS, GetDirSize(mPath))
        .WillOnce(Return(RetWithError<size_t>(totalExistSize, ErrorEnum::eNone)));

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(effectiveTotalSize - totalExistSize, ErrorEnum::eNone)));

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(256 * cKilobyte);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(128 * cKilobyte);
    ASSERT_FALSE(err2.IsNone());
    ASSERT_EQ(space2.Get(), nullptr);

    mSpaceAllocator.FreeSpace(128 * cKilobyte);

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(128 * cKilobyte);
    ASSERT_TRUE(err3.IsNone());
    ASSERT_NE(space3.Get(), nullptr);

    ASSERT_TRUE(space3->Release().IsNone());
    ASSERT_TRUE(space1->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

} // namespace aos::spaceallocator
