/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tests/mocks/blobinfoprovidermock.hpp>
#include <core/common/tests/mocks/fileinfoprovidermock.hpp>
#include <core/common/tests/mocks/fsmock.hpp>
#include <core/common/tests/mocks/itemstatuslistenermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>

#include <core/common/imagemanager/imagemanager.hpp>

#include "stubs/storagestub.hpp"

using namespace testing;

namespace aos::common::imagemanager {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestImagePath = "/tmp/common_imagemanager_test/images";
constexpr auto cTestDigest    = "sha256:a3b2c1d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90";

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

DeployableItemInfo CreateDeployableItem(
    const String& itemID, const String& version, ItemStateEnum state, const Error& error = ErrorEnum::eNone)
{
    DeployableItemInfo item;

    item.mItemID     = itemID;
    item.mType       = UpdateItemTypeEnum::eService;
    item.mVersion    = version;
    item.mRootDigest = cTestDigest;
    item.mState      = state;
    item.mTimestamp  = Time::Now();
    item.mError      = error;

    return item;
}

UpdateItemStatus CreateItemStatus(const String& itemID, const String& version, ItemStateEnum state)
{
    UpdateItemStatus status;

    status.mItemID  = itemID;
    status.mType    = UpdateItemTypeEnum::eService;
    status.mVersion = version;
    status.mState   = state;

    return status;
}

const UpdateItemStatus* FindStatus(const Array<UpdateItemStatus>& statuses, const String& itemID, const String& version)
{
    auto it = statuses.FindIf([&itemID, &version](const UpdateItemStatus& status) {
        return status.mItemID == itemID && status.mVersion == version;
    });

    return it != statuses.end() ? it : nullptr;
}

/***********************************************************************************************************************
 * Test image manager
 **********************************************************************************************************************/

class TestImageManager : public ImageManager {
public:
    using ImageManager::Init;
    using ImageManager::InstallItemInfo;

    Error Install(const InstallItemInfo& itemInfo) { return InstallItem(itemInfo); }
    Error Remove(const String& itemID, const String& version) { return RemoveItem(itemID, version); }

    MOCK_METHOD(Error, DownloadBlob, (const String& path, const BlobInfo& blobInfo), (override));
    MOCK_METHOD(Error, CancelDownload, (const String& digest), (override));
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CommonImageManagerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    Error InitImageManager()
    {
        Config config {cTestImagePath, 0, 10 * Time::cSeconds, 5 * Time::cSeconds};

        return mImageManager.Init(
            config, mBlobInfoProvider, mStorage, mSpaceAllocator, mFileInfoProvider, mOCISpec, mFSPlatform, mAllocator);
    }

    void AddStoredItem(
        const String& itemID, const String& version, ItemStateEnum state, const Error& error = ErrorEnum::eNone)
    {
        auto err = mStorage.AddDeployableItem(CreateDeployableItem(itemID, version, state, error));
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    HeapAllocator                                mAllocator;
    NiceMock<DownloadBlobInfoProviderMock>       mBlobInfoProvider;
    StorageStub                                  mStorage;
    NiceMock<spaceallocator::SpaceAllocatorMock> mSpaceAllocator;
    NiceMock<fs::FileInfoProviderMock>           mFileInfoProvider;
    NiceMock<oci::OCISpecMock>                   mOCISpec;
    NiceMock<FSPlatformMock>                     mFSPlatform;
    NiceMock<ItemStatusListenerMock>             mListener;
    NiceMock<TestImageManager>                   mImageManager;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CommonImageManagerTest, Init)
{
    auto err = InitImageManager();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, SubscribeListenerTwice)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    auto err = mImageManager.SubscribeListener(mListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.SubscribeListener(mListener);
    EXPECT_TRUE(err.Is(ErrorEnum::eAlreadyExist)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, UnsubscribeUnknownListener)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    auto err = mImageManager.UnsubscribeListener(mListener);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, SubscribeUnsubscribeListener)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    auto err = mImageManager.SubscribeListener(mListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.UnsubscribeListener(mListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.UnsubscribeListener(mListener);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    err = mImageManager.SubscribeListener(mListener);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, GetItemsStatusesEmpty)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    StaticArray<UpdateItemStatus, cMaxNumUpdateItems> statuses;

    auto err = mImageManager.GetItemsStatuses(statuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_TRUE(statuses.IsEmpty());
}

TEST_F(CommonImageManagerTest, GetItemsStatusesStoredStates)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    AddStoredItem("item1", "1.0.0", ItemStateEnum::eInstalled);
    AddStoredItem("item1", "2.0.0", ItemStateEnum::eRemoved);
    AddStoredItem("item2", "1.0.0", ItemStateEnum::eFailed, Error(ErrorEnum::eFailed, "download failed"));
    AddStoredItem("item3", "1.0.0", ItemStateEnum::eInstalling);

    StaticArray<UpdateItemStatus, cMaxNumUpdateItems> statuses;

    // The statuses are appended to the output array.
    const auto preFilledStatus = CreateItemStatus("item0", "1.0.0", ItemStateEnum::eInstalled);

    ASSERT_TRUE(statuses.PushBack(preFilledStatus).IsNone());

    auto err = mImageManager.GetItemsStatuses(statuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_EQ(statuses.Size(), 5);
    EXPECT_EQ(statuses[0], preFilledStatus);

    for (const auto& status : statuses) {
        EXPECT_EQ(status.mType, UpdateItemTypeEnum::eService);
        EXPECT_FALSE(status.mPreinstalled);
    }

    const auto* status = FindStatus(statuses, "item1", "1.0.0");
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->mState, ItemStateEnum::eInstalled);
    EXPECT_TRUE(status->mError.IsNone());

    status = FindStatus(statuses, "item1", "2.0.0");
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->mState, ItemStateEnum::eRemoved);
    EXPECT_TRUE(status->mError.IsNone());

    status = FindStatus(statuses, "item2", "1.0.0");
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(status->mError.Is(ErrorEnum::eFailed));
    EXPECT_STREQ(status->mError.Message(), "download failed");

    status = FindStatus(statuses, "item3", "1.0.0");
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->mState, ItemStateEnum::eInstalling);
    EXPECT_TRUE(status->mError.IsNone());
}

TEST_F(CommonImageManagerTest, GetItemsStatusesNoMemory)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    AddStoredItem("item1", "1.0.0", ItemStateEnum::eInstalled);
    AddStoredItem("item1", "2.0.0", ItemStateEnum::eInstalled);

    StaticArray<UpdateItemStatus, 1> statuses;

    auto err = mImageManager.GetItemsStatuses(statuses);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);
}

} // namespace aos::common::imagemanager
