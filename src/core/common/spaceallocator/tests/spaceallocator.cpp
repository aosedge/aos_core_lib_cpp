/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <core/common/spaceallocator/spaceallocator.hpp>
#include <core/common/tests/mocks/fsmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/common/tools/thread.hpp>

using namespace testing;

namespace aos::spaceallocator {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cWaitTimeout  = 5 * Time::cSeconds;
constexpr auto cBlockTimeout = std::chrono::milliseconds(100);

/***********************************************************************************************************************
 * Helpers
 **********************************************************************************************************************/

class BlockingRemover {
public:
    RetWithError<size_t> Remove(size_t size)
    {
        UniqueLock lock {mMutex};

        mEntered = true;
        mCondVar.NotifyAll();

        if (auto err = mCondVar.Wait(lock, cWaitTimeout, [this] { return mReleased; }); !err.IsNone()) {
            return {0, err};
        }

        return {size, ErrorEnum::eNone};
    }

    Error WaitEntered()
    {
        UniqueLock lock {mMutex};

        return mCondVar.Wait(lock, cWaitTimeout, [this] { return mEntered; });
    }

    void Release()
    {
        {
            LockGuard lock {mMutex};

            mReleased = true;
        }

        mCondVar.NotifyAll();
    }

private:
    Mutex               mMutex;
    ConditionalVariable mCondVar;
    bool                mEntered {};
    bool                mReleased {};
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class SpaceallocatorTest : public Test {
protected:
    void TearDown()
    {
        // Clear global partition state between tests
        SpaceAllocator<1>::mPartitions.Clear();
        fs::RemoveAll(mPath);
    }

    HeapAllocator mAllocator;

    StrictMock<FSPlatformMock>       mPlatformFS;
    StrictMock<ItemRemoverMock>      mRemover;
    const String                     mPath       = "path";
    const size_t                     mLimit      = 0;
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

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit).IsNone());

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

    ASSERT_TRUE(allocator1.Init(mAllocator, mPath, mPlatformFS).IsNone());
    ASSERT_TRUE(allocator2.Init(mAllocator, mPath, mPlatformFS).IsNone());
    ASSERT_TRUE(allocator3.Init(mAllocator, mPath, mPlatformFS).IsNone());

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
        .WillRepeatedly(Return(RetWithError<size_t>(effectiveTotalSize, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, 100, &mRemover).IsNone());

    std::vector<std::string> removedFiles;
    EXPECT_CALL(mRemover, RemoveItem(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke([&removedFiles, &outdatedFiles](const String& id, const String&) {
            removedFiles.push_back(id.CStr());

            size_t size = 0;
            for (const auto& file : outdatedFiles) {
                if (file.name == id) {
                    size = file.size;
                    break;
                }
            }

            return RetWithError<size_t> {size, ErrorEnum::eNone};
        }));

    for (const auto& file : outdatedFiles) {
        ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem(file.name, "", file.timestamp).IsNone());
    }

    EXPECT_CALL(mPlatformFS, GetDirSize(mPath))
        .WillRepeatedly(Return(RetWithError<size_t>(totalOutdatedSize, ErrorEnum::eNone)));

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(effectiveTotalSize - totalOutdatedSize, ErrorEnum::eNone)));

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

    ASSERT_TRUE(mSpaceAllocator.RestoreOutdatedItem(outdatedFiles[0].name, "").IsNone());
    ASSERT_TRUE(mSpaceAllocator.RestoreOutdatedItem(outdatedFiles[4].name, "").IsNone());
    ASSERT_TRUE(mSpaceAllocator.RestoreOutdatedItem(outdatedFiles[5].name, "").IsNone());

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
        .WillRepeatedly(Return(RetWithError<size_t>(effectiveTotalSize, ErrorEnum::eNone)));

    SpaceAllocator<2> mSpaceAllocator;

    // Initialize allocator with 50% limit
    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, 50).IsNone());

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

TEST_F(SpaceallocatorTest, PartLimitPerAllocator)
{
    constexpr size_t cTotalSize = 100 * cKilobyte;

    SpaceAllocator<2> allocator1;
    SpaceAllocator<2> allocator2;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .Times(2)
        .WillRepeatedly(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));

    ASSERT_TRUE(allocator1.Init(mAllocator, mPath, mPlatformFS, 30).IsNone());
    ASSERT_TRUE(allocator2.Init(mAllocator, mPath, mPlatformFS, 20).IsNone());

    EXPECT_CALL(mPlatformFS, GetDirSize(mPath)).WillRepeatedly(Return(RetWithError<size_t>(0, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));

    // each allocator is limited by its own part, not by the sum of the parts registered on the partition

    auto [space1, err1] = allocator1.AllocateSpace(30 * cKilobyte);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    auto [space2, err2] = allocator1.AllocateSpace(1);
    EXPECT_EQ(err2, ErrorEnum::eNoMemory);

    auto [space3, err3] = allocator2.AllocateSpace(30 * cKilobyte);
    EXPECT_EQ(err3, ErrorEnum::eNoMemory);

    auto [space4, err4] = allocator2.AllocateSpace(20 * cKilobyte);
    ASSERT_TRUE(err4.IsNone());
    ASSERT_NE(space4.Get(), nullptr);

    ASSERT_TRUE(space1->Accept().IsNone());
    ASSERT_TRUE(space4->Accept().IsNone());

    ASSERT_TRUE(allocator1.Close().IsNone());
    ASSERT_TRUE(allocator2.Close().IsNone());
}

TEST_F(SpaceallocatorTest, PartLimitFollowsTotalSize)
{
    constexpr size_t cTotalSize      = 100 * cKilobyte;
    constexpr size_t cGrownTotalSize = 200 * cKilobyte;

    SpaceAllocator<2> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, 50).IsNone());

    EXPECT_CALL(mPlatformFS, GetDirSize(mPath)).WillRepeatedly(Return(RetWithError<size_t>(0, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cGrownTotalSize, ErrorEnum::eNone)));

    // the limit is taken from the total size read on the first allocation, not from the one read on init

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(50 * cKilobyte);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(1);
    EXPECT_EQ(err2, ErrorEnum::eNoMemory);

    ASSERT_TRUE(space1->Release().IsNone());

    // partition has grown while the allocator was running: the limit follows it

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cGrownTotalSize, ErrorEnum::eNone)));

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(100 * cKilobyte);
    ASSERT_TRUE(err3.IsNone());
    ASSERT_NE(space3.Get(), nullptr);

    ASSERT_TRUE(space3->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, CloseRemovesOwnPartLimit)
{
    SpaceAllocator<1> allocator1;
    SpaceAllocator<1> allocator2;
    SpaceAllocator<1> allocator3;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .Times(3)
        .WillRepeatedly(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(allocator1.Init(mAllocator, mPath, mPlatformFS, 60).IsNone());
    ASSERT_TRUE(allocator2.Init(mAllocator, mPath, mPlatformFS, 30).IsNone());

    ASSERT_TRUE(allocator1.Close().IsNone());

    // allocator2 still holds its part, so the partition has no room for another 80%

    EXPECT_EQ(allocator3.Init(mAllocator, mPath, mPlatformFS, 80), ErrorEnum::eNoMemory);
}

TEST_F(SpaceallocatorTest, ResizeSpace)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(mTotalSize, ErrorEnum::eNone)));

    const size_t initialSize = 256 * cKilobyte;

    auto [space, err] = mSpaceAllocator.AllocateSpace(initialSize);
    ASSERT_TRUE(err.IsNone());
    ASSERT_NE(space.Get(), nullptr);
    ASSERT_EQ(space->Size(), initialSize);

    const size_t newSize = 512 * cKilobyte;
    ASSERT_TRUE(space->Resize(newSize).IsNone());
    ASSERT_EQ(space->Size(), newSize);

    const size_t smallerSize = 128 * cKilobyte;
    ASSERT_TRUE(space->Resize(smallerSize).IsNone());
    ASSERT_EQ(space->Size(), smallerSize);

    ASSERT_TRUE(space->Resize(smallerSize).IsNone());
    ASSERT_EQ(space->Size(), smallerSize);

    EXPECT_TRUE(space->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, ResizeSpaceEviction)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(mTotalSize, ErrorEnum::eNone)));

    // Leave only 100K free after initial allocation
    const size_t initialSize = mTotalSize - 100 * cKilobyte;

    auto [space, err] = mSpaceAllocator.AllocateSpace(initialSize);
    ASSERT_TRUE(err.IsNone());
    ASSERT_NE(space.Get(), nullptr);

    const size_t outdatedItemSize = 256 * cKilobyte;

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("file1", "", Time::Now()).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("file1"), String("")))
        .WillOnce(Return(RetWithError<size_t>(outdatedItemSize, ErrorEnum::eNone)));

    // Resize needs 200K delta but only 100K available — requires evicting "file1"
    const size_t newSize = initialSize + 200 * cKilobyte;

    ASSERT_TRUE(space->Resize(newSize).IsNone());
    ASSERT_EQ(space->Size(), newSize);

    ASSERT_TRUE(space->Accept().IsNone());
    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, ResizeSpaceInsufficientSpace)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(mTotalSize, ErrorEnum::eNone)));

    // Leave only 100K free after initial allocation
    const size_t initialSize = mTotalSize - 100 * cKilobyte;

    auto [space, err] = mSpaceAllocator.AllocateSpace(initialSize);
    ASSERT_TRUE(err.IsNone());
    ASSERT_NE(space.Get(), nullptr);

    // Resize needs 200K but only 100K available with no outdated items to evict
    const size_t newSize = initialSize + 200 * cKilobyte;

    ASSERT_EQ(space->Resize(newSize), ErrorEnum::eNoMemory);
    ASSERT_EQ(space->Size(), initialSize);

    ASSERT_TRUE(space->Release().IsNone());
    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, OutdatedItemsMultipleVersions)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(0, ErrorEnum::eNone)));

    const Time   now      = Time::Now();
    const size_t itemSize = 128 * cKilobyte;

    // both versions of the same item should be tracked independently

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "2.0.0", now.Add(-1 * Time::cHours)).IsNone());
    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", now.Add(-2 * Time::cHours)).IsNone());

    {
        InSequence seq;

        EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
            .WillOnce(Return(RetWithError<size_t>(itemSize, ErrorEnum::eNone)));
        EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("2.0.0")))
            .WillOnce(Return(RetWithError<size_t>(itemSize, ErrorEnum::eNone)));
    }

    // the oldest version is removed first

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(itemSize);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);
    ASSERT_TRUE(space1->Accept().IsNone());

    // then the newer one

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(itemSize);
    ASSERT_TRUE(err2.IsNone());
    ASSERT_NE(space2.Get(), nullptr);
    ASSERT_TRUE(space2->Accept().IsNone());

    // no outdated items left

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(itemSize);
    EXPECT_EQ(err3, ErrorEnum::eNoMemory);
    EXPECT_EQ(space3.Get(), nullptr);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RestoreOutdatedItemVersion)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(0, ErrorEnum::eNone)));

    const Time   now      = Time::Now();
    const size_t itemSize = 128 * cKilobyte;

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", now.Add(-2 * Time::cHours)).IsNone());
    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "2.0.0", now.Add(-1 * Time::cHours)).IsNone());

    // restoring one version should keep the other one outdated

    ASSERT_TRUE(mSpaceAllocator.RestoreOutdatedItem("item1", "1.0.0").IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("2.0.0")))
        .WillOnce(Return(RetWithError<size_t>(itemSize, ErrorEnum::eNone)));

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(itemSize);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);
    ASSERT_TRUE(space1->Accept().IsNone());

    // restored version must not be removed

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(itemSize);
    EXPECT_EQ(err2, ErrorEnum::eNoMemory);
    EXPECT_EQ(space2.Get(), nullptr);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, ReAddOutdatedItemUpdatesExisting)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(0, ErrorEnum::eNone)));

    const Time   now      = Time::Now();
    const size_t itemSize = 128 * cKilobyte;

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", now.Add(-1 * Time::cHours)).IsNone());
    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item2", "1.0.0", now.Add(-2 * Time::cHours)).IsNone());

    // re-adding the same ID and version updates the timestamp instead of adding a duplicate

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", now.Add(-3 * Time::cHours)).IsNone());

    {
        InSequence seq;

        EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
            .WillOnce(Return(RetWithError<size_t>(itemSize, ErrorEnum::eNone)));
        EXPECT_CALL(mRemover, RemoveItem(String("item2"), String("1.0.0")))
            .WillOnce(Return(RetWithError<size_t>(itemSize, ErrorEnum::eNone)));
    }

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(itemSize);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);
    ASSERT_TRUE(space1->Accept().IsNone());

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(itemSize);
    ASSERT_TRUE(err2.IsNone());
    ASSERT_NE(space2.Get(), nullptr);
    ASSERT_TRUE(space2->Accept().IsNone());

    // no duplicate of item1 is left

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(itemSize);
    EXPECT_EQ(err3, ErrorEnum::eNoMemory);
    EXPECT_EQ(space3.Get(), nullptr);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemNoItems)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(_, _)).Times(0);

    auto [size, err] = mSpaceAllocator.RemoveOutdatedItem();
    EXPECT_EQ(err, ErrorEnum::eNotFound);
    EXPECT_EQ(size, 0U);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemOldestFirst)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    const Time now = Time::Now();

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", now.Add(-1 * Time::cHours)).IsNone());
    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item2", "1.0.0", now.Add(-3 * Time::cHours)).IsNone());
    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item3", "1.0.0", now.Add(-2 * Time::cHours)).IsNone());

    {
        InSequence seq;

        EXPECT_CALL(mRemover, RemoveItem(String("item2"), String("1.0.0")))
            .WillOnce(Return(RetWithError<size_t>(32 * cKilobyte, ErrorEnum::eNone)));
        EXPECT_CALL(mRemover, RemoveItem(String("item3"), String("1.0.0")))
            .WillOnce(Return(RetWithError<size_t>(64 * cKilobyte, ErrorEnum::eNone)));
        EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
            .WillOnce(Return(RetWithError<size_t>(128 * cKilobyte, ErrorEnum::eNone)));
    }

    // items are removed one by one starting from the oldest one

    auto [size1, err1] = mSpaceAllocator.RemoveOutdatedItem();
    ASSERT_TRUE(err1.IsNone());
    EXPECT_EQ(size1, 32 * cKilobyte);

    auto [size2, err2] = mSpaceAllocator.RemoveOutdatedItem();
    ASSERT_TRUE(err2.IsNone());
    EXPECT_EQ(size2, 64 * cKilobyte);

    auto [size3, err3] = mSpaceAllocator.RemoveOutdatedItem();
    ASSERT_TRUE(err3.IsNone());
    EXPECT_EQ(size3, 128 * cKilobyte);

    // no outdated items left

    auto [size4, err4] = mSpaceAllocator.RemoveOutdatedItem();
    EXPECT_EQ(err4, ErrorEnum::eNotFound);
    EXPECT_EQ(size4, 0U);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemRemoverErrorPartition)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    // no allocator limit: only the partition accounting is checked
    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(30 * cKilobyte, ErrorEnum::eNone)));

    // space1 is not accepted on purpose: the partition available size is not re-read from the file system

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(25 * cKilobyte);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", Time::Now()).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
        .WillOnce(Return(RetWithError<size_t>(64 * cKilobyte, ErrorEnum::eFailed)));

    // remover error is returned and the failed item is dropped from the outdated items

    auto [size1, err2] = mSpaceAllocator.RemoveOutdatedItem();
    EXPECT_EQ(err2, ErrorEnum::eFailed);
    EXPECT_EQ(size1, 0U);

    auto [size2, err3] = mSpaceAllocator.RemoveOutdatedItem();
    EXPECT_EQ(err3, ErrorEnum::eNotFound);
    EXPECT_EQ(size2, 0U);

    // 5K is available: 10K doesn't fit, the size returned with the error must not be accounted

    auto [space2, err4] = mSpaceAllocator.AllocateSpace(10 * cKilobyte);
    EXPECT_EQ(err4, ErrorEnum::eNoMemory);
    EXPECT_EQ(space2.Get(), nullptr);

    ASSERT_TRUE(space1->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemRemoverErrorAllocator)
{
    constexpr size_t cTotalSize = 100 * cKilobyte;

    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    // allocator limit is 50K
    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, 50, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetDirSize(mPath))
        .WillRepeatedly(Return(RetWithError<size_t>(20 * cKilobyte, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));

    // space1 is not accepted on purpose: the allocated size is not re-read from the file system

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(25 * cKilobyte);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", Time::Now()).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
        .WillOnce(Return(RetWithError<size_t>(64 * cKilobyte, ErrorEnum::eFailed)));

    // remover error is returned and the failed item is dropped from the outdated items

    auto [size1, err2] = mSpaceAllocator.RemoveOutdatedItem();
    EXPECT_EQ(err2, ErrorEnum::eFailed);
    EXPECT_EQ(size1, 0U);

    auto [size2, err3] = mSpaceAllocator.RemoveOutdatedItem();
    EXPECT_EQ(err3, ErrorEnum::eNotFound);
    EXPECT_EQ(size2, 0U);

    // 45K is allocated: 10K more exceeds the limit, the size returned with the error must not be accounted

    auto [space2, err4] = mSpaceAllocator.AllocateSpace(10 * cKilobyte);
    EXPECT_EQ(err4, ErrorEnum::eNoMemory);
    EXPECT_EQ(space2.Get(), nullptr);

    ASSERT_TRUE(space1->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemOtherAllocatorUntouched)
{
    SpaceAllocator<5> allocator1;
    SpaceAllocator<5> allocator2;

    StrictMock<ItemRemoverMock> remover2;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .Times(2)
        .WillRepeatedly(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(allocator1.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());
    ASSERT_TRUE(allocator2.Init(mAllocator, mPath, mPlatformFS, mLimit, &remover2).IsNone());

    const Time now = Time::Now();

    // allocator2 item is older but belongs to another allocator sharing the partition

    ASSERT_TRUE(allocator1.AddOutdatedItem("item1", "1.0.0", now.Add(-1 * Time::cHours)).IsNone());
    ASSERT_TRUE(allocator2.AddOutdatedItem("item2", "1.0.0", now.Add(-2 * Time::cHours)).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
        .WillOnce(Return(RetWithError<size_t>(32 * cKilobyte, ErrorEnum::eNone)));

    auto [size1, err1] = allocator1.RemoveOutdatedItem();
    ASSERT_TRUE(err1.IsNone());
    EXPECT_EQ(size1, 32 * cKilobyte);

    auto [size2, err2] = allocator1.RemoveOutdatedItem();
    EXPECT_EQ(err2, ErrorEnum::eNotFound);
    EXPECT_EQ(size2, 0U);

    // allocator2 item is still there

    EXPECT_CALL(remover2, RemoveItem(String("item2"), String("1.0.0")))
        .WillOnce(Return(RetWithError<size_t>(64 * cKilobyte, ErrorEnum::eNone)));

    auto [size3, err3] = allocator2.RemoveOutdatedItem();
    ASSERT_TRUE(err3.IsNone());
    EXPECT_EQ(size3, 64 * cKilobyte);

    auto [size4, err4] = allocator2.RemoveOutdatedItem();
    EXPECT_EQ(err4, ErrorEnum::eNotFound);
    EXPECT_EQ(size4, 0U);

    ASSERT_TRUE(allocator1.Close().IsNone());
    ASSERT_TRUE(allocator2.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemFreesSpace)
{
    constexpr size_t cTotalSize = 100 * cKilobyte;

    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    // allocator limit is 50K
    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, 50, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetDirSize(mPath))
        .WillRepeatedly(Return(RetWithError<size_t>(20 * cKilobyte, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(25 * cKilobyte);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    // 45K is allocated: 10K more exceeds the limit

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(10 * cKilobyte);
    EXPECT_EQ(err2, ErrorEnum::eNoMemory);
    EXPECT_EQ(space2.Get(), nullptr);

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", Time::Now()).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
        .WillOnce(Return(RetWithError<size_t>(15 * cKilobyte, ErrorEnum::eNone)));

    auto [size, err] = mSpaceAllocator.RemoveOutdatedItem();
    ASSERT_TRUE(err.IsNone());
    EXPECT_EQ(size, 15 * cKilobyte);

    // freed size is taken into account without FreeSpace call

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(10 * cKilobyte);
    ASSERT_TRUE(err3.IsNone());
    ASSERT_NE(space3.Get(), nullptr);

    // 40K is allocated: 15K more exceeds the limit, the freed size must not be accounted twice

    auto [space4, err4] = mSpaceAllocator.AllocateSpace(15 * cKilobyte);
    EXPECT_EQ(err4, ErrorEnum::eNoMemory);
    EXPECT_EQ(space4.Get(), nullptr);

    // space1 is accepted only here on purpose: while it is not accepted, the allocated size is not re-read from the
    // file system, so the allocator accounting is checked

    ASSERT_TRUE(space3->Accept().IsNone());
    ASSERT_TRUE(space1->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemFreesPartitionSpace)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    // no allocator limit: only the partition accounting is checked
    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(30 * cKilobyte, ErrorEnum::eNone)));

    // space1 is not accepted on purpose: the partition available size is not re-read from the file system

    auto [space1, err1] = mSpaceAllocator.AllocateSpace(25 * cKilobyte);
    ASSERT_TRUE(err1.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    // 5K is available: 10K doesn't fit

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(10 * cKilobyte);
    EXPECT_EQ(err2, ErrorEnum::eNoMemory);
    EXPECT_EQ(space2.Get(), nullptr);

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", Time::Now()).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
        .WillOnce(Return(RetWithError<size_t>(15 * cKilobyte, ErrorEnum::eNone)));

    auto [size, err] = mSpaceAllocator.RemoveOutdatedItem();
    ASSERT_TRUE(err.IsNone());
    EXPECT_EQ(size, 15 * cKilobyte);

    // 20K is available now

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(10 * cKilobyte);
    ASSERT_TRUE(err3.IsNone());
    ASSERT_NE(space3.Get(), nullptr);

    // 10K is available: 15K doesn't fit, the freed size must not be accounted twice

    auto [space4, err4] = mSpaceAllocator.AllocateSpace(15 * cKilobyte);
    EXPECT_EQ(err4, ErrorEnum::eNoMemory);
    EXPECT_EQ(space4.Get(), nullptr);

    ASSERT_TRUE(space3->Accept().IsNone());
    ASSERT_TRUE(space1->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemAfterRestore)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", Time::Now()).IsNone());
    ASSERT_TRUE(mSpaceAllocator.RestoreOutdatedItem("item1", "1.0.0").IsNone());

    // restored item must not be removed

    EXPECT_CALL(mRemover, RemoveItem(_, _)).Times(0);

    auto [size, err] = mSpaceAllocator.RemoveOutdatedItem();
    EXPECT_EQ(err, ErrorEnum::eNotFound);
    EXPECT_EQ(size, 0U);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemAfterEviction)
{
    SpaceAllocator<5> mSpaceAllocator;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());

    const Time now = Time::Now();

    // item0 is the oldest one

    for (size_t i = 0; i < Partition::cMaxNumOutdatedItems; i++) {
        const auto id = "item" + std::to_string(i);

        ASSERT_TRUE(mSpaceAllocator
                        .AddOutdatedItem(id.c_str(), "1.0.0",
                            now.Add(-static_cast<int64_t>(Partition::cMaxNumOutdatedItems - i) * Time::cMinutes))
                        .IsNone());
    }

    {
        InSequence seq;

        // the outdated items list is full: the oldest item is evicted on adding a new one
        EXPECT_CALL(mRemover, RemoveItem(String("item0"), String("1.0.0")))
            .WillOnce(Return(RetWithError<size_t>(32 * cKilobyte, ErrorEnum::eNone)));
        // the next oldest item is removed after eviction
        EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
            .WillOnce(Return(RetWithError<size_t>(64 * cKilobyte, ErrorEnum::eNone)));
    }

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("itemNew", "1.0.0", now).IsNone());

    auto [size, err] = mSpaceAllocator.RemoveOutdatedItem();
    ASSERT_TRUE(err.IsNone());
    EXPECT_EQ(size, 64 * cKilobyte);

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, CloseClearsOutdatedItems)
{
    SpaceAllocator<5> allocator1;
    SpaceAllocator<5> allocator2;

    StrictMock<ItemRemoverMock> remover2;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .Times(2)
        .WillRepeatedly(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(allocator1.Init(mAllocator, mPath, mPlatformFS, mLimit, &mRemover).IsNone());
    ASSERT_TRUE(allocator2.Init(mAllocator, mPath, mPlatformFS, mLimit, &remover2).IsNone());

    ASSERT_TRUE(allocator1.AddOutdatedItem("item1", "1.0.0", Time::Now()).IsNone());

    ASSERT_TRUE(allocator1.Close().IsNone());

    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillOnce(Return(RetWithError<size_t>(10 * cKilobyte, ErrorEnum::eNone)));

    auto [space, err] = allocator2.AllocateSpace(20 * cKilobyte);
    EXPECT_EQ(err, ErrorEnum::eNoMemory);
    EXPECT_EQ(space.Get(), nullptr);

    auto [size, removeErr] = allocator2.RemoveOutdatedItem();
    EXPECT_EQ(removeErr, ErrorEnum::eNotFound);
    EXPECT_EQ(size, 0U);

    ASSERT_TRUE(allocator2.Close().IsNone());
}

TEST_F(SpaceallocatorTest, RemoveOutdatedItemAccountingIsAtomic)
{
    constexpr size_t cTotalSize = 100 * cKilobyte;
    constexpr size_t cItemSize  = 15 * cKilobyte;

    SpaceAllocator<5> mSpaceAllocator;
    BlockingRemover   blockingRemover;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, 50, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetDirSize(mPath))
        .WillRepeatedly(Return(RetWithError<size_t>(25 * cKilobyte, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("item1", "1.0.0", Time::Now()).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("item1"), String("1.0.0")))
        .WillOnce(
            Invoke([&blockingRemover](const String&, const String&) { return blockingRemover.Remove(cItemSize); }));

    size_t   removedSize = 0;
    Error    removeErr;
    Thread<> removeThread;

    ASSERT_TRUE(removeThread
                    .Run([&](void*) {
                        auto [size, err] = mSpaceAllocator.RemoveOutdatedItem();

                        removedSize = size;
                        removeErr   = err;
                    })
                    .IsNone());

    EXPECT_TRUE(blockingRemover.WaitEntered().IsNone());

    std::atomic_bool    allocated {false};
    UniquePtr<SpaceItf> space1;
    Error               allocateErr;
    Thread<>            allocateThread;

    EXPECT_TRUE(allocateThread
                    .Run([&](void*) {
                        auto [space, err] = mSpaceAllocator.AllocateSpace(10 * cKilobyte);

                        space1      = Move(space);
                        allocateErr = err;
                        allocated   = true;
                    })
                    .IsNone());

    std::this_thread::sleep_for(cBlockTimeout);

    EXPECT_FALSE(allocated.load());

    blockingRemover.Release();

    ASSERT_TRUE(removeThread.Join().IsNone());
    ASSERT_TRUE(allocateThread.Join().IsNone());

    ASSERT_TRUE(removeErr.IsNone());
    EXPECT_EQ(removedSize, cItemSize);

    ASSERT_TRUE(allocateErr.IsNone());
    ASSERT_NE(space1.Get(), nullptr);

    auto [space2, err2] = mSpaceAllocator.AllocateSpace(15 * cKilobyte + 1);
    EXPECT_EQ(err2, ErrorEnum::eNoMemory);
    EXPECT_EQ(space2.Get(), nullptr);

    auto [space3, err3] = mSpaceAllocator.AllocateSpace(15 * cKilobyte);
    ASSERT_TRUE(err3.IsNone());
    ASSERT_NE(space3.Get(), nullptr);

    ASSERT_TRUE(space3->Accept().IsNone());
    ASSERT_TRUE(space1->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

TEST_F(SpaceallocatorTest, OutdatedItemsListIsConsistentDuringEviction)
{
    constexpr size_t cTotalSize = 100 * cKilobyte;
    constexpr size_t cItemSize  = 15 * cKilobyte;

    SpaceAllocator<5> mSpaceAllocator;
    BlockingRemover   blockingRemover;

    EXPECT_CALL(mPlatformFS, GetMountPoint(mPath))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>(mMountPoint, ErrorEnum::eNone)));

    ASSERT_TRUE(mSpaceAllocator.Init(mAllocator, mPath, mPlatformFS, 50, &mRemover).IsNone());

    EXPECT_CALL(mPlatformFS, GetTotalSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetDirSize(mPath))
        .WillRepeatedly(Return(RetWithError<size_t>(45 * cKilobyte, ErrorEnum::eNone)));
    EXPECT_CALL(mPlatformFS, GetAvailableSize(mMountPoint))
        .WillRepeatedly(Return(RetWithError<size_t>(cTotalSize, ErrorEnum::eNone)));

    const Time now = Time::Now();

    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("itemA", "1.0.0", now.Add(-2 * Time::cHours)).IsNone());
    ASSERT_TRUE(mSpaceAllocator.AddOutdatedItem("itemB", "1.0.0", now.Add(-1 * Time::cHours)).IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("itemA"), String("1.0.0")))
        .WillOnce(
            Invoke([&blockingRemover](const String&, const String&) { return blockingRemover.Remove(cItemSize); }));

    UniquePtr<SpaceItf> space;
    Error               allocateErr;
    Thread<>            allocateThread;

    ASSERT_TRUE(allocateThread
                    .Run([&](void*) {
                        auto [allocatedSpace, err] = mSpaceAllocator.AllocateSpace(10 * cKilobyte);

                        space       = Move(allocatedSpace);
                        allocateErr = err;
                    })
                    .IsNone());

    EXPECT_TRUE(blockingRemover.WaitEntered().IsNone());

    std::atomic_bool updated {false};
    Error            restoreErr;
    Error            addErr;
    Thread<>         updateThread;

    EXPECT_TRUE(updateThread
                    .Run([&](void*) {
                        restoreErr = mSpaceAllocator.RestoreOutdatedItem("itemB", "1.0.0");
                        addErr     = mSpaceAllocator.AddOutdatedItem("itemC", "1.0.0", now);
                        updated    = true;
                    })
                    .IsNone());

    std::this_thread::sleep_for(cBlockTimeout);

    EXPECT_FALSE(updated.load());

    blockingRemover.Release();

    ASSERT_TRUE(allocateThread.Join().IsNone());
    ASSERT_TRUE(updateThread.Join().IsNone());

    ASSERT_TRUE(allocateErr.IsNone());
    ASSERT_NE(space.Get(), nullptr);

    ASSERT_TRUE(restoreErr.IsNone());
    ASSERT_TRUE(addErr.IsNone());

    EXPECT_CALL(mRemover, RemoveItem(String("itemC"), String("1.0.0")))
        .WillOnce(Return(RetWithError<size_t>(64 * cKilobyte, ErrorEnum::eNone)));

    auto [size1, err1] = mSpaceAllocator.RemoveOutdatedItem();
    ASSERT_TRUE(err1.IsNone());
    EXPECT_EQ(size1, 64 * cKilobyte);

    auto [size2, err2] = mSpaceAllocator.RemoveOutdatedItem();
    EXPECT_EQ(err2, ErrorEnum::eNotFound);
    EXPECT_EQ(size2, 0U);

    ASSERT_TRUE(space->Accept().IsNone());

    ASSERT_TRUE(mSpaceAllocator.Close().IsNone());
}

} // namespace aos::spaceallocator
