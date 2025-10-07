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
#include <core/cm/tests/mocks/fileserver.hpp>
#include <core/common/tests/mocks/cryptomock.hpp>
#include <core/common/tests/mocks/fileinfoprovider.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tests/utils/log.hpp>

#include "mocks/imageunpacker.hpp"
#include "mocks/statusnotifier.hpp"
#include "mocks/storage.hpp"

using namespace testing;

namespace aos::cm::imagemanager {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ImageManagerTest : public testing::Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        mConfig.mInstallPath   = "/tmp/imagemanager_test/install";
        mConfig.mTmpPath       = "/tmp/imagemanager_test/temp";
        mConfig.mUpdateItemTTL = 24 * Time::cHours;

        fs::MakeDirAll(mConfig.mInstallPath);
        fs::MakeDirAll(mConfig.mTmpPath);

        EXPECT_CALL(mMockStorage, GetItemsInfo(_)).WillRepeatedly(Return(ErrorEnum::eNone));

        EXPECT_TRUE(mImageManager
                        .Init(mConfig, mMockStorage, mMockSpaceAllocator, mMockTmpSpaceAllocator, mMockFileServer,
                            mMockImageDecrypter, mMockFileInfoProvider, mMockImageUnpacker, mMockOCISpec)
                        .IsNone());
    }

    void TearDown() override
    {
        fs::RemoveAll(mConfig.mInstallPath);
        fs::RemoveAll(mConfig.mTmpPath);
    }

    Config                                         mConfig;
    ImageManager                                   mImageManager;
    StaticAllocator<1024 * 5, 20>                  mAllocator;
    StrictMock<MockStorage>                        mMockStorage;
    StrictMock<spaceallocator::MockSpaceAllocator> mMockSpaceAllocator;
    StrictMock<spaceallocator::MockSpaceAllocator> mMockTmpSpaceAllocator;
    StrictMock<fileserver::MockFileServer>         mMockFileServer;
    StrictMock<crypto::CryptoHelperMock>           mMockImageDecrypter;
    StrictMock<fs::MockFileInfoProvider>           mMockFileInfoProvider;
    StrictMock<spaceallocator::MockSpace>          mMockSpace;
    StrictMock<MockStatusListener>                 mMockStatusListener;
    StrictMock<MockImageUnpacker>                  mMockImageUnpacker;
    StrictMock<oci::OCISpecMock>                   mMockOCISpec;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ImageManagerTest, InstallUpdateItems_Success)
{
    StaticArray<UpdateItemInfo, 5>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 5>             statuses;

    for (size_t i = 0; i < 5; ++i) {
        {
            auto err = itemsInfo.EmplaceBack();
            ASSERT_TRUE(err.IsNone());
        }

        auto& itemInfo = itemsInfo.Back();

        itemInfo.mID      = (std::string("12345678-1234-1234-1234-12345678901") + std::to_string(i)).c_str();
        itemInfo.mType    = (i < 4) ? UpdateItemTypeEnum::eService : UpdateItemTypeEnum::eLayer;
        itemInfo.mVersion = (("1.0." + std::to_string(i)).c_str());

        auto err = itemInfo.mImages.EmplaceBack();
        ASSERT_TRUE(err.IsNone());

        auto& imageInfo = itemInfo.mImages.Back();

        imageInfo.mImage.mImageID = (std::string("87654321-4321-4321-4321-87654321098") + std::to_string(i)).c_str();
        imageInfo.mImage.mArchInfo.mArchitecture = "x86_64";
        imageInfo.mImage.mOSInfo.mOS             = "linux";

        imageInfo.mPath = ("/tmp/test-image-" + std::to_string(i + 1) + ".tar").c_str();
        imageInfo.mSize = 1024 * (i + 1);
        imageInfo.mSHA256.Clear();
        imageInfo.mSHA256.PushBack(static_cast<uint8_t>(0x01 + i));
        imageInfo.mSHA256.PushBack(0x02);
        imageInfo.mSHA256.PushBack(0x03);
    }

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(_, _))
        .Times(5)
        .WillRepeatedly(DoAll(Invoke([](const String&, Array<storage::ItemInfo>& items) {
            items.Clear();
            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockSpaceAllocator, AllocateSpace(_)).Times(5).WillRepeatedly(Invoke([this](size_t) {
        auto mockSpace = aos::MakeUnique<spaceallocator::MockSpace>(&mAllocator);
        EXPECT_CALL(*mockSpace, Accept()).WillOnce(Return(ErrorEnum::eNone));
        return RetWithError<UniquePtr<spaceallocator::SpaceItf>>(std::move(mockSpace));
    }));
    EXPECT_CALL(mMockImageDecrypter, Decrypt(_, _, _)).Times(5).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockImageDecrypter, ValidateSigns(_, _, _, _)).Times(5).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockFileInfoProvider, CreateFileInfo(_, _))
        .Times(5)
        .WillRepeatedly(DoAll(Invoke([&itemsInfo](const String& path, fs::FileInfo& info) {
            for (const auto& item : itemsInfo) {
                for (const auto& image : item.mImages) {
                    auto [_, err] = path.FindSubstr(0, image.mImage.mImageID);
                    if (err.IsNone()) {
                        info.mSize   = image.mSize;
                        info.mSHA256 = image.mSHA256;

                        return ErrorEnum::eNone;
                    }
                }
            }

            info.mSize = 1024;
            info.mSHA256.Clear();
            info.mSHA256.PushBack(0x01);
            info.mSHA256.PushBack(0x02);
            info.mSHA256.PushBack(0x03);

            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockFileServer, TranslateFilePathURL(_, _))
        .Times(5)
        .WillRepeatedly(DoAll(Invoke([](const String&, String& outURL) {
            outURL = "http://test-url";

            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockTmpSpaceAllocator, AllocateSpace(_)).Times(13).WillRepeatedly(Invoke([this](size_t) {
        auto mockSpace = aos::MakeUnique<spaceallocator::MockSpace>(&mAllocator);
        EXPECT_CALL(*mockSpace, Release()).WillOnce(Return(ErrorEnum::eNone));
        return RetWithError<UniquePtr<spaceallocator::SpaceItf>>(std::move(mockSpace));
    }));
    EXPECT_CALL(mMockImageUnpacker, GetUncompressedFileSize(_, _))
        .Times(13)
        .WillRepeatedly(Return(RetWithError<size_t>(128)));
    EXPECT_CALL(mMockImageUnpacker, ExtractFileFromArchive(_, _, _))
        .Times(13)
        .WillRepeatedly(DoAll(Invoke([](const String&, const String&, const String& outputPath) {
            std::ofstream file(outputPath.CStr());
            file << "{}";
            file.close();

            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockOCISpec, LoadImageManifest(_, _))
        .Times(4)
        .WillRepeatedly(DoAll(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:configDigest";
            manifest.mAosService.EmplaceValue();
            manifest.mAosService->mDigest = "sha256:serviceDigest";

            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockStorage, AddItem(_)).Times(5).WillRepeatedly(Return(ErrorEnum::eNone));

    auto err = mImageManager.InstallUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(err.IsNone()) << "InstallUpdateItems should succeed";
    EXPECT_EQ(statuses.Size(), 5) << "Should return 5 statuses";

    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(statuses[i].mItemID, itemsInfo[i].mID) << "Status ID should match for item " << i;
        EXPECT_EQ(statuses[i].mVersion, itemsInfo[i].mVersion) << "Status version should match for item " << i;
        EXPECT_EQ(statuses[i].mStatuses.Size(), 1) << "Should have one image status for item " << i;
        EXPECT_EQ(statuses[i].mStatuses[0].mState, ImageStateEnum::eInstalled)
            << "Image should be installed for item " << i;
    }
}

TEST_F(ImageManagerTest, InstallUpdateItems_NewVersionCachesPrevious)
{
    StaticArray<UpdateItemInfo, 1> itemsInfo;
    // cppcheck-suppress templateRecursion
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 1>             statuses;

    {
        auto err = itemsInfo.EmplaceBack();
        ASSERT_TRUE(err.IsNone());
    }

    auto& itemInfo = itemsInfo.Back();

    itemInfo.mID      = "12345678-1234-1234-1234-123456789010";
    itemInfo.mType    = UpdateItemTypeEnum::eService;
    itemInfo.mVersion = "2.0.0";

    auto err = itemInfo.mImages.EmplaceBack();
    ASSERT_TRUE(err.IsNone());

    auto& imageInfo = itemInfo.mImages.Back();

    imageInfo.mImage.mImageID                = "87654321-4321-4321-4321-876543210980";
    imageInfo.mImage.mArchInfo.mArchitecture = "x86_64";
    imageInfo.mImage.mOSInfo.mOS             = "linux";
    imageInfo.mPath                          = "/tmp/test-image-2.0.0.tar";
    imageInfo.mSize                          = 2048;
    imageInfo.mSHA256.Clear();
    imageInfo.mSHA256.PushBack(0x02);
    imageInfo.mSHA256.PushBack(0x00);
    imageInfo.mSHA256.PushBack(0x00);

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(itemInfo.mID, _))
        // cppcheck-suppress templateRecursion
        .WillOnce(Invoke([&itemInfo](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& existingItem = items.Back();

            existingItem.mID        = itemInfo.mID;
            existingItem.mVersion   = "1.0.0";
            existingItem.mState     = storage::ItemStateEnum::eActive;
            existingItem.mPath      = "/tmp/existing-1.0.0";
            existingItem.mTotalSize = 1024;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mMockSpaceAllocator, AllocateSpace(_)).WillOnce(Invoke([this](size_t) {
        auto mockSpace = aos::MakeUnique<spaceallocator::MockSpace>(&mAllocator);
        EXPECT_CALL(*mockSpace, Accept()).WillOnce(Return(ErrorEnum::eNone));
        return RetWithError<UniquePtr<spaceallocator::SpaceItf>>(std::move(mockSpace));
    }));
    EXPECT_CALL(mMockImageDecrypter, Decrypt(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockImageDecrypter, ValidateSigns(_, _, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockFileInfoProvider, CreateFileInfo(_, _))
        .WillOnce(DoAll(Invoke([&imageInfo](const String&, fs::FileInfo& info) {
            info.mSize   = imageInfo.mSize;
            info.mSHA256 = imageInfo.mSHA256;

            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockFileServer, TranslateFilePathURL(_, _)).WillOnce(DoAll(Invoke([](const String&, String& outURL) {
        outURL = "http://test-url-2.0.0";

        return ErrorEnum::eNone;
    })));
    EXPECT_CALL(
        mMockStorage, SetItemState(itemInfo.mID, String("1.0.0"), storage::ItemState(storage::ItemStateEnum::eCached)))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockSpaceAllocator, AddOutdatedItem(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    // Metadata parsing expectations for service image
    EXPECT_CALL(mMockTmpSpaceAllocator, AllocateSpace(_)).Times(3).WillRepeatedly(Invoke([this](size_t) {
        auto mockSpace = aos::MakeUnique<spaceallocator::MockSpace>(&mAllocator);
        EXPECT_CALL(*mockSpace, Release()).WillOnce(Return(ErrorEnum::eNone));
        return RetWithError<UniquePtr<spaceallocator::SpaceItf>>(std::move(mockSpace));
    }));
    EXPECT_CALL(mMockImageUnpacker, GetUncompressedFileSize(_, _))
        .Times(3)
        .WillRepeatedly(Return(RetWithError<size_t>(128)));
    EXPECT_CALL(mMockImageUnpacker, ExtractFileFromArchive(_, _, _))
        .Times(3)
        .WillRepeatedly(DoAll(Invoke([](const String&, const String&, const String& outputPath) {
            std::ofstream file(outputPath.CStr());
            file << "{}";
            file.close();
            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockOCISpec, LoadImageManifest(_, _))
        .WillOnce(DoAll(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:configDigest";
            manifest.mAosService.EmplaceValue();
            manifest.mAosService->mDigest = "sha256:serviceDigest";
            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockStorage, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));

    auto installErr = mImageManager.InstallUpdateItems(itemsInfo, certificates, certificateChains, statuses);
    EXPECT_TRUE(installErr.IsNone()) << "InstallUpdateItems should succeed";

    EXPECT_EQ(statuses.Size(), 1) << "Should return one status";
    EXPECT_EQ(statuses[0].mItemID, itemInfo.mID) << "Status ID should match";
    EXPECT_EQ(statuses[0].mVersion, itemInfo.mVersion) << "Status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eInstalled) << "Image should be installed";
}

TEST_F(ImageManagerTest, InstallUpdateItems_NewVersionRemovesCachedVersion)
{
    StaticArray<UpdateItemInfo, 1>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 1>             statuses;

    {
        auto err = itemsInfo.EmplaceBack();
        ASSERT_TRUE(err.IsNone());
    }

    auto& itemInfo = itemsInfo.Back();

    itemInfo.mID      = "12345678-1234-1234-1234-123456789010";
    itemInfo.mType    = UpdateItemTypeEnum::eService;
    itemInfo.mVersion = "3.0.0";

    auto err = itemInfo.mImages.EmplaceBack();
    ASSERT_TRUE(err.IsNone());

    auto& imageInfo = itemInfo.mImages.Back();

    imageInfo.mImage.mImageID                = "87654321-4321-4321-4321-876543210980";
    imageInfo.mImage.mArchInfo.mArchitecture = "x86_64";
    imageInfo.mImage.mOSInfo.mOS             = "linux";
    imageInfo.mPath                          = "/tmp/test-image-3.0.0.tar";
    imageInfo.mSize                          = 3072;
    imageInfo.mSHA256.Clear();
    imageInfo.mSHA256.PushBack(0x03);
    imageInfo.mSHA256.PushBack(0x00);
    imageInfo.mSHA256.PushBack(0x00);

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(itemInfo.mID, _))
        .WillOnce(Invoke([&itemInfo](const String&, Array<storage::ItemInfo>& items) -> Error {
            auto err = items.EmplaceBack();
            if (!err.IsNone()) {
                return err;
            }

            auto& activeItem = items.Back();

            activeItem.mID        = itemInfo.mID;
            activeItem.mVersion   = "2.0.0";
            activeItem.mState     = storage::ItemStateEnum::eActive;
            activeItem.mPath      = "/tmp/active-2.0.0";
            activeItem.mTotalSize = 2048;

            err = items.EmplaceBack();
            if (!err.IsNone()) {
                return err;
            }

            auto& cachedItem = items.Back();

            cachedItem.mID        = itemInfo.mID;
            cachedItem.mVersion   = "1.0.0";
            cachedItem.mState     = storage::ItemStateEnum::eCached;
            cachedItem.mPath      = "/tmp/cached-1.0.0";
            cachedItem.mTotalSize = 1024;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mMockSpaceAllocator, AllocateSpace(_)).WillOnce(Invoke([this](size_t) {
        auto mockSpace = aos::MakeUnique<spaceallocator::MockSpace>(&mAllocator);
        EXPECT_CALL(*mockSpace, Accept()).WillOnce(Return(ErrorEnum::eNone));

        return RetWithError<UniquePtr<spaceallocator::SpaceItf>>(std::move(mockSpace));
    }));
    EXPECT_CALL(mMockImageDecrypter, Decrypt(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockImageDecrypter, ValidateSigns(_, _, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockFileInfoProvider, CreateFileInfo(_, _))
        .WillOnce(DoAll(Invoke([&imageInfo](const String&, fs::FileInfo& info) {
            info.mSize   = imageInfo.mSize;
            info.mSHA256 = imageInfo.mSHA256;

            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockFileServer, TranslateFilePathURL(_, _)).WillOnce(DoAll(Invoke([](const String&, String& outURL) {
        outURL = "http://test-url-3.0.0";

        return ErrorEnum::eNone;
    })));
    EXPECT_CALL(
        mMockStorage, SetItemState(itemInfo.mID, String("2.0.0"), storage::ItemState(storage::ItemStateEnum::eCached)))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mMockSpaceAllocator, RestoreOutdatedItem(String(itemInfo.mID))).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockSpaceAllocator, FreeSpace(1024)).WillOnce(Return());
    EXPECT_CALL(mMockStorage, RemoveItem(itemInfo.mID, String("1.0.0"))).WillOnce(Return(ErrorEnum::eNone));
    // Metadata parsing expectations for service image
    EXPECT_CALL(mMockTmpSpaceAllocator, AllocateSpace(_)).Times(3).WillRepeatedly(Invoke([this](size_t) {
        auto mockSpace = aos::MakeUnique<spaceallocator::MockSpace>(&mAllocator);
        EXPECT_CALL(*mockSpace, Release()).WillOnce(Return(ErrorEnum::eNone));
        return RetWithError<UniquePtr<spaceallocator::SpaceItf>>(std::move(mockSpace));
    }));
    EXPECT_CALL(mMockImageUnpacker, GetUncompressedFileSize(_, _))
        .Times(3)
        .WillRepeatedly(Return(RetWithError<size_t>(128)));
    EXPECT_CALL(mMockImageUnpacker, ExtractFileFromArchive(_, _, _))
        .Times(3)
        .WillRepeatedly(DoAll(Invoke([](const String&, const String&, const String& outputPath) {
            std::ofstream file(outputPath.CStr());
            file << "{}";
            file.close();
            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockOCISpec, LoadImageManifest(_, _))
        .WillOnce(DoAll(Invoke([](const String&, oci::ImageManifest& manifest) {
            manifest.mConfig.mDigest = "sha256:configDigest";
            manifest.mAosService.EmplaceValue();
            manifest.mAosService->mDigest = "sha256:serviceDigest";
            return ErrorEnum::eNone;
        })));
    EXPECT_CALL(mMockStorage, AddItem(_)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockSpaceAllocator, AddOutdatedItem(_, _, _)).WillOnce(Return(ErrorEnum::eNone));

    auto installErr = mImageManager.InstallUpdateItems(itemsInfo, certificates, certificateChains, statuses);
    EXPECT_TRUE(installErr.IsNone()) << "InstallUpdateItems should succeed";

    EXPECT_EQ(statuses.Size(), 1) << "Should return one status";
    EXPECT_EQ(statuses[0].mItemID, itemInfo.mID) << "Status ID should match";
    EXPECT_EQ(statuses[0].mVersion, itemInfo.mVersion) << "Status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eInstalled) << "Image should be installed";
}

TEST_F(ImageManagerTest, InstallUpdateItems_SameVersionAlreadyExists)
{
    StaticArray<UpdateItemInfo, 1>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 1>             statuses;

    {
        auto err = itemsInfo.EmplaceBack();
        ASSERT_TRUE(err.IsNone());
    }

    auto& itemInfo = itemsInfo.Back();

    itemInfo.mID      = "12345678-1234-1234-1234-123456789010";
    itemInfo.mType    = UpdateItemTypeEnum::eService;
    itemInfo.mVersion = "1.0.0";

    auto err = itemInfo.mImages.EmplaceBack();
    ASSERT_TRUE(err.IsNone());

    auto& imageInfo = itemInfo.mImages.Back();

    imageInfo.mImage.mImageID = "87654321-4321-4321-4321-876543210980";
    imageInfo.mPath           = "/tmp/test-image-same.tar";
    imageInfo.mSize           = 1024;
    imageInfo.mSHA256.Clear();
    imageInfo.mSHA256.PushBack(0xFF);

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(itemInfo.mID, _))
        .WillOnce(Invoke([&itemInfo](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& existingItem = items.Back();

            existingItem.mID        = itemInfo.mID;
            existingItem.mVersion   = "1.0.0"; // Same version as we're trying to install
            existingItem.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
            existingItem.mPath      = "/tmp/existing-item";
            existingItem.mTotalSize = 1024;

            return ErrorEnum::eNone;
        }));

    auto installErr = mImageManager.InstallUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(installErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(statuses.Size(), 1) << "Should return one status";
    EXPECT_EQ(statuses[0].mItemID, itemInfo.mID) << "Status ID should match";
    EXPECT_EQ(statuses[0].mVersion, itemInfo.mVersion) << "Status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eFailed) << "Image should be failed";
    EXPECT_EQ(statuses[0].mStatuses[0].mError, ErrorEnum::eAlreadyExist) << "Error should be already exist";
}

TEST_F(ImageManagerTest, InstallUpdateItems_OlderVersionWrongState)
{
    StaticArray<UpdateItemInfo, 1>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 1>             statuses;

    {
        auto err = itemsInfo.EmplaceBack();
        ASSERT_TRUE(err.IsNone());
    }

    auto& itemInfo = itemsInfo.Back();

    itemInfo.mID      = "12345678-1234-1234-1234-123456789010";
    itemInfo.mType    = UpdateItemTypeEnum::eService;
    itemInfo.mVersion = "1.0.0";

    auto err = itemInfo.mImages.EmplaceBack();
    ASSERT_TRUE(err.IsNone());

    auto& imageInfo = itemInfo.mImages.Back();

    imageInfo.mImage.mImageID = "87654321-4321-4321-4321-876543210980";
    imageInfo.mPath           = "/tmp/test-image-old.tar";
    imageInfo.mSize           = 1024;
    imageInfo.mSHA256.Clear();
    imageInfo.mSHA256.PushBack(0xFF);

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(itemInfo.mID, _))
        .WillOnce(Invoke([&itemInfo](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& existingItem = items.Back();

            existingItem.mID        = itemInfo.mID;
            existingItem.mVersion   = "2.0.0";
            existingItem.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
            existingItem.mPath      = "/tmp/existing-newer-item";
            existingItem.mTotalSize = 1024;

            return ErrorEnum::eNone;
        }));

    auto installErr = mImageManager.InstallUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(installErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(statuses.Size(), 1) << "Should return one status";
    EXPECT_EQ(statuses[0].mItemID, itemInfo.mID) << "Status ID should match";
    EXPECT_EQ(statuses[0].mVersion, itemInfo.mVersion) << "Status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eFailed) << "Image should be failed";
    EXPECT_EQ(statuses[0].mStatuses[0].mError, ErrorEnum::eWrongState) << "Error should be wrong state";
}

TEST_F(ImageManagerTest, InstallUpdateItems_DecryptionFailed)
{
    StaticArray<UpdateItemInfo, 1>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 1>             statuses;

    {
        auto err = itemsInfo.EmplaceBack();
        ASSERT_TRUE(err.IsNone());
    }

    auto& itemInfo = itemsInfo.Back();

    itemInfo.mID      = "12345678-1234-1234-1234-123456789010";
    itemInfo.mType    = UpdateItemTypeEnum::eService;
    itemInfo.mVersion = "1.0.0";

    auto err = itemInfo.mImages.EmplaceBack();
    ASSERT_TRUE(err.IsNone());

    auto& imageInfo = itemsInfo.Back().mImages.Back();

    imageInfo.mImage.mImageID = "87654321-4321-4321-4321-876543210980";
    imageInfo.mPath           = "/tmp/test-image-decrypt-fail.tar";
    imageInfo.mSize           = 1024;
    imageInfo.mSHA256.Clear();
    imageInfo.mSHA256.PushBack(0xFF);

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(itemInfo.mID, _))
        .WillOnce(Invoke([](const String&, Array<storage::ItemInfo>& items) -> Error {
            items.Clear();

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mMockSpaceAllocator, AllocateSpace(_)).WillOnce(Invoke([this](size_t) {
        auto mockSpace = aos::MakeUnique<spaceallocator::MockSpace>(&mAllocator);
        EXPECT_CALL(*mockSpace, Release()).WillOnce(Return(ErrorEnum::eNone));

        return RetWithError<UniquePtr<spaceallocator::SpaceItf>>(std::move(mockSpace));
    }));
    EXPECT_CALL(mMockImageDecrypter, Decrypt(_, _, _)).WillOnce(Return(ErrorEnum::eRuntime));
    EXPECT_CALL(mMockStatusListener, OnImageStatusChanged(imageInfo.mImage.mImageID, _))
        .WillOnce(Invoke([](const String&, const ImageStatus& status) {
            EXPECT_EQ(status.mState, ImageStateEnum::eFailed);
            EXPECT_EQ(status.mError, ErrorEnum::eRuntime);
        }));

    err = mImageManager.SubscribeListener(mMockStatusListener);
    ASSERT_TRUE(err.IsNone());

    auto installErr = mImageManager.InstallUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(installErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(statuses.Size(), 1) << "Should return one status";
    EXPECT_EQ(statuses[0].mItemID, itemInfo.mID) << "Status ID should match";
    EXPECT_EQ(statuses[0].mVersion, itemInfo.mVersion) << "Status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eFailed) << "Image should be failed";
    EXPECT_EQ(statuses[0].mStatuses[0].mError, ErrorEnum::eRuntime) << "Error should be runtime error";
}

TEST_F(ImageManagerTest, InstallUpdateItems_InvalidHashValidation)
{
    StaticArray<UpdateItemInfo, 1>               itemsInfo;
    StaticArray<crypto::CertificateInfo, 1>      certificates;
    StaticArray<crypto::CertificateChainInfo, 1> certificateChains;
    StaticArray<UpdateItemStatus, 1>             statuses;

    {
        auto err = itemsInfo.EmplaceBack();
        ASSERT_TRUE(err.IsNone());
    }

    auto& itemInfo = itemsInfo.Back();

    itemInfo.mID      = "12345678-1234-1234-1234-123456789010";
    itemInfo.mType    = UpdateItemTypeEnum::eService;
    itemInfo.mVersion = "1.0.0";

    auto err = itemInfo.mImages.EmplaceBack();
    ASSERT_TRUE(err.IsNone());

    auto& imageInfo = itemsInfo.Back().mImages.Back();

    imageInfo.mImage.mImageID = "87654321-4321-4321-4321-876543210980";
    imageInfo.mPath           = "/tmp/test-image-hash-fail.tar";
    imageInfo.mSize           = 1024;
    imageInfo.mSHA256.Clear();
    imageInfo.mSHA256.PushBack(0xAA);

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(itemInfo.mID, _))
        .WillOnce(Invoke([](const String&, Array<storage::ItemInfo>& items) -> Error {
            items.Clear();

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mMockSpaceAllocator, AllocateSpace(_)).WillOnce(Invoke([this](size_t) {
        auto mockSpace = aos::MakeUnique<spaceallocator::MockSpace>(&mAllocator);
        EXPECT_CALL(*mockSpace, Release()).WillOnce(Return(ErrorEnum::eNone));

        return RetWithError<UniquePtr<spaceallocator::SpaceItf>>(std::move(mockSpace));
    }));
    EXPECT_CALL(mMockImageDecrypter, Decrypt(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockImageDecrypter, ValidateSigns(_, _, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockFileInfoProvider, CreateFileInfo(_, _))
        .WillOnce(Invoke([&imageInfo](const String&, fs::FileInfo& info) {
            info.mSize = imageInfo.mSize;
            info.mSHA256.Clear();
            info.mSHA256.PushBack(0xBB);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mMockStatusListener, OnImageStatusChanged(imageInfo.mImage.mImageID, _))
        .WillOnce(Invoke([](const String&, const ImageStatus& status) {
            EXPECT_EQ(status.mState, ImageStateEnum::eFailed);
            EXPECT_EQ(status.mError, ErrorEnum::eInvalidChecksum);
        }));

    err = mImageManager.SubscribeListener(mMockStatusListener);
    ASSERT_TRUE(err.IsNone());

    auto installErr = mImageManager.InstallUpdateItems(itemsInfo, certificates, certificateChains, statuses);

    EXPECT_TRUE(installErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(statuses.Size(), 1) << "Should return one status";
    EXPECT_EQ(statuses[0].mItemID, itemInfo.mID) << "Status ID should match";
    EXPECT_EQ(statuses[0].mVersion, itemInfo.mVersion) << "Status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eFailed) << "Image should be failed";
    EXPECT_EQ(statuses[0].mStatuses[0].mError, ErrorEnum::eInvalidChecksum) << "Error should be invalid checksum";
}

TEST_F(ImageManagerTest, UninstallUpdateItems_ActiveToCached)
{
    StaticArray<StaticString<cIDLen>, 1> ids;
    StaticArray<UpdateItemStatus, 1>     statuses;

    auto id = "12345678-1234-1234-1234-123456789010";

    auto err = ids.EmplaceBack(id);
    ASSERT_TRUE(err.IsNone());

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(id), _))
        .WillOnce(Invoke([&id](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem = items.Back();

            activeItem.mID        = id;
            activeItem.mVersion   = "1.0.0";
            activeItem.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
            activeItem.mPath      = "/tmp/active-item";
            activeItem.mTotalSize = 1024;

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& imageItem    = activeItem.mImages.Back();
            imageItem.mImageID = "87654321-4321-4321-4321-876543210980";
            imageItem.mPath    = "/tmp/active-image";

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(
        mMockStorage, SetItemState(String(id), String("1.0.0"), storage::ItemState(storage::ItemStateEnum::eCached)))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mMockSpaceAllocator, AddOutdatedItem(_, _, _)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mMockStatusListener, OnImageStatusChanged(_, _))
        .WillOnce(Invoke([](const String&, const ImageStatus& status) {
            EXPECT_EQ(status.mState, ImageStateEnum::eRemoved);
            EXPECT_TRUE(status.mError.IsNone());
        }));

    err = mImageManager.SubscribeListener(mMockStatusListener);
    ASSERT_TRUE(err.IsNone());

    auto uninstallErr = mImageManager.UninstallUpdateItems(ids, statuses);

    EXPECT_TRUE(uninstallErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(statuses.Size(), 1) << "Should return one status";
    EXPECT_EQ(statuses[0].mItemID, id) << "Status ID should match";
    EXPECT_EQ(statuses[0].mVersion, String("1.0.0")) << "Status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eRemoved) << "Image should be removed";
}

TEST_F(ImageManagerTest, UninstallUpdateItems_CachedRemoval)
{
    StaticArray<StaticString<cIDLen>, 1> ids;
    StaticArray<UpdateItemStatus, 1>     statuses;

    auto id = "12345678-1234-1234-1234-123456789010";

    auto err = ids.EmplaceBack(id);
    ASSERT_TRUE(err.IsNone());

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(id), _))
        .WillOnce(Invoke([&id](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedItem = items.Back();

            cachedItem.mID        = id;
            cachedItem.mVersion   = "1.0.0";
            cachedItem.mState     = storage::ItemState(storage::ItemStateEnum::eCached);
            cachedItem.mPath      = "/tmp/cached-item";
            cachedItem.mTotalSize = 1024;

            if (auto err = cachedItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& imageItem    = cachedItem.mImages.Back();
            imageItem.mImageID = "87654321-4321-4321-4321-876543210980";
            imageItem.mPath    = "/tmp/cached-image";

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mMockSpaceAllocator, RestoreOutdatedItem(String(id))).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockSpaceAllocator, FreeSpace(1024)).WillOnce(Return());
    EXPECT_CALL(mMockStorage, RemoveItem(String(id), String("1.0.0"))).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockStatusListener, OnImageStatusChanged(_, _))
        .WillOnce(Invoke([](const String&, const ImageStatus& status) {
            EXPECT_EQ(status.mState, ImageStateEnum::eRemoved);
            EXPECT_TRUE(status.mError.IsNone());
        }));
    EXPECT_CALL(mMockStatusListener, OnUpdateItemRemoved(String(id))).WillOnce(Return());

    err = mImageManager.SubscribeListener(mMockStatusListener);
    ASSERT_TRUE(err.IsNone());

    auto uninstallErr = mImageManager.UninstallUpdateItems(ids, statuses);

    EXPECT_TRUE(uninstallErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(statuses.Size(), 1) << "Should return one status";
    EXPECT_EQ(statuses[0].mItemID, id) << "Status ID should match";
    EXPECT_EQ(statuses[0].mVersion, String("1.0.0")) << "Status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eRemoved) << "Image should be removed";
}

TEST_F(ImageManagerTest, RevertUpdateItems_ActiveRemovedCachedActivated)
{
    StaticArray<StaticString<cIDLen>, 1> ids;
    StaticArray<UpdateItemStatus, 2>     statuses;

    auto id = "12345678-1234-1234-1234-123456789010";

    auto err = ids.EmplaceBack(id);
    ASSERT_TRUE(err.IsNone());

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(id), _))
        .WillOnce(Invoke([&id](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem      = items.Back();
            activeItem.mID        = id;
            activeItem.mVersion   = "2.0.0";
            activeItem.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
            activeItem.mPath      = "/tmp/active-item";
            activeItem.mTotalSize = 2048;

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeImageItem    = activeItem.mImages.Back();
            activeImageItem.mImageID = "87654321-4321-4321-4321-876543210980";
            activeImageItem.mPath    = "/tmp/active-image";

            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedItem      = items.Back();
            cachedItem.mID        = id;
            cachedItem.mVersion   = "1.0.0";
            cachedItem.mState     = storage::ItemState(storage::ItemStateEnum::eCached);
            cachedItem.mPath      = "/tmp/cached-item";
            cachedItem.mTotalSize = 1024;

            if (auto err = cachedItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedImageItem    = cachedItem.mImages.Back();
            cachedImageItem.mImageID = "87654321-4321-4321-4321-876543210981";
            cachedImageItem.mPath    = "/tmp/cached-image";

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mMockSpaceAllocator, RestoreOutdatedItem(String(id))).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockSpaceAllocator, FreeSpace(2048)).WillOnce(Return());
    EXPECT_CALL(mMockStorage, RemoveItem(String(id), String("2.0.0"))).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(
        mMockStorage, SetItemState(String(id), String("1.0.0"), storage::ItemState(storage::ItemStateEnum::eActive)))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mMockStatusListener, OnImageStatusChanged(_, _))
        .Times(2)
        .WillOnce(Invoke([](const String&, const ImageStatus& status) {
            EXPECT_EQ(status.mState, ImageStateEnum::eRemoved);
            EXPECT_TRUE(status.mError.IsNone());
        }))
        .WillOnce(Invoke([](const String&, const ImageStatus& status) {
            EXPECT_EQ(status.mState, ImageStateEnum::eInstalled);
            EXPECT_TRUE(status.mError.IsNone());
        }));
    EXPECT_CALL(mMockStatusListener, OnUpdateItemRemoved(String(id))).WillOnce(Return());

    err = mImageManager.SubscribeListener(mMockStatusListener);
    ASSERT_TRUE(err.IsNone());

    auto revertErr = mImageManager.RevertUpdateItems(ids, statuses);

    EXPECT_TRUE(revertErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(statuses.Size(), 2) << "Should return two statuses";

    EXPECT_EQ(statuses[0].mItemID, id) << "First status ID should match";
    EXPECT_EQ(statuses[0].mVersion, String("2.0.0")) << "First status should be active version";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eRemoved) << "Active version should be removed";

    EXPECT_EQ(statuses[1].mItemID, id) << "Second status ID should match";
    EXPECT_EQ(statuses[1].mVersion, String("1.0.0")) << "Second status should be cached version";
    EXPECT_EQ(statuses[1].mStatuses.Size(), 1) << "Should have one image status";
    EXPECT_EQ(statuses[1].mStatuses[0].mState, ImageStateEnum::eInstalled) << "Cached version should be installed";
}

TEST_F(ImageManagerTest, GetUpdateItemsStatuses_Success)
{
    StaticArray<UpdateItemStatus, 4> statuses;

    EXPECT_CALL(mMockStorage, GetItemsInfo(_)).WillOnce(Invoke([](Array<storage::ItemInfo>& items) -> Error {
        if (auto err = items.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& activeItem1 = items.Back();

        activeItem1.mID        = "11111111-1111-1111-1111-111111111111";
        activeItem1.mVersion   = "1.0.0";
        activeItem1.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
        activeItem1.mPath      = "/tmp/active-item-1";
        activeItem1.mTotalSize = 1024;

        if (auto err = activeItem1.mImages.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& imageItem1 = activeItem1.mImages.Back();

        imageItem1.mImageID = "22222222-2222-2222-2222-222222222222";
        imageItem1.mPath    = "/tmp/active-image-1";

        if (auto err = items.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& cachedItem = items.Back();

        cachedItem.mID        = "33333333-3333-3333-3333-333333333333";
        cachedItem.mVersion   = "0.5.0";
        cachedItem.mState     = storage::ItemState(storage::ItemStateEnum::eCached);
        cachedItem.mPath      = "/tmp/cached-item";
        cachedItem.mTotalSize = 512;

        if (auto err = items.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& activeItem2 = items.Back();

        activeItem2.mID        = "44444444-4444-4444-4444-444444444444";
        activeItem2.mVersion   = "2.0.0";
        activeItem2.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
        activeItem2.mPath      = "/tmp/active-item-2";
        activeItem2.mTotalSize = 2048;

        for (int i = 0; i < 2; ++i) {
            if (auto err = activeItem2.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& imageItem = activeItem2.mImages.Back();

            imageItem.mImageID = ("5555555" + std::to_string(i) + "-5555-5555-5555-555555555555").c_str();
            imageItem.mPath    = ("/tmp/active-image-2-" + std::to_string(i)).c_str();
        }

        return ErrorEnum::eNone;
    }));

    auto err = mImageManager.GetUpdateItemsStatuses(statuses);

    EXPECT_TRUE(err.Is(ErrorEnum::eNone));
    EXPECT_EQ(statuses.Size(), 2) << "Should return 2 active items (cached item ignored)";

    auto expectedId1 = "11111111-1111-1111-1111-111111111111";
    EXPECT_EQ(statuses[0].mItemID, expectedId1) << "First status ID should match";
    EXPECT_EQ(statuses[0].mVersion, String("1.0.0")) << "First status version should match";
    EXPECT_EQ(statuses[0].mStatuses.Size(), 1) << "First item should have one image";
    EXPECT_EQ(statuses[0].mStatuses[0].mState, ImageStateEnum::eInstalled) << "First image should be installed";

    auto expectedImageId1 = "22222222-2222-2222-2222-222222222222";
    EXPECT_EQ(statuses[0].mStatuses[0].mImageID, expectedImageId1) << "First image ID should match";

    auto expectedId2 = "44444444-4444-4444-4444-444444444444";
    EXPECT_EQ(statuses[1].mItemID, expectedId2) << "Second status ID should match";
    EXPECT_EQ(statuses[1].mVersion, String("2.0.0")) << "Second status version should match";
    EXPECT_EQ(statuses[1].mStatuses.Size(), 2) << "Second item should have two images";

    for (size_t i = 0; i < 2; ++i) {
        EXPECT_EQ(statuses[1].mStatuses[i].mState, ImageStateEnum::eInstalled)
            << "Second item image " << i << " should be installed";

        auto expectedImageId = ("5555555" + std::to_string(i) + "-5555-5555-5555-555555555555");

        EXPECT_EQ(statuses[1].mStatuses[i].mImageID, expectedImageId.c_str())
            << "Second item image " << i << " ID should match";
    }
}

TEST_F(ImageManagerTest, GetUpdateImageInfo_Success)
{
    auto itemId = "11111111-1111-1111-1111-111111111111";

    PlatformInfo platform;
    platform.mArchInfo.mArchitecture = "x86_64";
    platform.mOSInfo.mOS             = "linux";

    smcontroller::UpdateImageInfo info;

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&itemId](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem = items.Back();

            activeItem.mID        = itemId;
            activeItem.mVersion   = "1.0.0";
            activeItem.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
            activeItem.mPath      = "/tmp/active-item";
            activeItem.mTotalSize = 1024;

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& imageItem = activeItem.mImages.Back();

            imageItem.mImageID = "22222222-2222-2222-2222-222222222222";
            imageItem.mPath    = "/tmp/active-image";
            imageItem.mURL     = "http://test-url/image.tar";
            imageItem.mSize    = 2048;
            imageItem.mSHA256.Clear();
            imageItem.mSHA256.PushBack(0xAB);
            imageItem.mSHA256.PushBack(0xCD);

            imageItem.mArchInfo.mArchitecture = "x86_64";
            imageItem.mOSInfo.mOS             = "linux";

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& nonMatchingImage = activeItem.mImages.Back();

            nonMatchingImage.mImageID                = "33333333-3333-3333-3333-333333333333";
            nonMatchingImage.mPath                   = "/tmp/non-matching-image";
            nonMatchingImage.mURL                    = "http://test-url/other.tar";
            nonMatchingImage.mSize                   = 1024;
            nonMatchingImage.mArchInfo.mArchitecture = "arm64";
            nonMatchingImage.mOSInfo.mOS             = "linux";

            return ErrorEnum::eNone;
        }));

    auto getErr = mImageManager.GetUpdateImageInfo(itemId, platform, info);

    EXPECT_TRUE(getErr.Is(ErrorEnum::eNone));

    auto expectedImageId = "22222222-2222-2222-2222-222222222222";
    EXPECT_EQ(info.mImageID, expectedImageId) << "Should return matching image ID";
    EXPECT_EQ(info.mVersion, String("1.0.0")) << "Should return item version";
    EXPECT_EQ(info.mURL, String("http://test-url/image.tar")) << "Should return image URL";
    EXPECT_EQ(info.mSize, 2048) << "Should return image size";
    EXPECT_EQ(info.mSHA256.Size(), 2) << "Should return SHA256 hash";
    EXPECT_EQ(info.mSHA256[0], 0xAB) << "Should return correct SHA256[0]";
    EXPECT_EQ(info.mSHA256[1], 0xCD) << "Should return correct SHA256[1]";
}

TEST_F(ImageManagerTest, GetUpdateImageInfo_NotFound_NoActiveItem)
{
    auto itemId = "11111111-1111-1111-1111-111111111111";

    PlatformInfo platform;
    platform.mArchInfo.mArchitecture = "x86_64";
    platform.mOSInfo.mOS             = "linux";

    smcontroller::UpdateImageInfo info;

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&itemId](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedItem = items.Back();

            cachedItem.mID        = itemId;
            cachedItem.mVersion   = "1.0.0";
            cachedItem.mState     = storage::ItemState(storage::ItemStateEnum::eCached);
            cachedItem.mPath      = "/tmp/cached-item";
            cachedItem.mTotalSize = 1024;

            return ErrorEnum::eNone;
        }));

    auto getErr = mImageManager.GetUpdateImageInfo(itemId, platform, info);

    EXPECT_TRUE(getErr.Is(ErrorEnum::eNotFound)) << "Should return eNotFound when no active item";
}

TEST_F(ImageManagerTest, GetUpdateImageInfo_NotFound_NoPlatformMatch)
{
    auto itemId = "11111111-1111-1111-1111-111111111111";

    PlatformInfo platform;
    platform.mArchInfo.mArchitecture = "x86_64";
    platform.mOSInfo.mOS             = "linux";

    smcontroller::UpdateImageInfo info;

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&itemId](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem = items.Back();

            activeItem.mID        = itemId;
            activeItem.mVersion   = "1.0.0";
            activeItem.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
            activeItem.mPath      = "/tmp/active-item";
            activeItem.mTotalSize = 1024;

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& imageItem = activeItem.mImages.Back();

            imageItem.mImageID                = "22222222-2222-2222-2222-222222222222";
            imageItem.mPath                   = "/tmp/active-image";
            imageItem.mArchInfo.mArchitecture = "arm64";
            imageItem.mOSInfo.mOS             = "windows";

            return ErrorEnum::eNone;
        }));

    auto getErr = mImageManager.GetUpdateImageInfo(itemId, platform, info);

    EXPECT_TRUE(getErr.Is(ErrorEnum::eNotFound)) << "Should return eNotFound when no platform match";
}

TEST_F(ImageManagerTest, GetItemVersion_Success)
{
    auto itemId = "11111111-1111-1111-1111-111111111111";

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&itemId](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem = items.Back();

            activeItem.mID        = itemId;
            activeItem.mVersion   = "2.5.1";
            activeItem.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
            activeItem.mPath      = "/tmp/active-item";
            activeItem.mTotalSize = 1024;

            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedItem = items.Back();

            cachedItem.mID        = itemId;
            cachedItem.mVersion   = "1.0.0";
            cachedItem.mState     = storage::ItemState(storage::ItemStateEnum::eCached);
            cachedItem.mPath      = "/tmp/cached-item";
            cachedItem.mTotalSize = 512;

            return ErrorEnum::eNone;
        }));

    auto [version, getErr] = mImageManager.GetItemVersion(itemId);

    EXPECT_TRUE(getErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(version, String("2.5.1")) << "Should return active item version";
}

TEST_F(ImageManagerTest, GetItemVersion_NotFound_NoActiveItem)
{
    auto itemId = "11111111-1111-1111-1111-111111111111";

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&itemId](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedItem = items.Back();

            cachedItem.mID        = itemId;
            cachedItem.mVersion   = "1.0.0";
            cachedItem.mState     = storage::ItemState(storage::ItemStateEnum::eCached);
            cachedItem.mPath      = "/tmp/cached-item";
            cachedItem.mTotalSize = 512;

            return ErrorEnum::eNone;
        }));

    auto [version, getErr] = mImageManager.GetItemVersion(itemId);

    EXPECT_TRUE(getErr.Is(ErrorEnum::eNotFound)) << "Should return eNotFound when no active item";
    EXPECT_TRUE(version.IsEmpty()) << "Version should be empty on error";
}

TEST_F(ImageManagerTest, GetItemImages_Success)
{
    auto itemId = "11111111-1111-1111-1111-111111111111";

    StaticArray<ImageInfo, 5> imagesInfos;

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&itemId](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem = items.Back();

            activeItem.mID        = itemId;
            activeItem.mVersion   = "1.0.0";
            activeItem.mState     = storage::ItemState(storage::ItemStateEnum::eActive);
            activeItem.mPath      = "/tmp/active-item";
            activeItem.mTotalSize = 1024;

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& imageItem1 = activeItem.mImages.Back();

            imageItem1.mImageID                = "22222222-2222-2222-2222-222222222222";
            imageItem1.mPath                   = "/tmp/active-image-1";
            imageItem1.mArchInfo.mArchitecture = "x86_64";
            imageItem1.mOSInfo.mOS             = "linux";

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& imageItem2 = activeItem.mImages.Back();

            imageItem2.mImageID                = "33333333-3333-3333-3333-333333333333";
            imageItem2.mPath                   = "/tmp/active-image-2";
            imageItem2.mArchInfo.mArchitecture = "arm64";
            imageItem2.mOSInfo.mOS             = "linux";

            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedItem = items.Back();

            cachedItem.mID        = itemId;
            cachedItem.mVersion   = "0.5.0";
            cachedItem.mState     = storage::ItemState(storage::ItemStateEnum::eCached);
            cachedItem.mPath      = "/tmp/cached-item";
            cachedItem.mTotalSize = 512;

            if (auto err = cachedItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedImageItem = cachedItem.mImages.Back();

            cachedImageItem.mImageID                = "44444444-4444-4444-4444-444444444444";
            cachedImageItem.mPath                   = "/tmp/cached-image";
            cachedImageItem.mArchInfo.mArchitecture = "x86_64";
            cachedImageItem.mOSInfo.mOS             = "windows";

            return ErrorEnum::eNone;
        }));

    auto getErr = mImageManager.GetItemImages(itemId, imagesInfos);

    EXPECT_TRUE(getErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(imagesInfos.Size(), 2) << "Should return 2 images from active item only";

    auto expectedImageId1 = "22222222-2222-2222-2222-222222222222";
    EXPECT_EQ(imagesInfos[0].mImageID, expectedImageId1) << "First image ID should match";
    EXPECT_EQ(imagesInfos[0].mArchInfo.mArchitecture, String("x86_64")) << "First image arch should match";
    EXPECT_EQ(imagesInfos[0].mOSInfo.mOS, String("linux")) << "First image OS should match";

    auto expectedImageId2 = "33333333-3333-3333-3333-333333333333";
    EXPECT_EQ(imagesInfos[1].mImageID, expectedImageId2) << "Second image ID should match";
    EXPECT_EQ(imagesInfos[1].mArchInfo.mArchitecture, String("arm64")) << "Second image arch should match";
    EXPECT_EQ(imagesInfos[1].mOSInfo.mOS, String("linux")) << "Second image OS should match";
}

TEST_F(ImageManagerTest, GetItemImages_NoActiveItems)
{
    auto itemId = "11111111-1111-1111-1111-111111111111";

    StaticArray<ImageInfo, 5> imagesInfos;

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&itemId](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedItem = items.Back();

            cachedItem.mID        = itemId;
            cachedItem.mVersion   = "1.0.0";
            cachedItem.mState     = storage::ItemState(storage::ItemStateEnum::eCached);
            cachedItem.mPath      = "/tmp/cached-item";
            cachedItem.mTotalSize = 512;

            if (auto err = cachedItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& cachedImageItem = cachedItem.mImages.Back();

            cachedImageItem.mImageID                = "44444444-4444-4444-4444-444444444444";
            cachedImageItem.mPath                   = "/tmp/cached-image";
            cachedImageItem.mArchInfo.mArchitecture = "x86_64";
            cachedImageItem.mOSInfo.mOS             = "windows";

            return ErrorEnum::eNone;
        }));

    auto getErr = mImageManager.GetItemImages(itemId, imagesInfos);

    EXPECT_TRUE(getErr.Is(ErrorEnum::eNone));
    EXPECT_EQ(imagesInfos.Size(), 0) << "Should return no images when no active items";
}

TEST_F(ImageManagerTest, GetServiceConfig_Success)
{
    auto itemId  = "99999999-9999-9999-9999-999999999999";
    auto imageId = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem    = items.Back();
            activeItem.mID      = itemId;
            activeItem.mVersion = "1.0.0";
            activeItem.mType    = UpdateItemTypeEnum::eService;
            activeItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& image    = activeItem.mImages.Back();
            image.mImageID = imageId;

            // Front: image spec data, Back: service config data
            if (auto err = image.mMetadata.EmplaceBack(); !err.IsNone()) {
                return err;
            }
            image.mMetadata.Back() = "image-spec-data";
            if (auto err = image.mMetadata.EmplaceBack(); !err.IsNone()) {
                return err;
            }
            image.mMetadata.Back() = "service-config-data";

            // Add layer item to cover filter case
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& layerItem    = items.Back();
            layerItem.mID      = "layer-id-1111-1111-1111-111111111111";
            layerItem.mVersion = "1.0.0";
            layerItem.mType    = UpdateItemTypeEnum::eLayer;
            layerItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

            if (auto err = layerItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& layerImage    = layerItem.mImages.Back();
            layerImage.mImageID = "layer-image-id-aaaa-aaaa-aaaaaaaaaaaa";

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mMockOCISpec, ServiceConfigFromJSON(String("service-config-data"), _))
        .WillOnce(DoAll(Invoke([](const String&, oci::ServiceConfig& svc) {
            svc.mAuthor             = "author";
            svc.mSkipResourceLimits = true;
            svc.mBalancingPolicy    = "rr";
            auto err                = svc.mRunners.EmplaceBack();
            if (err.IsNone()) {
                svc.mRunners.Back() = "runc";
            }
            return ErrorEnum::eNone;
        })));

    oci::ServiceConfig cfg;
    auto               err = mImageManager.GetServiceConfig(itemId, imageId, cfg);

    EXPECT_TRUE(err.Is(ErrorEnum::eNone));
    EXPECT_EQ(cfg.mAuthor, String("author"));
    EXPECT_TRUE(cfg.mSkipResourceLimits);
    EXPECT_EQ(cfg.mBalancingPolicy, String("rr"));
    EXPECT_EQ(cfg.mRunners.Size(), 1);
    EXPECT_EQ(cfg.mRunners[0], String("runc"));
}

TEST_F(ImageManagerTest, GetServiceConfig_NotFound_NoMetadata)
{
    auto itemId  = "99999999-9999-9999-9999-999999999999";
    auto imageId = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem    = items.Back();
            activeItem.mID      = itemId;
            activeItem.mVersion = "1.0.0";
            activeItem.mType    = UpdateItemTypeEnum::eService;
            activeItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }
            activeItem.mImages.Back().mImageID = imageId;

            return ErrorEnum::eNone;
        }));

    oci::ServiceConfig cfg;
    auto               err = mImageManager.GetServiceConfig(itemId, imageId, cfg);

    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));
}

TEST_F(ImageManagerTest, GetImageConfig_Success)
{
    auto itemId  = "11112222-3333-4444-5555-666677778888";
    auto imageId = "cccccccc-cccc-cccc-cccc-cccccccccccc";

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem    = items.Back();
            activeItem.mID      = itemId;
            activeItem.mVersion = "2.1.0";
            activeItem.mType    = UpdateItemTypeEnum::eService;
            activeItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& image    = activeItem.mImages.Back();
            image.mImageID = imageId;

            // Front: image spec data, Back: service config data
            if (auto err = image.mMetadata.EmplaceBack(); !err.IsNone()) {
                return err;
            }
            image.mMetadata.Back() = "image-spec-data";
            if (auto err = image.mMetadata.EmplaceBack(); !err.IsNone()) {
                return err;
            }
            image.mMetadata.Back() = "service-config-data";

            // Add layer item to cover filter case
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& layerItem    = items.Back();
            layerItem.mID      = "layer-id-0000-0000-0000-000000000000";
            layerItem.mVersion = "1.0.0";
            layerItem.mType    = UpdateItemTypeEnum::eLayer;
            layerItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

            if (auto err = layerItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& layerImage    = layerItem.mImages.Back();
            layerImage.mImageID = "layer-image-id-cccc-cccc-cccccccccccc";

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mMockOCISpec, ImageSpecFromJSON(String("image-spec-data"), _))
        .WillOnce(DoAll(Invoke([](const String&, oci::ImageSpec& spec) {
            spec.mConfig.mWorkingDir = "/work";
            auto err                 = spec.mConfig.mEnv.EmplaceBack();
            if (err.IsNone()) {
                spec.mConfig.mEnv.Back() = "A=1";
            }
            err = spec.mConfig.mCmd.EmplaceBack();
            if (err.IsNone()) {
                spec.mConfig.mCmd.Back() = "run";
            }
            return ErrorEnum::eNone;
        })));

    oci::ImageConfig cfg;
    auto             err = mImageManager.GetImageConfig(itemId, imageId, cfg);

    EXPECT_TRUE(err.Is(ErrorEnum::eNone));
    EXPECT_EQ(cfg.mWorkingDir, String("/work"));
    EXPECT_EQ(cfg.mEnv.Size(), 1);
    EXPECT_EQ(cfg.mEnv[0], String("A=1"));
    EXPECT_EQ(cfg.mCmd.Size(), 1);
    EXPECT_EQ(cfg.mCmd[0], String("run"));
}

TEST_F(ImageManagerTest, GetImageConfig_NotFound_NoMetadata)
{
    auto itemId  = "11112222-3333-4444-5555-666677778888";
    auto imageId = "dddddddd-dddd-dddd-dddd-dddddddddddd";

    EXPECT_CALL(mMockStorage, GetItemVersionsByID(String(itemId), _))
        .WillOnce(Invoke([&](const String&, Array<storage::ItemInfo>& items) -> Error {
            if (auto err = items.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            auto& activeItem    = items.Back();
            activeItem.mID      = itemId;
            activeItem.mVersion = "2.1.0";
            activeItem.mType    = UpdateItemTypeEnum::eService;
            activeItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

            if (auto err = activeItem.mImages.EmplaceBack(); !err.IsNone()) {
                return err;
            }
            activeItem.mImages.Back().mImageID = imageId;

            return ErrorEnum::eNone;
        }));

    oci::ImageConfig cfg;
    auto             err = mImageManager.GetImageConfig(itemId, imageId, cfg);

    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));
}

TEST_F(ImageManagerTest, GetLayerImageInfo_Success)
{
    std::string layerDigest = "abcd1234567890abcdef";

    smcontroller::UpdateImageInfo info;

    EXPECT_CALL(mMockStorage, GetItemsInfo(_)).WillOnce(Invoke([&](Array<storage::ItemInfo>& items) -> Error {
        if (auto err = items.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& serviceItem    = items.Back();
        serviceItem.mID      = "service-id-1111-1111-1111-111111111111";
        serviceItem.mVersion = "1.0.0";
        serviceItem.mType    = UpdateItemTypeEnum::eService;
        serviceItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

        if (auto err = serviceItem.mImages.EmplaceBack(); !err.IsNone()) {
            return err;
        }
        serviceItem.mImages.Back().mImageID = "service-image-id";

        if (auto err = items.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& layerItem    = items.Back();
        layerItem.mID      = "layer-id-2222-2222-2222-222222222222";
        layerItem.mVersion = "2.0.0";
        layerItem.mType    = UpdateItemTypeEnum::eLayer;
        layerItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

        if (auto err = layerItem.mImages.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& layerImage    = layerItem.mImages.Back();
        layerImage.mImageID = "layer-image-id-3333-3333-333333333333";
        layerImage.mURL     = "http://test-layer-url/layer.tar";
        layerImage.mSize    = 4096;
        layerImage.mSHA256.Clear();
        layerImage.mSHA256.PushBack(0xDE);
        layerImage.mSHA256.PushBack(0xAD);
        layerImage.mSHA256.PushBack(0xBE);
        layerImage.mSHA256.PushBack(0xEF);

        if (auto err = layerImage.mMetadata.EmplaceBack(); !err.IsNone()) {
            return err;
        }
        layerImage.mMetadata.Back() = "layer-descriptor-data";

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mMockOCISpec, ContentDescriptorFromJSON(String("layer-descriptor-data"), _))
        .WillOnce(DoAll(Invoke([&layerDigest](const String&, oci::ContentDescriptor& descriptor) {
            descriptor.mMediaType = "application/vnd.oci.image.layer.v1.tar";
            descriptor.mDigest    = ("sha256:" + layerDigest).c_str();
            descriptor.mSize      = 4096;

            return ErrorEnum::eNone;
        })));

    auto err = mImageManager.GetLayerImageInfo(layerDigest.c_str(), info);

    EXPECT_TRUE(err.Is(ErrorEnum::eNone));
    EXPECT_EQ(info.mImageID, String("layer-image-id-3333-3333-333333333333"));
    EXPECT_EQ(info.mVersion, String("2.0.0"));
    EXPECT_EQ(info.mURL, String("http://test-layer-url/layer.tar"));
    EXPECT_EQ(info.mSize, 4096);
    EXPECT_EQ(info.mSHA256.Size(), 4);
    EXPECT_EQ(info.mSHA256[0], 0xDE);
    EXPECT_EQ(info.mSHA256[1], 0xAD);
    EXPECT_EQ(info.mSHA256[2], 0xBE);
    EXPECT_EQ(info.mSHA256[3], 0xEF);
}

TEST_F(ImageManagerTest, GetLayerImageInfo_NotFound_NoMatchingDigest)
{
    std::string layerDigest = "differentdigest1234567890";

    smcontroller::UpdateImageInfo info;

    EXPECT_CALL(mMockStorage, GetItemsInfo(_)).WillOnce(Invoke([](Array<storage::ItemInfo>& items) -> Error {
        if (auto err = items.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& layerItem    = items.Back();
        layerItem.mID      = "layer-id-4444-4444-4444-444444444444";
        layerItem.mVersion = "1.0.0";
        layerItem.mType    = UpdateItemTypeEnum::eLayer;
        layerItem.mState   = storage::ItemState(storage::ItemStateEnum::eActive);

        if (auto err = layerItem.mImages.EmplaceBack(); !err.IsNone()) {
            return err;
        }

        auto& layerImage    = layerItem.mImages.Back();
        layerImage.mImageID = "layer-image-id-5555-5555-555555555555";

        // Add metadata with different content descriptor
        if (auto err = layerImage.mMetadata.EmplaceBack(); !err.IsNone()) {
            return err;
        }
        layerImage.mMetadata.Back() = "different-layer-descriptor-data";

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mMockOCISpec, ContentDescriptorFromJSON(String("different-layer-descriptor-data"), _))
        .WillOnce(DoAll(Invoke([](const String&, oci::ContentDescriptor& descriptor) {
            descriptor.mMediaType = "application/vnd.oci.image.layer.v1.tar";
            descriptor.mDigest    = "sha256:differentdigest0000000000";
            descriptor.mSize      = 2048;

            return ErrorEnum::eNone;
        })));

    auto err = mImageManager.GetLayerImageInfo(layerDigest.c_str(), info);

    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));
}

} // namespace aos::cm::imagemanager
