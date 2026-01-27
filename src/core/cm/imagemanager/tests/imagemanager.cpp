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
#include "mocks/itemstatuslistenermock.hpp"
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
        mConfig.mInstallPath          = "/tmp/imagemanager_test/install";
        mConfig.mDownloadPath         = "/tmp/imagemanager_test/download";
        mConfig.mUpdateItemTTL        = Time::cSeconds * 10;
        mConfig.mRemoveOutdatedPeriod = Time::cSeconds * 20;

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

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(statuses.Size(), 0);
}

TEST_F(ImageManagerTest, DownloadUpdateItems_EmptyList_RemovesPendingItems)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(2).WillRepeatedly(Invoke([](Array<ItemInfo>& items) {
        ItemInfo pendingItem1;
        pendingItem1.mItemID      = "service1";
        pendingItem1.mVersion     = "1.0.0";
        pendingItem1.mIndexDigest = "sha256:abc123";
        pendingItem1.mState       = ItemStateEnum::ePending;
        items.PushBack(pendingItem1);

        ItemInfo pendingItem2;
        pendingItem2.mItemID      = "service2";
        pendingItem2.mVersion     = "2.0.0";
        pendingItem2.mIndexDigest = "sha256:def456";
        pendingItem2.mState       = ItemStateEnum::ePending;
        items.PushBack(pendingItem2);

        ItemInfo installedItem;
        installedItem.mItemID      = "service3";
        installedItem.mVersion     = "3.0.0";
        installedItem.mIndexDigest = "sha256:ghi789";
        installedItem.mState       = ItemStateEnum::eInstalled;
        items.PushBack(installedItem);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorageMock, RemoveItem(_, _))
        .Times(2)
        .WillOnce(Invoke([](const String& itemID, const String& version) {
            EXPECT_EQ(itemID, "service1");
            EXPECT_EQ(version, "1.0.0");
            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](const String& itemID, const String& version) {
            EXPECT_EQ(itemID, "service2");
            EXPECT_EQ(version, "2.0.0");
            return ErrorEnum::eNone;
        }));

    ItemStatusListenerMock listener;
    EXPECT_CALL(listener, OnItemsStatusesChanged(_))
        .Times(2)
        .WillRepeatedly(Invoke([](const Array<UpdateItemStatus>& statuses) {
            ASSERT_EQ(statuses.Size(), 1);
            EXPECT_EQ(statuses[0].mState, ItemStateEnum::eRemoved);
            EXPECT_TRUE(statuses[0].mError.IsNone());
        }));

    EXPECT_TRUE(mImageManager.SubscribeListener(listener).IsNone());

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 2);
    EXPECT_EQ(statuses[0].mItemID, "service1");
    EXPECT_EQ(statuses[0].mVersion, "1.0.0");
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eRemoved);
    EXPECT_TRUE(statuses[0].mError.IsNone());
    EXPECT_EQ(statuses[1].mItemID, "service2");
    EXPECT_EQ(statuses[1].mVersion, "2.0.0");
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eRemoved);
    EXPECT_TRUE(statuses[1].mError.IsNone());

    EXPECT_TRUE(mImageManager.UnsubscribeListener(listener).IsNone());
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
    item.mIndexDigest = "sha256:abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto&, Array<BlobInfo>& blobsInfo) {
            BlobInfo info;
            info.mDigest = "sha256:abc123";
            info.mSize   = 1024;
            info.mURLs.PushBack("http://test.com/blob");
            info.mDecryptInfo.EmplaceValue();
            info.mSignInfo.EmplaceValue();

            for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                info.mSHA256.PushBack(static_cast<uint8_t>(i));
            }

            blobsInfo.PushBack(info);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "sha256:fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:config1234567890abcdef1234567890abcdef1234567890abcdef1234567890";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
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

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, ItemState(ItemStateEnum::ePending), _))
        .WillOnce(Return(ErrorEnum::eNone));

    ItemStatusListenerMock listener;
    ItemStateEnum          lastState = ItemStateEnum::eDownloading;
    EXPECT_CALL(listener, OnItemsStatusesChanged(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([&item, &lastState](const Array<UpdateItemStatus>& statuses) {
            ASSERT_EQ(statuses.Size(), 1);
            EXPECT_EQ(statuses[0].mItemID, item.mItemID);
            EXPECT_EQ(statuses[0].mVersion, item.mVersion);
            lastState = statuses[0].mState;
        }));

    EXPECT_TRUE(mImageManager.SubscribeListener(listener).IsNone());

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, item.mItemID);
    EXPECT_EQ(statuses[0].mVersion, item.mVersion);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::ePending);
    EXPECT_EQ(lastState, ItemStateEnum::ePending);

    EXPECT_TRUE(mImageManager.UnsubscribeListener(listener).IsNone());
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
    item.mIndexDigest = "sha256:abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(2).WillRepeatedly(Invoke([&](Array<ItemInfo>& items) {
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
            info.mDigest = "sha256:abc123";
            info.mSize   = 1024;
            info.mURLs.PushBack("http://test.com/blob");
            info.mDecryptInfo.EmplaceValue();
            info.mSignInfo.EmplaceValue();

            for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                info.mSHA256.PushBack(static_cast<uint8_t>(i));
            }

            blobsInfo.PushBack(info);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
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
        manifest.mDigest = "sha256:fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:config1234567890abcdef1234567890abcdef1234567890abcdef1234567890";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
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

TEST_F(ImageManagerTest, DownloadUpdateItems_BlobsAlreadyExistOnDisk)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "sha256:aabb";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    auto blobsDir = fs::JoinPath(mConfig.mInstallPath, "/blobs/sha256/");
    fs::MakeDirAll(blobsDir);

    auto indexPath    = fs::JoinPath(blobsDir, "aabb");
    auto manifestPath = fs::JoinPath(blobsDir, "ccdd");
    auto configPath   = fs::JoinPath(blobsDir, "eeff");
    auto layerPath    = fs::JoinPath(blobsDir, "1122");

    std::ofstream(indexPath.CStr()).close();
    std::ofstream(manifestPath.CStr()).close();
    std::ofstream(configPath.CStr()).close();
    std::ofstream(layerPath.CStr()).close();

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .Times(4)
        .WillRepeatedly(Invoke([](const String& path, fs::FileInfo& info) {
            size_t lastSlashPos = 0;
            for (size_t i = 0; i < path.Size(); i++) {
                if (path[i] == '/') {
                    lastSlashPos = i;
                }
            }

            String digest(path.CStr() + lastSlashPos + 1);

            digest.HexToByteArray(info.mSHA256);
            info.mSize = 100;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "sha256:ccdd";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:eeff";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:1122";
            manifest.mLayers.PushBack(layer);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, ItemState(ItemStateEnum::ePending), _))
        .WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, item.mItemID);
    EXPECT_EQ(statuses[0].mVersion, item.mVersion);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::ePending);
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
        item.mIndexDigest = ("sha256:digest" + std::to_string(i)).c_str();
        itemsInfo.PushBack(item);
    }

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).Times(3).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto& digests, Array<BlobInfo>& blobsInfo) {
            for (const auto& digest : digests) {
                BlobInfo info;
                info.mDigest = digest;
                info.mSize   = 1024;
                info.mURLs.PushBack("http://test.com/blob");
                info.mDecryptInfo.EmplaceValue();
                info.mSignInfo.EmplaceValue();

                for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                    info.mSHA256.PushBack(static_cast<uint8_t>(i));
                }

                blobsInfo.PushBack(info);
            }

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "sha256:manifest";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:configlayer";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:layer";
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

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, ItemState(ItemStateEnum::ePending), _))
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
    item.mIndexDigest = "sha256:abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _)).WillRepeatedly(Return(ErrorEnum::eNotFound));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, _, _)).Times(AtLeast(0));

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
    item.mIndexDigest = "sha256:abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto& digests, Array<BlobInfo>& blobsInfo) {
            for (const auto& digest : digests) {
                BlobInfo info;
                info.mDigest = digest;
                info.mSize   = 1024;
                info.mURLs.PushBack("http://test.com/blob");
                info.mDecryptInfo.EmplaceValue();
                info.mSignInfo.EmplaceValue();

                for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                    info.mSHA256.PushBack(static_cast<uint8_t>(i));
                }

                blobsInfo.PushBack(info);
            }

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));
            testing::Mock::AllowLeak(space.Get());

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eRuntime));

    EXPECT_CALL(mDownloaderMock, Cancel(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, _, _)).Times(AtLeast(0));

    std::thread downloadThread([&]() {
        auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);
        EXPECT_EQ(err, ErrorEnum::eCanceled);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto cancelErr = mImageManager.Cancel();
    EXPECT_TRUE(cancelErr.IsNone());

    downloadThread.join();
}

TEST_F(ImageManagerTest, DownloadUpdateItems_RemovesOldPendingVersion)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "2.0.0";
    item.mIndexDigest = "sha256:abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(2).WillRepeatedly(Invoke([](Array<ItemInfo>& items) {
        ItemInfo oldItem;
        oldItem.mItemID      = "service1";
        oldItem.mVersion     = "1.0.0";
        oldItem.mIndexDigest = "sha256:old123";
        oldItem.mState       = ItemStateEnum::ePending;
        items.PushBack(oldItem);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorageMock, RemoveItem(_, _))
        .Times(2)
        .WillRepeatedly(Invoke([](const String& itemID, const String& version) {
            EXPECT_EQ(itemID, "service1");
            EXPECT_EQ(version, "1.0.0");

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Invoke([](const ItemInfo& item) {
        EXPECT_EQ(item.mItemID, "service1");
        EXPECT_EQ(item.mVersion, "2.0.0");
        EXPECT_EQ(item.mState, ItemStateEnum::eDownloading);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto&, Array<BlobInfo>& blobsInfo) {
            BlobInfo info;
            info.mDigest = "sha256:abc123";
            info.mSize   = 1024;
            info.mURLs.PushBack("http://test.com/blob");
            info.mDecryptInfo.EmplaceValue();
            info.mSignInfo.EmplaceValue();

            for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                info.mSHA256.PushBack(static_cast<uint8_t>(i));
            }

            blobsInfo.PushBack(info);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "sha256:fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:config1234567890abcdef1234567890abcdef1234567890abcdef1234567890";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
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

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, ItemState(ItemStateEnum::ePending), _))
        .WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, "service1");
    EXPECT_EQ(statuses[0].mVersion, "2.0.0");
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::ePending);
}

TEST_F(ImageManagerTest, DownloadUpdateItems_RemovesOldFailedVersion)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "2.0.0";
    item.mIndexDigest = "sha256:abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(2).WillRepeatedly(Invoke([](Array<ItemInfo>& items) {
        ItemInfo oldItem;
        oldItem.mItemID      = "service1";
        oldItem.mVersion     = "1.0.0";
        oldItem.mIndexDigest = "sha256:old123";
        oldItem.mState       = ItemStateEnum::eFailed;
        items.PushBack(oldItem);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorageMock, RemoveItem(_, _)).WillOnce(Invoke([](const String& itemID, const String& version) {
        EXPECT_EQ(itemID, "service1");
        EXPECT_EQ(version, "1.0.0");

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorageMock, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfos(_, _))
        .WillRepeatedly(Invoke([](const auto&, Array<BlobInfo>& blobsInfo) {
            BlobInfo info;
            info.mDigest = "sha256:abc123";
            info.mSize   = 1024;
            info.mURLs.PushBack("http://test.com/blob");
            info.mDecryptInfo.EmplaceValue();
            info.mSignInfo.EmplaceValue();

            for (size_t i = 0; i < crypto::cSHA256Size; i++) {
                info.mSHA256.PushBack(static_cast<uint8_t>(i));
            }

            blobsInfo.PushBack(info);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mDownloadingSpaceAllocatorMock, FreeSpace(_)).Times(AtLeast(0));
    EXPECT_CALL(mDownloadingSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {std::move(space), ErrorEnum::eNone};
        }));

    EXPECT_CALL(mDownloaderMock, Download(_, _, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "sha256:fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:config1234567890abcdef1234567890abcdef1234567890abcdef1234567890";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
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

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, ItemState(ItemStateEnum::ePending), _))
        .WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.DownloadUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::ePending);
}

TEST_F(ImageManagerTest, InstallUpdateItems_Success)
{
    StaticArray<UpdateItemInfo, 5>   itemsInfo;
    StaticArray<UpdateItemStatus, 5> statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "sha256:1111";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).Times(5).WillRepeatedly(Invoke([](Array<ItemInfo>& items) {
        ItemInfo storedItem;
        storedItem.mItemID      = "service1";
        storedItem.mVersion     = "1.0.0";
        storedItem.mIndexDigest = "sha256:1111";
        storedItem.mState       = ItemStateEnum::ePending;
        items.PushBack(storedItem);

        return ErrorEnum::eNone;
    }));

    auto blobsDir = fs::JoinPath(mConfig.mInstallPath, "/blobs/sha256/");
    fs::MakeDirAll(blobsDir);

    auto indexPath    = fs::JoinPath(blobsDir, "1111");
    auto manifestPath = fs::JoinPath(blobsDir, "2222");
    auto configPath   = fs::JoinPath(blobsDir, "cccc");
    auto layerPath    = fs::JoinPath(blobsDir, "3333");

    std::ofstream(indexPath.CStr()).close();
    std::ofstream(manifestPath.CStr()).close();
    std::ofstream(configPath.CStr()).close();
    std::ofstream(layerPath.CStr()).close();

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .Times(4)
        .WillRepeatedly(Invoke([](const String& path, fs::FileInfo& info) {
            size_t lastSlashPos = 0;
            for (size_t i = 0; i < path.Size(); i++) {
                if (path[i] == '/') {
                    lastSlashPos = i;
                }
            }

            String digest(path.CStr() + lastSlashPos + 1);

            digest.HexToByteArray(info.mSHA256);
            info.mSize = 100;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "sha256:2222";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:cccc";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:3333";
            manifest.mLayers.PushBack(layer);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, _, _))
        .WillOnce(Invoke([](const String& id, const String& version, ItemState state, Time) {
            EXPECT_EQ(id, "service1");
            EXPECT_EQ(version, "1.0.0");
            EXPECT_EQ(state, ItemStateEnum::eInstalled);

            return ErrorEnum::eNone;
        }));

    ItemStatusListenerMock listener;
    ItemStateEnum          lastState = ItemStateEnum::ePending;
    EXPECT_CALL(listener, OnItemsStatusesChanged(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([&lastState](const Array<UpdateItemStatus>& statuses) {
            ASSERT_EQ(statuses.Size(), 1);
            EXPECT_EQ(statuses[0].mItemID, "service1");
            EXPECT_EQ(statuses[0].mVersion, "1.0.0");
            lastState = statuses[0].mState;
        }));

    EXPECT_TRUE(mImageManager.SubscribeListener(listener).IsNone());

    auto err = mImageManager.InstallUpdateItems(itemsInfo, statuses);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, "service1");
    EXPECT_EQ(statuses[0].mVersion, "1.0.0");
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalled);
    EXPECT_TRUE(statuses[0].mError.IsNone());
    EXPECT_EQ(lastState, ItemStateEnum::eInstalled);

    EXPECT_TRUE(mImageManager.UnsubscribeListener(listener).IsNone());
}

TEST_F(ImageManagerTest, InstallUpdateItems_VerifyBlobsIntegrity_IndexNotFound)
{
    StaticArray<UpdateItemInfo, 5>   itemsInfo;
    StaticArray<UpdateItemStatus, 5> statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "sha256:abc123";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_))
        .Times(5)
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo storedItem;
            storedItem.mItemID      = "service1";
            storedItem.mVersion     = "1.0.0";
            storedItem.mIndexDigest = "sha256:abc123";
            storedItem.mState       = ItemStateEnum::ePending;
            items.PushBack(storedItem);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo storedItem;
            storedItem.mItemID      = "service1";
            storedItem.mVersion     = "1.0.0";
            storedItem.mIndexDigest = "sha256:abc123";
            storedItem.mState       = ItemStateEnum::ePending;
            items.PushBack(storedItem);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>&) { return ErrorEnum::eNone; }))
        .WillOnce(Invoke([](Array<ItemInfo>&) { return ErrorEnum::eNone; }))
        .WillOnce(Invoke([](Array<ItemInfo>&) { return ErrorEnum::eNone; }));

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _)).WillOnce(Return(ErrorEnum::eNotFound));

    EXPECT_CALL(mStorageMock, RemoveItem(_, _)).WillOnce(Invoke([](const String& id, const String& version) {
        EXPECT_EQ(id, "service1");
        EXPECT_EQ(version, "1.0.0");

        return ErrorEnum::eNone;
    }));

    auto err = mImageManager.InstallUpdateItems(itemsInfo, statuses);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, "service1");
    EXPECT_EQ(statuses[0].mVersion, "1.0.0");
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(!statuses[0].mError.IsNone());
}

TEST_F(ImageManagerTest, InstallUpdateItems_RemoveDifferentVersion)
{
    StaticArray<UpdateItemInfo, 5>   itemsInfo;
    StaticArray<UpdateItemStatus, 5> statuses;

    UpdateItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "2.0.0";
    item.mIndexDigest = "sha256:4444";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_))
        .Times(5)
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo oldItem;
            oldItem.mItemID      = "service1";
            oldItem.mVersion     = "1.0.0";
            oldItem.mIndexDigest = "sha256:1111";
            oldItem.mState       = ItemStateEnum::eInstalled;
            items.PushBack(oldItem);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo newItem;
            newItem.mItemID      = "service1";
            newItem.mVersion     = "2.0.0";
            newItem.mIndexDigest = "sha256:4444";
            newItem.mState       = ItemStateEnum::ePending;
            items.PushBack(newItem);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo newItem;
            newItem.mItemID      = "service1";
            newItem.mVersion     = "2.0.0";
            newItem.mIndexDigest = "sha256:4444";
            newItem.mState       = ItemStateEnum::ePending;
            items.PushBack(newItem);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo newItem;
            newItem.mItemID      = "service1";
            newItem.mVersion     = "2.0.0";
            newItem.mIndexDigest = "sha256:4444";
            newItem.mState       = ItemStateEnum::eInstalled;
            items.PushBack(newItem);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>&) { return ErrorEnum::eNone; }));

    EXPECT_CALL(mStorageMock, RemoveItem(_, _)).WillOnce(Invoke([](const String& id, const String& version) {
        EXPECT_EQ(id, "service1");
        EXPECT_EQ(version, "1.0.0");
        return ErrorEnum::eNone;
    }));

    auto blobsDir = fs::JoinPath(mConfig.mInstallPath, "/blobs/sha256/");
    fs::MakeDirAll(blobsDir);

    auto indexPath    = fs::JoinPath(blobsDir, "4444");
    auto manifestPath = fs::JoinPath(blobsDir, "5555");
    auto configPath   = fs::JoinPath(blobsDir, "7777");
    auto layerPath    = fs::JoinPath(blobsDir, "6666");

    std::ofstream(indexPath.CStr()).close();
    std::ofstream(manifestPath.CStr()).close();
    std::ofstream(configPath.CStr()).close();
    std::ofstream(layerPath.CStr()).close();

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .Times(4)
        .WillRepeatedly(Invoke([](const String& path, fs::FileInfo& info) {
            size_t lastSlashPos = 0;
            for (size_t i = 0; i < path.Size(); i++) {
                if (path[i] == '/') {
                    lastSlashPos = i;
                }
            }

            String digest(path.CStr() + lastSlashPos + 1);

            digest.HexToByteArray(info.mSHA256);
            info.mSize = 100;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "sha256:5555";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:7777";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:6666";
            manifest.mLayers.PushBack(layer);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, _, _))
        .WillOnce(Invoke([](const String& id, const String& version, ItemState state, Time) {
            EXPECT_EQ(id, "service1");
            EXPECT_EQ(version, "2.0.0");
            EXPECT_EQ(state, ItemStateEnum::eInstalled);

            return ErrorEnum::eNone;
        }));

    auto err = mImageManager.InstallUpdateItems(itemsInfo, statuses);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, "service1");
    EXPECT_EQ(statuses[0].mVersion, "2.0.0");
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalled);
    EXPECT_TRUE(statuses[0].mError.IsNone());
}

TEST_F(ImageManagerTest, InstallUpdateItems_SetItemsToRemoved)
{
    StaticArray<UpdateItemInfo, 5>   itemsInfo;
    StaticArray<UpdateItemStatus, 5> statuses;
    UpdateItemInfo                   item;

    item.mItemID      = "service2";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "sha256:aaaa";
    itemsInfo.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_))
        .Times(5)
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo item1;
            item1.mItemID      = "service1";
            item1.mVersion     = "1.0.0";
            item1.mIndexDigest = "sha256:1111";
            item1.mState       = ItemStateEnum::eInstalled;
            items.PushBack(item1);

            ItemInfo item2;
            item2.mItemID      = "service2";
            item2.mVersion     = "1.0.0";
            item2.mIndexDigest = "sha256:aaaa";
            item2.mState       = ItemStateEnum::ePending;

            items.PushBack(item2);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo item1;
            item1.mItemID      = "service1";
            item1.mVersion     = "1.0.0";
            item1.mIndexDigest = "sha256:1111";
            item1.mState       = ItemStateEnum::eInstalled;
            items.PushBack(item1);

            ItemInfo item2;
            item2.mItemID      = "service2";
            item2.mVersion     = "1.0.0";
            item2.mIndexDigest = "sha256:aaaa";
            item2.mState       = ItemStateEnum::ePending;
            items.PushBack(item2);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo item1;
            item1.mItemID      = "service1";
            item1.mVersion     = "1.0.0";
            item1.mIndexDigest = "sha256:1111";
            item1.mState       = ItemStateEnum::eInstalled;
            items.PushBack(item1);

            ItemInfo item2;
            item2.mItemID      = "service2";
            item2.mVersion     = "1.0.0";
            item2.mIndexDigest = "sha256:aaaa";
            item2.mState       = ItemStateEnum::ePending;
            items.PushBack(item2);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo item1;
            item1.mItemID      = "service1";
            item1.mVersion     = "1.0.0";
            item1.mIndexDigest = "sha256:1111";
            item1.mState       = ItemStateEnum::eInstalled;
            items.PushBack(item1);

            ItemInfo item2;
            item2.mItemID      = "service2";
            item2.mVersion     = "1.0.0";
            item2.mIndexDigest = "sha256:aaaa";
            item2.mState       = ItemStateEnum::eInstalled;
            items.PushBack(item2);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>&) { return ErrorEnum::eNone; }));

    auto blobsDir = fs::JoinPath(mConfig.mInstallPath, "/blobs/sha256/");
    fs::MakeDirAll(blobsDir);

    auto service2Index    = fs::JoinPath(blobsDir, "aaaa");
    auto service2Manifest = fs::JoinPath(blobsDir, "bbbb");
    auto service2Config   = fs::JoinPath(blobsDir, "dddd");
    auto service2Layer    = fs::JoinPath(blobsDir, "cccc");

    std::ofstream(service2Index.CStr()).close();
    std::ofstream(service2Manifest.CStr()).close();
    std::ofstream(service2Config.CStr()).close();
    std::ofstream(service2Layer.CStr()).close();

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .Times(4)
        .WillRepeatedly(Invoke([](const String& path, fs::FileInfo& info) {
            size_t lastSlashPos = 0;
            for (size_t i = 0; i < path.Size(); i++) {
                if (path[i] == '/') {
                    lastSlashPos = i;
                }
            }

            String digest(path.CStr() + lastSlashPos + 1);

            digest.HexToByteArray(info.mSHA256);
            info.mSize = 100;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _)).WillRepeatedly(Invoke([](const String&, oci::ImageIndex& index) {
        oci::IndexContentDescriptor manifest;
        manifest.mDigest = "sha256:bbbb";
        index.mManifests.PushBack(manifest);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:dddd";

            oci::ContentDescriptor layer;
            layer.mDigest = "sha256:cccc";
            manifest.mLayers.PushBack(layer);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mStorageMock, UpdateItemState(_, _, _, _))
        .WillOnce(Invoke([](const String& id, const String& version, ItemState state, Time) {
            EXPECT_EQ(id, "service2");
            EXPECT_EQ(version, "1.0.0");
            EXPECT_EQ(state, ItemStateEnum::eInstalled);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](const String& id, const String& version, ItemState state, Time) {
            EXPECT_EQ(id, "service1");
            EXPECT_EQ(version, "1.0.0");
            EXPECT_EQ(state, ItemStateEnum::eRemoved);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mInstallSpaceAllocatorMock, AddOutdatedItem(_, _, _))
        .WillOnce(Invoke([](const String& id, const String& version, const Time&) {
            EXPECT_EQ(id, "service1");
            EXPECT_EQ(version, "1.0.0");

            return ErrorEnum::eNone;
        }));

    auto err = mImageManager.InstallUpdateItems(itemsInfo, statuses);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(statuses.Size(), 1);
    EXPECT_EQ(statuses[0].mItemID, "service2");
    EXPECT_EQ(statuses[0].mVersion, "1.0.0");
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalled);
    EXPECT_TRUE(statuses[0].mError.IsNone());
}

TEST_F(ImageManagerTest, RemoveItem_Success)
{
    EXPECT_CALL(mStorageMock, GetItemsInfo(_))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo item1;
            item1.mItemID      = "service1";
            item1.mVersion     = "1.0.0";
            item1.mIndexDigest = "sha256:1111";
            item1.mState       = ItemStateEnum::eRemoved;
            items.PushBack(item1);

            ItemInfo item2;
            item2.mItemID      = "service2";
            item2.mVersion     = "1.0.0";
            item2.mIndexDigest = "sha256:4444";
            item2.mState       = ItemStateEnum::eInstalled;
            items.PushBack(item2);

            return ErrorEnum::eNone;
        }))
        .WillOnce(Invoke([](Array<ItemInfo>& items) {
            ItemInfo item2;
            item2.mItemID      = "service2";
            item2.mVersion     = "1.0.0";
            item2.mIndexDigest = "sha256:4444";
            item2.mState       = ItemStateEnum::eInstalled;
            items.PushBack(item2);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mStorageMock, RemoveItem(_, _)).WillOnce(Invoke([](const String& id, const String& version) {
        EXPECT_EQ(id, "service1");
        EXPECT_EQ(version, "1.0.0");

        return ErrorEnum::eNone;
    }));

    auto blobsDir = fs::JoinPath(mConfig.mInstallPath, "/blobs/sha256/");
    fs::MakeDirAll(blobsDir);

    auto index1Path    = fs::JoinPath(blobsDir, "1111");
    auto manifest1Path = fs::JoinPath(blobsDir, "2222");
    auto config1Path   = fs::JoinPath(blobsDir, "config2");
    auto layer1Path    = fs::JoinPath(blobsDir, "3333");

    {
        std::ofstream f1(index1Path.CStr(), std::ios::binary);
        f1 << "index1_content_data";
        f1.flush();
    }
    {
        std::ofstream f2(manifest1Path.CStr(), std::ios::binary);
        f2 << "manifest1_content_data";
        f2.flush();
    }
    {
        std::ofstream f3(config1Path.CStr(), std::ios::binary);
        f3 << "config1_content_data";
        f3.flush();
    }
    {
        std::ofstream f4(layer1Path.CStr(), std::ios::binary);
        f4 << "layer1_content_data";
        f4.flush();
    }

    auto index2Path    = fs::JoinPath(blobsDir, "4444");
    auto manifest2Path = fs::JoinPath(blobsDir, "5555");
    auto config2Path   = fs::JoinPath(blobsDir, "config5");
    auto layer2Path    = fs::JoinPath(blobsDir, "6666");

    {
        std::ofstream f1(index2Path.CStr(), std::ios::binary);
        f1 << "index2_content_data";
        f1.flush();
    }
    {
        std::ofstream f2(manifest2Path.CStr(), std::ios::binary);
        f2 << "manifest2_content_data";
        f2.flush();
    }
    {
        std::ofstream f3(config2Path.CStr(), std::ios::binary);
        f3 << "config2_content_data";
        f3.flush();
    }
    {
        std::ofstream f4(layer2Path.CStr(), std::ios::binary);
        f4 << "layer2_content_data";
        f4.flush();
    }

    EXPECT_CALL(mOCISpecMock, LoadImageIndex(_, _))
        .WillRepeatedly(Invoke([]([[maybe_unused]] const String& path, oci::ImageIndex& index) {
            auto [pos1, err1] = path.FindSubstr(0, "1111");
            if (err1.IsNone()) {
                oci::IndexContentDescriptor manifest;
                manifest.mDigest = "sha256:2222";
                index.mManifests.PushBack(manifest);
            } else {
                auto [pos2, err2] = path.FindSubstr(0, "4444");
                if (err2.IsNone()) {
                    oci::IndexContentDescriptor manifest;
                    manifest.mDigest = "sha256:5555";
                    index.mManifests.PushBack(manifest);
                }
            }
            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([]([[maybe_unused]] const String& path, oci::ImageManifest& manifest) {
            auto [pos1, err1] = path.FindSubstr(0, "2222");
            if (err1.IsNone()) {
                manifest.mConfig.mDigest = "sha256:config2";

                oci::ContentDescriptor layer;
                layer.mDigest = "sha256:3333";
                manifest.mLayers.PushBack(layer);
            } else {
                auto [pos2, err2] = path.FindSubstr(0, "5555");
                if (err2.IsNone()) {
                    manifest.mConfig.mDigest = "sha256:config5";

                    oci::ContentDescriptor layer;
                    layer.mDigest = "sha256:6666";
                    manifest.mLayers.PushBack(layer);
                }
            }
            return ErrorEnum::eNone;
        }));

    ItemStatusListenerMock listener;
    EXPECT_CALL(listener, OnItemRemoved(_)).WillOnce(Invoke([](const String& itemID) {
        EXPECT_EQ(itemID, "service1");
    }));

    EXPECT_TRUE(mImageManager.SubscribeListener(listener).IsNone());

    auto [totalSize, err] = mImageManager.RemoveItem("service1", "1.0.0");

    EXPECT_TRUE(err.IsNone());
    EXPECT_GT(totalSize, 0);

    EXPECT_TRUE(mImageManager.UnsubscribeListener(listener).IsNone());

    auto [index1Exists, index1Err] = fs::FileExist(index1Path);
    EXPECT_FALSE(index1Exists);

    auto [manifest1Exists, manifest1Err] = fs::FileExist(manifest1Path);
    EXPECT_FALSE(manifest1Exists);

    auto [config1Exists, config1Err] = fs::FileExist(config1Path);
    EXPECT_FALSE(config1Exists);

    auto [layer1Exists, layer1Err] = fs::FileExist(layer1Path);
    EXPECT_FALSE(layer1Exists);

    auto [index2Exists, index2Err] = fs::FileExist(index2Path);
    EXPECT_TRUE(index2Exists);

    auto [manifest2Exists, manifest2Err] = fs::FileExist(manifest2Path);
    EXPECT_TRUE(manifest2Exists);

    auto [config2Exists, config2Err] = fs::FileExist(config2Path);
    EXPECT_TRUE(config2Exists);

    auto [layer2Exists, layer2Err] = fs::FileExist(layer2Path);
    EXPECT_TRUE(layer2Exists);
}

TEST_F(ImageManagerTest, Start_RemovesOutdatedItems)
{
    StaticArray<ItemInfo, 5> items;

    ItemInfo item1;
    item1.mItemID      = "service1";
    item1.mVersion     = "1.0.0";
    item1.mIndexDigest = "sha256:1111";
    item1.mState       = ItemStateEnum::eRemoved;
    item1.mTimestamp   = Time::Now().Add(-(Time::cSeconds * 20));
    items.PushBack(item1);

    ItemInfo item2;
    item2.mItemID      = "service2";
    item2.mVersion     = "2.0.0";
    item2.mIndexDigest = "sha256:2222";
    item2.mState       = ItemStateEnum::eRemoved;
    item2.mTimestamp   = Time::Now();
    items.PushBack(item2);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_))
        .WillOnce(DoAll(SetArgReferee<0>(items), Return(ErrorEnum::eNone)))
        .WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mStorageMock, RemoveItem(_, _)).WillOnce(Invoke([](const String& id, const String& version) {
        EXPECT_EQ(id, "service1");
        EXPECT_EQ(version, "1.0.0");
        return ErrorEnum::eNone;
    }));

    ItemStatusListenerMock listener;
    EXPECT_CALL(listener, OnItemRemoved(_)).WillOnce(Invoke([](const String& itemID) {
        EXPECT_EQ(itemID, "service1");
    }));

    EXPECT_TRUE(mImageManager.SubscribeListener(listener).IsNone());

    EXPECT_CALL(mInstallSpaceAllocatorMock, FreeSpace(_)).Times(1);

    EXPECT_TRUE(mImageManager.Start().IsNone());
    EXPECT_TRUE(mImageManager.Stop().IsNone());
    EXPECT_TRUE(mImageManager.UnsubscribeListener(listener).IsNone());
}

TEST_F(ImageManagerTest, GetIndexDigest_Success)
{
    StaticArray<ItemInfo, 5> items;

    ItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "sha256:abc123";
    item.mState       = ItemStateEnum::eRemoved;
    items.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfos(_, _)).WillOnce(DoAll(SetArgReferee<1>(items), Return(ErrorEnum::eNone)));

    StaticString<oci::cDigestLen> digest;
    auto                          err = mImageManager.GetIndexDigest("service1", "1.0.0", digest);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(digest, "sha256:abc123");
}

TEST_F(ImageManagerTest, GetIndexDigest_NotFound)
{
    StaticArray<ItemInfo, 5> items;

    EXPECT_CALL(mStorageMock, GetItemsInfos(_, _)).WillOnce(DoAll(SetArgReferee<1>(items), Return(ErrorEnum::eNone)));

    StaticString<oci::cDigestLen> digest;
    auto                          err = mImageManager.GetIndexDigest("nonexistent", "1.0.0", digest);

    EXPECT_EQ(err.Value(), ErrorEnum::eNotFound);
}

TEST_F(ImageManagerTest, GetIndexDigest_WrongState)
{
    StaticArray<ItemInfo, 5> items;

    ItemInfo item;
    item.mItemID      = "service1";
    item.mVersion     = "1.0.0";
    item.mIndexDigest = "sha256:abc123";
    item.mState       = ItemStateEnum::eDownloading;
    items.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfos(_, _)).WillOnce(DoAll(SetArgReferee<1>(items), Return(ErrorEnum::eNone)));

    StaticString<oci::cDigestLen> digest;
    auto                          err = mImageManager.GetIndexDigest("service1", "1.0.0", digest);

    EXPECT_EQ(err.Value(), ErrorEnum::eNotFound);
}

TEST_F(ImageManagerTest, GetBlobPath_Success)
{
    auto blobsDir = fs::JoinPath(mConfig.mInstallPath, "blobs", "sha256");
    fs::MakeDirAll(blobsDir);

    StaticString<cFilePathLen> blobPath;
    blobPath = fs::JoinPath(blobsDir, "testdigest");

    std::ofstream file(blobPath.CStr());
    file << "test content";
    file.close();

    StaticString<cFilePathLen> path;
    auto                       err = mImageManager.GetBlobPath("sha256:testdigest", path);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(path, blobPath);
}

TEST_F(ImageManagerTest, GetBlobPath_NotFound)
{
    StaticString<cFilePathLen> path;
    auto                       err = mImageManager.GetBlobPath("sha256:nonexistent", path);

    EXPECT_EQ(err.Value(), ErrorEnum::eNotFound);
}

TEST_F(ImageManagerTest, GetBlobURL_Success)
{
    auto blobsDir = fs::JoinPath(mConfig.mInstallPath, "/blobs/sha256/");
    fs::MakeDirAll(blobsDir);

    StaticString<cFilePathLen> blobPath;
    blobPath = fs::JoinPath(blobsDir, "testdigest");

    std::ofstream file(blobPath.CStr());
    file << "test content";
    file.close();

    EXPECT_CALL(mFileServerMock, TranslateFilePathURL(_, _))
        .WillOnce(DoAll(SetArgReferee<1>("http://localhost/blobs/testdigest"), Return(ErrorEnum::eNone)));

    StaticString<cURLLen> url;
    auto                  err = mImageManager.GetBlobURL("sha256:testdigest", url);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(url, "http://localhost/blobs/testdigest");
}

TEST_F(ImageManagerTest, GetBlobURL_NotFound)
{
    StaticString<cURLLen> url;
    auto                  err = mImageManager.GetBlobURL("sha256:nonexistent", url);

    EXPECT_EQ(err.Value(), ErrorEnum::eNotFound);
}

TEST_F(ImageManagerTest, GetItemCurrentVersion_PendingPriority)
{
    StaticArray<ItemInfo, 5> items;

    ItemInfo installed;
    installed.mItemID  = "service1";
    installed.mVersion = "1.0.0";
    installed.mState   = ItemStateEnum::eInstalled;
    items.PushBack(installed);

    ItemInfo pending;
    pending.mItemID  = "service1";
    pending.mVersion = "2.0.0";
    pending.mState   = ItemStateEnum::ePending;
    items.PushBack(pending);

    EXPECT_CALL(mStorageMock, GetItemsInfos(_, _)).WillOnce(DoAll(SetArgReferee<1>(items), Return(ErrorEnum::eNone)));

    StaticString<cVersionLen> version;
    auto                      err = mImageManager.GetItemCurrentVersion("service1", version);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(version, "2.0.0");
}

TEST_F(ImageManagerTest, GetItemCurrentVersion_InstalledFallback)
{
    StaticArray<ItemInfo, 5> items;

    ItemInfo item;
    item.mItemID  = "service1";
    item.mVersion = "1.0.0";
    item.mState   = ItemStateEnum::eInstalled;
    items.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfos(_, _)).WillOnce(DoAll(SetArgReferee<1>(items), Return(ErrorEnum::eNone)));

    StaticString<cVersionLen> version;
    auto                      err = mImageManager.GetItemCurrentVersion("service1", version);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(version, "1.0.0");
}

TEST_F(ImageManagerTest, GetItemCurrentVersion_NotFound)
{
    StaticArray<ItemInfo, 5> items;

    ItemInfo item;
    item.mItemID  = "service1";
    item.mVersion = "1.0.0";
    item.mState   = ItemStateEnum::eRemoved;
    items.PushBack(item);

    EXPECT_CALL(mStorageMock, GetItemsInfos(_, _)).WillOnce(DoAll(SetArgReferee<1>(items), Return(ErrorEnum::eNone)));

    StaticString<cVersionLen> version;
    auto                      err = mImageManager.GetItemCurrentVersion("service1", version);

    EXPECT_EQ(err.Value(), ErrorEnum::eNotFound);
}

TEST_F(ImageManagerTest, GetUpdateItemsStatuses_Success)
{
    StaticArray<ItemInfo, 5> items;

    ItemInfo item1;
    item1.mItemID  = "service1";
    item1.mVersion = "1.0.0";
    item1.mState   = ItemStateEnum::eInstalled;
    items.PushBack(item1);

    ItemInfo item2;
    item2.mItemID  = "service2";
    item2.mVersion = "2.0.0";
    item2.mState   = ItemStateEnum::ePending;
    items.PushBack(item2);

    EXPECT_CALL(mStorageMock, GetItemsInfo(_)).WillOnce(DoAll(SetArgReferee<0>(items), Return(ErrorEnum::eNone)));

    StaticArray<UpdateItemStatus, 5> statuses;
    auto                             err = mImageManager.GetUpdateItemsStatuses(statuses);

    EXPECT_TRUE(err.IsNone());
    ASSERT_EQ(statuses.Size(), 2);

    EXPECT_EQ(statuses[0].mItemID, "service1");
    EXPECT_EQ(statuses[0].mVersion, "1.0.0");
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalled);

    EXPECT_EQ(statuses[1].mItemID, "service2");
    EXPECT_EQ(statuses[1].mVersion, "2.0.0");
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::ePending);
}

} // namespace aos::cm::imagemanager
