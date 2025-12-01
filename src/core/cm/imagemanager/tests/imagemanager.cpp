/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <thread>

#include <core/cm/imagemanager/imagemanager.hpp>
#include <core/cm/tests/mocks/fileservermock.hpp>
#include <core/common/tests/mocks/cryptomock.hpp>
#include <core/common/tests/mocks/downloadermock.hpp>
#include <core/common/tests/mocks/fileinfoprovidermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tests/utils/log.hpp>

#include "mocks/blobinfoprovidermock.hpp"
#include "mocks/storagemock.hpp"

using namespace testing;

namespace aos::cm::imagemanager {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ImageManagerTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        tests::utils::InitLog();

        LOG_INF() << "Image manager size" << Log::Field("size", sizeof(ImageManager));
    }

    void SetUp() override
    {
        mConfig.mInstallPath  = "/tmp/imagemanager_test/install";
        mConfig.mDownloadPath = "/tmp/imagemanager_test/download";

        EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillRepeatedly(Return(ErrorEnum::eNone));

        EXPECT_TRUE(mImageManager
                        .Init(mConfig, mStorageMock, mBlobInfoProviderMock, mDownloadingSpaceAllocatorMock,
                            mInstallSpaceAllocatorMock, mDownloaderMock, mFileServerMock, mCryptoHelperMock,
                            mFileInfoProviderMock, mOCISpecMock)
                        .IsNone());
    }

    void TearDown() override
    {
        fs::RemoveAll(mConfig.mInstallPath);
        fs::RemoveAll(mConfig.mDownloadPath);
    }

    Config                                         mConfig;
    ImageManager                                   mImageManager;
    StaticAllocator<1024 * 5, 20>                  mAllocator;
    StrictMock<StorageMock>                        mStorageMock;
    StrictMock<BlobInfoProviderMock>               mBlobInfoProviderMock;
    StrictMock<spaceallocator::SpaceAllocatorMock> mDownloadingSpaceAllocatorMock;
    StrictMock<spaceallocator::SpaceAllocatorMock> mInstallSpaceAllocatorMock;
    StrictMock<downloader::DownloaderMock>         mDownloaderMock;
    StrictMock<fileserver::FileServerMock>         mFileServerMock;
    StrictMock<crypto::CryptoHelperMock>           mCryptoHelperMock;
    StrictMock<fs::FileInfoProviderMock>           mFileInfoProviderMock;
    StrictMock<spaceallocator::SpaceMock>          mSpaceMock;
    StrictMock<oci::OCISpecMock>                   mOCISpecMock;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ImageManagerTest, DownloadUpdateItems_EmptyList)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(statuses.Size(), 0);
}

TEST_F(ImageManagerTest, DownloadUpdateItems_Success_NewItem)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto&, Array<BlobInfo>& blobsInfo) {
            BlobInfo info;
            info.mDigest = "abc123";
            info.mSize   = 1024;
            info.mURLs.PushBack("http://test.com/blob");

            for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                info.mSHA256.PushBack(static_cast<uint8_t>(i));
            }

            blobsInfo.PushBack(info);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "manifest123";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            oci::ContentDescriptor layer;
            layer.mDigest = "layer123";
            manifest.mLayers.PushBack(layer);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mCryptoHelperMock, Decrypt(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mCryptoHelperMock, ValidateSigns(_, _, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _)).WillRepeatedly(Invoke([](const String&, fs::FileInfo& info) {
        for (size_t i = 0; i < crypto::cSHA256Size; i++) {
            info.mSHA256.PushBack(static_cast<uint8_t>(i));
        }

        info.mSize = 1024;

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, ItemState(ItemStateEnum::ePending)))
        .WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, item.mItemID);
    EXPECT_EQ(statuses[0].mVersion, item.mVersion);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::ePending);
}

TEST_F(ImageManagerTest, DownloadUpdateItems_AlreadyInstalled)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(Invoke([&](Array<ItemInfo>& items) {
        ItemInfo storedItem;
        storedItem.mItemID      = item.mItemID;
        storedItem.mVersion     = item.mVersion;
        storedItem.mIndexDigest = item.mIndexDigest;
        storedItem.mState       = ItemStateEnum::eInstalled;
        items.PushBack(storedItem);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto&, Array<BlobInfo>& blobsInfo) {
            BlobInfo info;
            info.mDigest = "abc123";
            info.mSize   = 1024;
            info.mURLs.PushBack("http://test.com/blob");

            for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                info.mSHA256.PushBack(static_cast<uint8_t>(i));
            }

            blobsInfo.PushBack(info);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _)).WillRepeatedly(Invoke([](const String&, fs::FileInfo& info) {
        for (size_t i = 0; i < crypto::cSHA256Size; i++) {
            info.mSHA256.PushBack(static_cast<uint8_t>(i));
        }

        info.mSize = 1024;

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mCryptoHelperMock, Decrypt(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mCryptoHelperMock, ValidateSigns(_, _, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "manifest123";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            oci::ContentDescriptor layer;
            layer.mDigest = "layer123";
            manifest.mLayers.PushBack(layer);

            return ErrorEnum::eNone;
        }));

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, item.mItemID);
    EXPECT_EQ(statuses[0].mVersion, item.mVersion);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalled);
}

TEST_F(ImageManagerTest, DownloadUpdateItems_MultipleItems_Success)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    for (int i = 0; i < 3; i++) {
        UpdateItemInfo item;
        item.mItemID      = ("service" + std::to_string(i)).c_str();
        item.mVersion     = "1.0.0";
        item.mIndexDigest = ("digest" + std::to_string(i)).c_str();
        itemsInfo.PushBack(item);
    }

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).Times(3).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto& digests, Array<BlobInfo>& blobsInfo) {
            BlobInfo info;
            info.mDigest = digests[0];
            info.mSize   = 1024;
            info.mURLs.PushBack("http://test.com/blob");

            for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                info.mSHA256.PushBack(static_cast<uint8_t>(i));
            }

            blobsInfo.PushBack(info);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "manifest";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            oci::ContentDescriptor layer;
            layer.mDigest = "layer";
            manifest.mLayers.PushBack(layer);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mCryptoHelperMock, Decrypt(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mCryptoHelperMock, ValidateSigns(_, _, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _)).WillRepeatedly(Invoke([](const String&, fs::FileInfo& info) {
        for (size_t i = 0; i < crypto::cSHA256Size; i++) {
            info.mSHA256.PushBack(static_cast<uint8_t>(i));
        }

        info.mSize = 1024;

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, ItemState(ItemStateEnum::ePending)))
        .Times(3)
        .WillRepeatedly(Return(ErrorEnum::eNone));

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 3);

    for (size_t i = 0; i < statuses.Size(); i++) {
        EXPECT_EQ(statuses[i].mItemID, itemsInfo[i].mItemID);
        EXPECT_EQ(statuses[i].mVersion, itemsInfo[i].mVersion);
        EXPECT_EQ(statuses[i].mState, ItemStateEnum::ePending);
    }
}

TEST_F(ImageManagerTest, DownloadUpdateItems_Cancel_BlobInfoFailed)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _)).WillRepeatedly(Return(ErrorEnum::eNotFound));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));

    std::thread downloadThread([&]() {
        auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);
        EXPECT_EQ(err, ErrorEnum::eCanceled);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto cancelErr = mImageManager.Cancel();
    EXPECT_TRUE(cancelErr.IsNone());

    downloadThread.join();
}

TEST_F(ImageManagerTest, DownloadUpdateItems_Cancel_DownloadFailed)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto& digests, Array<BlobInfo>& blobsInfo) {
            BlobInfo info;
            info.mDigest = digests[0];
            info.mSize   = 1024;
            info.mURLs.PushBack("http://test.com/blob");

            for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                info.mSHA256.PushBack(static_cast<uint8_t>(i));
            }

            blobsInfo.PushBack(info);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));
            testing::Mock::AllowLeak(space.Get());

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eRuntime));

    EXPECT_CALL(mDownloaderMock, Cancel(_)).WillOnce(Return(ErrorEnum::eNone));

    std::thread downloadThread([&]() {
        auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);
        EXPECT_EQ(err, ErrorEnum::eCanceled);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto cancelErr = mImageManager.Cancel();
    EXPECT_TRUE(cancelErr.IsNone());

    downloadThread.join();
}

} // namespace aos::cm::imagemanager
