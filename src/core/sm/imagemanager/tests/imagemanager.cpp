/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <core/common/tests/mocks/downloadermock.hpp>
#include <core/common/tests/mocks/fileinfoprovidermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include <core/sm/imagemanager/imagemanager.hpp>

#include "mocks/blobinfoprovidermock.hpp"
#include "mocks/imagehandlermock.hpp"
#include "stubs/storagestub.hpp"

using namespace testing;

namespace aos::sm::imagemanager {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestImagePath        = "/tmp/imagemanager_test/images";
constexpr auto cUpdateItemTTL        = 10 * Time::cSeconds;
constexpr auto cRemoveOutdatedPeriod = 5 * Time::cSeconds;

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

Error SplitDigest(const String& digest, String& alg, String& hash)
{
    StaticArray<StaticString<oci::cDigestLen>, 2> digestList;

    if (auto err = digest.Split(digestList, ':'); !err.IsNone()) {
        return err;
    }

    if (digestList.Size() != 2) {
        return ErrorEnum::eInvalidArgument;
    }

    alg  = digestList[0];
    hash = digestList[1];

    return ErrorEnum::eNone;
}

fs::FileInfo GetFileInfoByDigest(const String& digest, size_t size = 1024)
{
    fs::FileInfo                  fileInfo;
    StaticString<oci::cDigestLen> alg;
    StaticString<oci::cDigestLen> hash;

    auto err = SplitDigest(digest, alg, hash);
    EXPECT_TRUE(err.IsNone()) << "Failed to split digest: " << tests::utils::ErrorToStr(err);

    err = hash.HexToByteArray(fileInfo.mSHA256);
    EXPECT_TRUE(err.IsNone()) << "Failed to convert hash to byte array: " << tests::utils::ErrorToStr(err);

    fileInfo.mSize = size;

    return fileInfo;
};

fs::FileInfo GetFileInfoByPath(const String& path, size_t size = 1024)
{
    fs::FileInfo                  fileInfo;
    StaticString<oci::cDigestLen> hash;

    auto err = fs::BaseName(path, hash);
    EXPECT_TRUE(err.IsNone()) << "Failed to get base name: " << tests::utils::ErrorToStr(err);

    err = hash.HexToByteArray(fileInfo.mSHA256);
    EXPECT_TRUE(err.IsNone()) << "Failed to convert hash to byte array: " << tests::utils::ErrorToStr(err);

    fileInfo.mSize = size;

    return fileInfo;
};

StaticString<cURLLen> GetURL(const String& digest)
{
    StaticString<cURLLen>         url;
    StaticString<oci::cDigestLen> alg;
    StaticString<oci::cDigestLen> hash;

    auto err = SplitDigest(digest, alg, hash);
    EXPECT_TRUE(err.IsNone()) << "Failed to split digest: " << tests::utils::ErrorToStr(err);

    err = url.Format("https://main/%s/%s", alg.CStr(), hash.CStr());
    EXPECT_TRUE(err.IsNone()) << "Failed to format URL: " << tests::utils::ErrorToStr(err);

    return url;
}

StaticString<cFilePathLen> GetBlobPath(const String& digest)
{
    StaticString<cFilePathLen>    path;
    StaticString<oci::cDigestLen> alg;
    StaticString<oci::cDigestLen> hash;

    auto err = SplitDigest(digest, alg, hash);
    EXPECT_TRUE(err.IsNone()) << "Failed to split digest: " << tests::utils::ErrorToStr(err);

    path = fs::JoinPath(cTestImagePath, "blobs", alg, hash);

    return path;
}

StaticString<cFilePathLen> GetLayerPath(const String& digest)
{
    StaticString<cFilePathLen>    path;
    StaticString<oci::cDigestLen> alg;
    StaticString<oci::cDigestLen> hash;

    auto err = SplitDigest(digest, alg, hash);
    EXPECT_TRUE(err.IsNone()) << "Failed to split digest: " << tests::utils::ErrorToStr(err);

    path = fs::JoinPath(cTestImagePath, "layers", alg, hash);

    return path;
}

std::string ReadFileToString(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    EXPECT_TRUE(file.is_open()) << "Failed to open file: " << filename;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    file.close();

    return content;
}

void CreateFile(const std::string& path, const std::string& content = "")
{
    std::ofstream file(path);
    EXPECT_TRUE(file.is_open()) << "Failed to create file: " << path;

    file << content;

    file.close();
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ImageManagerTest : public Test {
protected:
    static void SetUpTestSuite()
    {
        tests::utils::InitLog();

        LOG_INF() << "Image manager size" << Log::Field("size", sizeof(ImageManager));
    }

    void SetUp() override
    {
        Config config {cTestImagePath, 0, cUpdateItemTTL, cRemoveOutdatedPeriod};

        auto err = mImageManager.Init(config, mBlobInfoProviderMock, mSpaceAllocatorMock, mDownloaderMock,
            mFileInfoProviderMock, mOCISpecMock, mImageHandlerMock, mStorageStub);
        EXPECT_TRUE(err.IsNone()) << "Failed to initialize image manager: " << tests::utils::ErrorToStr(err);

        EXPECT_CALL(mBlobInfoProviderMock, GetBlobsInfo(_, _))
            .WillRepeatedly(Invoke(
                [](const Array<StaticString<oci::cDigestLen>>& digests, Array<StaticString<cURLLen>>& urls) -> Error {
                    (void)digests;
                    (void)urls;

                    if (!digests.Size()) {
                        return ErrorEnum::eInvalidArgument;
                    }

                    if (auto err = urls.EmplaceBack(); !err.IsNone()) {
                        return err;
                    }

                    urls[0] = GetURL(digests[0]);

                    return ErrorEnum::eNone;
                }));
        EXPECT_CALL(mSpaceAllocatorMock, AllocateSpace(_))
            .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
                auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
                EXPECT_TRUE(space);

                EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
                EXPECT_CALL(*space, Release()).Times(AtLeast(0));
                EXPECT_CALL(*space, Resize(_)).Times(AtLeast(0));
                EXPECT_CALL(*space, Size()).Times(AtLeast(0));

                return {Move(space), ErrorEnum::eNone};
            }));
    }

    void TearDown() override
    {
        auto err = fs::RemoveAll(cTestImagePath);
        EXPECT_TRUE(err.IsNone()) << "Failed to remove test image path: " << tests::utils::ErrorToStr(err);
    }

    ImageManager                                           mImageManager;
    NiceMock<BlobInfoProviderMock>                         mBlobInfoProviderMock;
    NiceMock<spaceallocator::SpaceAllocatorMock>           mSpaceAllocatorMock;
    NiceMock<downloader::DownloaderMock>                   mDownloaderMock;
    NiceMock<fs::FileInfoProviderMock>                     mFileInfoProviderMock;
    NiceMock<oci::OCISpecMock>                             mOCISpecMock;
    NiceMock<ImageHandlerMock>                             mImageHandlerMock;
    StorageStub                                            mStorageStub;
    StaticAllocator<1 * sizeof(spaceallocator::SpaceMock)> mAllocator;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ImageManagerTest, InstallComponent)
{
    // Input data

    constexpr auto cManifestDigest = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    constexpr auto cLayerDigest    = "sha256:4a6f6b8f5f5e3e7b9c4d3e2f1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b";

    UpdateItemInfo itemInfo {"component1", UpdateItemTypeEnum::eComponent, "1.0.0", cManifestDigest};

    auto manifestPath = GetBlobPath(cManifestDigest);
    auto layerPath    = GetBlobPath(cLayerDigest);

    auto imageManifest = std::make_unique<oci::ImageManifest>();

    imageManifest->mConfig.mMediaType = "application/vnd.oci.empty.v1+json";
    imageManifest->mConfig.mDigest    = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
    imageManifest->mConfig.mSize      = 2;
    imageManifest->mLayers.EmplaceBack(
        oci::ContentDescriptor {"vnd.aos.image.component.full.v1+gzip", cLayerDigest, 1024});

    // Expected calls

    EXPECT_CALL(mDownloaderMock, Download(String(cManifestDigest), _, manifestPath)).Times(1);
    EXPECT_CALL(mDownloaderMock, Download(String(cLayerDigest), _, layerPath)).Times(1);
    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByDigest(cManifestDigest)), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByDigest(cLayerDigest)), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mOCISpecMock, LoadImageManifest(manifestPath, _))
        .WillOnce(DoAll(SetArgReferee<1>(*imageManifest), Return(ErrorEnum::eNone)));

    // Install update item

    auto err = mImageManager.InstallUpdateItem(itemInfo);
    EXPECT_TRUE(err.IsNone()) << "Failed to install update item: " << tests::utils::ErrorToStr(err);
}

TEST_F(ImageManagerTest, GetBlobPath)
{
    constexpr auto cDigest  = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    auto           blobPath = GetBlobPath(cDigest);

    bool created = std::filesystem::create_directories(std::filesystem::path(blobPath.CStr()).parent_path());
    EXPECT_TRUE(created) << "Failed to create blob directory at path: " << blobPath.CStr();

    CreateFile(blobPath.CStr());

    StaticString<cFilePathLen> path;

    auto err = mImageManager.GetBlobPath(cDigest, path);
    EXPECT_TRUE(err.IsNone()) << "Failed to get blob path: " << tests::utils::ErrorToStr(err);
    EXPECT_EQ(path, blobPath);
}

TEST_F(ImageManagerTest, InstallService)
{
    // Input data

    constexpr auto cManifestDigest      = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    constexpr auto cImageConfigDigest   = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
    constexpr auto cServiceConfigDigest = "sha256:abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
    constexpr auto cLayerDigest         = "sha256:4a6f6b8f5f5e3e7b9c4d3e2f1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b";
    constexpr auto cDiffDigest          = "sha256:0f9e8d7c6b5a4b3c2b1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b1a0f9e";
    constexpr auto cUnpackedLayerDigest = "sha256:9e8d7c6b5a4b3c2b1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b1a0f9e8d";
    constexpr auto cUnpackedLayerSize   = 2048;

    UpdateItemInfo itemInfo {"service1", UpdateItemTypeEnum::eService, "1.0.0", cManifestDigest};

    auto manifestPath      = GetBlobPath(cManifestDigest);
    auto imageConfigPath   = GetBlobPath(cImageConfigDigest);
    auto serviceConfigPath = GetBlobPath(cServiceConfigDigest);
    auto layerBlobPath     = GetBlobPath(cLayerDigest);
    auto layerUnpackedPath = fs::JoinPath(GetLayerPath(cDiffDigest), "layer");

    auto imageManifest = std::make_unique<oci::ImageManifest>();

    imageManifest->mConfig.mMediaType = "application/vnd.oci.image.config.v1+json";
    imageManifest->mConfig.mDigest    = cImageConfigDigest;
    imageManifest->mConfig.mSize      = 512;
    imageManifest->mItemConfig.EmplaceValue(
        oci::ContentDescriptor {"application/vnd.aos.item.config.v1+json", cServiceConfigDigest, 256});
    imageManifest->mLayers.EmplaceBack(
        oci::ContentDescriptor {"application/vnd.oci.image.layer.v1.tar+gzip", cLayerDigest, 1024});

    auto imageConfig = std::make_unique<oci::ImageConfig>();

    imageConfig->mRootfs.mDiffIDs.EmplaceBack(cDiffDigest);

    // Expected calls

    EXPECT_CALL(mDownloaderMock, Download(String(cManifestDigest), _, manifestPath)).Times(1);
    EXPECT_CALL(mDownloaderMock, Download(String(cImageConfigDigest), _, imageConfigPath)).Times(1);
    EXPECT_CALL(mDownloaderMock, Download(String(cServiceConfigDigest), _, serviceConfigPath)).Times(1);
    EXPECT_CALL(mDownloaderMock, Download(String(cLayerDigest), _, layerBlobPath)).Times(1);
    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByDigest(cManifestDigest)), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByDigest(cImageConfigDigest)), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByDigest(cServiceConfigDigest)), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByDigest(cLayerDigest)), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mOCISpecMock, LoadImageManifest(manifestPath, _))
        .WillOnce(DoAll(SetArgReferee<1>(*imageManifest), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mOCISpecMock, LoadImageConfig(imageConfigPath, _))
        .WillOnce(DoAll(SetArgReferee<1>(*imageConfig), Return(ErrorEnum::eNone)));
    EXPECT_CALL(
        mImageHandlerMock, GetUnpackedLayerSize(layerBlobPath, String("application/vnd.oci.image.layer.v1.tar+gzip")))
        .WillOnce(Return(cUnpackedLayerSize));
    EXPECT_CALL(mImageHandlerMock, UnpackLayer(layerBlobPath, layerUnpackedPath, imageManifest->mLayers[0].mMediaType))
        .Times(1);
    EXPECT_CALL(mImageHandlerMock, GetUnpackedLayerDigest(layerUnpackedPath))
        .WillOnce(Return(StaticString<oci::cDigestLen>(cUnpackedLayerDigest)));

    // Install update item

    auto err = mImageManager.InstallUpdateItem(itemInfo);
    EXPECT_TRUE(err.IsNone()) << "Failed to install update item: " << tests::utils::ErrorToStr(err);

    // Check metadata

    auto diffLayerDigest = ReadFileToString(GetBlobPath(cLayerDigest).CStr());
    EXPECT_EQ(diffLayerDigest, cDiffDigest);

    auto unpackedLayerDigest = ReadFileToString(fs::JoinPath(GetLayerPath(cDiffDigest), "digest").CStr());
    EXPECT_EQ(unpackedLayerDigest, cUnpackedLayerDigest);

    auto unpackedLayerSizeStr = ReadFileToString(fs::JoinPath(GetLayerPath(cDiffDigest), "size").CStr());
    EXPECT_EQ(std::stoull(unpackedLayerSizeStr), cUnpackedLayerSize);
}

TEST_F(ImageManagerTest, GetLayerPath)
{
    constexpr auto cDigest   = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    auto           layerPath = fs::JoinPath(GetLayerPath(cDigest), "layer");

    bool created = std::filesystem::create_directories(layerPath.CStr());
    EXPECT_TRUE(created) << "Failed to create layer directory at path: " << layerPath.CStr();

    StaticString<cFilePathLen> path;

    auto err = mImageManager.GetLayerPath(cDigest, path);
    EXPECT_TRUE(err.IsNone()) << "Failed to get layer path: " << tests::utils::ErrorToStr(err);
    EXPECT_EQ(path, layerPath);
}

TEST_F(ImageManagerTest, GetAllInstalledItems)
{
    std::vector<UpdateItemInfo> installItems
        = {{"component1", UpdateItemTypeEnum::eComponent, "1.0.0",
               "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"},
            {"service1", UpdateItemTypeEnum::eService, "1.0.0",
                "sha256:abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"},
            {"component2", UpdateItemTypeEnum::eComponent, "2.0.0",
                "sha256:0f9e8d7c6b5a4b3c2b1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b1a0f9e"},
            {"service2", UpdateItemTypeEnum::eService, "2.0.0",
                "sha256:9e8d7c6b5a4b3c2b1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b1a0f9e8d"},
            {"component3", UpdateItemTypeEnum::eComponent, "3.0.0",
                "sha256:8d7c6b5a4b3c2b1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b1a0f9e8d7c"},
            {"service3", UpdateItemTypeEnum::eService, "3.0.0",
                "sha256:7c6b5a4b3c2b1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b1a0f9e8d7c6b"}};

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillRepeatedly(Invoke([](const String& path, fs::FileInfo& fileInfo) -> Error {
            fileInfo = GetFileInfoByPath(path);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([](const String& path, oci::ImageManifest& manifest) -> Error {
            (void)path;

            manifest.mConfig.mMediaType = "application/vnd.oci.image.config.v1+json";
            manifest.mConfig.mDigest    = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
            manifest.mConfig.mSize      = 512;
            manifest.mItemConfig.EmplaceValue(oci::ContentDescriptor {"application/vnd.aos.item.config.v1+json",
                "sha256:abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890", 256});
            manifest.mLayers.EmplaceBack(oci::ContentDescriptor {"application/vnd.oci.image.layer.v1.tar+gzip",
                "sha256:4a6f6b8f5f5e3e7b9c4d3e2f1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b", 1024});

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mOCISpecMock, LoadImageConfig(_, _))
        .WillRepeatedly(Invoke([](const String& path, oci::ImageConfig& imageConfig) -> Error {
            (void)path;

            imageConfig.mRootfs.mDiffIDs.EmplaceBack(
                "sha256:0f9e8d7c6b5a4b3c2b1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b1a0f9e");

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageHandlerMock, GetUnpackedLayerSize(_, String("application/vnd.oci.image.layer.v1.tar+gzip")))
        .WillRepeatedly(Return(1024));
    EXPECT_CALL(mImageHandlerMock, GetUnpackedLayerDigest(_))
        .WillRepeatedly(Return(
            StaticString<oci::cDigestLen>("sha256:9e8d7c6b5a4b3c2b1a0b9c8d7e6f5e4d3c2b1a0f9e8d7c6b5a4b3c2b1a0f9e8d")));

    // Install update item

    for (const auto& item : installItems) {
        auto err = mImageManager.InstallUpdateItem(item);
        EXPECT_TRUE(err.IsNone()) << "Failed to install item: " << tests::utils::ErrorToStr(err);
    }

    std::vector<size_t> removeIndexes = {1, 4};

    for (auto it = removeIndexes.rbegin(); it != removeIndexes.rend(); ++it) {
        auto err = mImageManager.RemoveUpdateItem(installItems[*it].mID, installItems[*it].mVersion);
        EXPECT_TRUE(err.IsNone()) << "Failed to remove item: " << tests::utils::ErrorToStr(err);

        installItems.erase(installItems.begin() + *it);
    }

    auto installedItems = std::make_unique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems>>();

    auto err = mImageManager.GetAllInstalledItems(*installedItems);
    EXPECT_TRUE(err.IsNone()) << "Failed to get all installed items: " << tests::utils::ErrorToStr(err);

    for (size_t i = 0; i < installedItems->Size(); ++i) {
        UpdateItemStatus status
            = {installItems[i].mID, installItems[i].mType, installItems[i].mVersion, ItemStateEnum::eInstalled};

        EXPECT_NE(installedItems->Find(status), installedItems->end())
            << "Installed item not found: " << (*installedItems)[i].mID.CStr() << " "
            << (*installedItems)[i].mVersion.CStr();
    }
}

TEST_F(ImageManagerTest, RemoveOutdatedItems)
{
    std::vector<UpdateItemData> initialItems = {
        {"item1", UpdateItemTypeEnum::eService, "1.0.0", "", ItemStateEnum::eRemoved,
            Time::Now().Add(-(cUpdateItemTTL + 1 * Time::cSeconds))},
        {"item2", UpdateItemTypeEnum::eService, "1.0.0", "", ItemStateEnum::eRemoved,
            Time::Now().Add(-(cUpdateItemTTL + 1 * Time::cSeconds))},
        {"item3", UpdateItemTypeEnum::eService, "1.0.0", "", ItemStateEnum::eRemoved, Time::Now()},
        {"item4", UpdateItemTypeEnum::eService, "1.0.0", "", ItemStateEnum::eRemoved, Time::Now()},
    };

    mStorageStub.Init(initialItems);

    // Expect adding outdated items to space allocator for all deleted items

    EXPECT_CALL(mSpaceAllocatorMock, AddOutdatedItem(String("item1"), String("1.0.0"), _)).Times(1);
    EXPECT_CALL(mSpaceAllocatorMock, AddOutdatedItem(String("item2"), String("1.0.0"), _)).Times(1);
    EXPECT_CALL(mSpaceAllocatorMock, AddOutdatedItem(String("item3"), String("1.0.0"), _)).Times(1);
    EXPECT_CALL(mSpaceAllocatorMock, AddOutdatedItem(String("item4"), String("1.0.0"), _)).Times(1);

    // Expect restoring outdated items for first two removed items just after start

    EXPECT_CALL(mSpaceAllocatorMock, RestoreOutdatedItem(String("item1"), String("1.0.0"))).Times(1);
    EXPECT_CALL(mSpaceAllocatorMock, RestoreOutdatedItem(String("item2"), String("1.0.0"))).Times(1);

    std::vector<std::future<UpdateItemData>> futures;

    for (size_t i = 0; i < 2; ++i) {
        futures.emplace_back(mStorageStub.GetRemoveFuture());
    }

    auto err = mImageManager.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start image manager: " << tests::utils::ErrorToStr(err);

    // Verify that first two removed items are removed just after start

    for (size_t i = 0; i < 2; ++i) {
        ASSERT_EQ(futures[i].wait_for(std::chrono::milliseconds(cUpdateItemTTL.Milliseconds() * 2)),
            std::future_status::ready);

        auto itemData = futures[i].get();

        EXPECT_TRUE(itemData.mID == "item1" || itemData.mID == "item2")
            << "Unexpected item ID: " << itemData.mID.CStr();
    }

    // Verify that last two removed items are still present in storage

    auto storedItems = std::make_unique<UpdateItemDataStaticArray>();

    err = mStorageStub.GetAllUpdateItems(*storedItems);
    EXPECT_TRUE(err.IsNone()) << "Failed to get all stored items: " << tests::utils::ErrorToStr(err);

    for (const auto& item : *storedItems) {
        EXPECT_TRUE(item.mID == "item3" || item.mID == "item4") << "Unexpected item ID: " << item.mID.CStr();
    }

    // Expect restoring outdated items for last two removed items after remove outdated period

    EXPECT_CALL(mSpaceAllocatorMock, RestoreOutdatedItem(String("item3"), String("1.0.0"))).Times(1);
    EXPECT_CALL(mSpaceAllocatorMock, RestoreOutdatedItem(String("item4"), String("1.0.0"))).Times(1);

    // Verify that last two removed items are removed after remove outdated period

    futures.clear();

    for (size_t i = 0; i < 2; ++i) {
        futures.emplace_back(mStorageStub.GetRemoveFuture());
    }

    for (size_t i = 0; i < 2; ++i) {
        ASSERT_EQ(futures[i].wait_for(std::chrono::milliseconds(cUpdateItemTTL.Milliseconds() * 2)),
            std::future_status::ready);

        auto itemData = futures[i].get();

        EXPECT_TRUE(itemData.mID == "item3" || itemData.mID == "item4")
            << "Unexpected item ID: " << itemData.mID.CStr();
    }

    // Verify that no stored items are present in storage

    storedItems->Clear();

    err = mStorageStub.GetAllUpdateItems(*storedItems);
    EXPECT_TRUE(err.IsNone()) << "Failed to get all stored items: " << tests::utils::ErrorToStr(err);
    EXPECT_TRUE(storedItems->IsEmpty()) << "Expected no stored items after removal";

    err = mImageManager.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop image manager: " << tests::utils::ErrorToStr(err);
}

TEST_F(ImageManagerTest, MaxItemVersions)
{
    UpdateItemInfo itemInfo {"service1", UpdateItemTypeEnum::eService, "1.0.0",
        "sha256:abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"};

    auto imageManifest = std::make_unique<oci::ImageManifest>();

    imageManifest->mConfig.mMediaType = "application/vnd.oci.image.config.v1+json";
    imageManifest->mConfig.mDigest    = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
    imageManifest->mConfig.mSize      = 1024;

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillRepeatedly(Invoke([](const String& path, fs::FileInfo& fileInfo) -> Error {
            fileInfo = GetFileInfoByPath(path);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(*imageManifest), Return(ErrorEnum::eNone)));

    // Install update items

    for (size_t i = 0; i < 3; ++i) {
        auto err = itemInfo.mVersion.Format("%d.0.0", i + 1);
        EXPECT_TRUE(err.IsNone()) << "Failed to format item version: " << tests::utils::ErrorToStr(err);

        err = mImageManager.InstallUpdateItem(itemInfo);
        EXPECT_TRUE(err.IsNone()) << "Failed to install update item: " << tests::utils::ErrorToStr(err);
    }

    auto storedItems = std::make_unique<UpdateItemDataStaticArray>();

    // Expect 2 last installed items

    auto err = mStorageStub.GetAllUpdateItems(*storedItems);
    EXPECT_TRUE(err.IsNone()) << "Failed to get all installed items: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(storedItems->Size(), 2) << "Unexpected number of installed items";
    EXPECT_EQ(storedItems->FindIf(
                  [&](const UpdateItemData& item) { return item.mID == "service1" && item.mVersion == "1.0.0"; }),
        storedItems->end())
        << "Unexpected installed item version 1.0.0 found";

    // Remove specific version

    err = mImageManager.RemoveUpdateItem("service1", "3.0.0");
    EXPECT_TRUE(err.IsNone()) << "Failed to remove update item: " << tests::utils::ErrorToStr(err);

    err = mImageManager.InstallUpdateItem({"service1", UpdateItemTypeEnum::eService, "4.0.0",
        "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"});
    EXPECT_TRUE(err.IsNone()) << "Failed to install update item: " << tests::utils::ErrorToStr(err);

    // Expect this version to be removed

    storedItems->Clear();

    err = mStorageStub.GetAllUpdateItems(*storedItems);
    EXPECT_TRUE(err.IsNone()) << "Failed to get all installed items: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(storedItems->Size(), 2) << "Unexpected number of installed items";
    EXPECT_EQ(storedItems->FindIf(
                  [&](const UpdateItemData& item) { return item.mID == "service1" && item.mVersion == "3.0.0"; }),
        storedItems->end())
        << "Unexpected installed item version 3.0.0 found";
}

TEST_F(ImageManagerTest, MaxStoredItems)
{
    UpdateItemInfo itemInfo {"service1", UpdateItemTypeEnum::eService, "1.0.0",
        "sha256:abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"};

    auto imageManifest = std::make_unique<oci::ImageManifest>();

    imageManifest->mConfig.mMediaType = "application/vnd.oci.image.config.v1+json";
    imageManifest->mConfig.mDigest    = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
    imageManifest->mConfig.mSize      = 1024;

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillRepeatedly(Invoke([](const String& path, fs::FileInfo& fileInfo) -> Error {
            fileInfo = GetFileInfoByPath(path);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(*imageManifest), Return(ErrorEnum::eNone)));

    // Install update items up to the max limit

    for (size_t i = 0; i < cMaxNumStoredUpdateItems; ++i) {
        auto err = itemInfo.mID.Format("service%d", i + 1);
        EXPECT_TRUE(err.IsNone()) << "Failed to format item version: " << tests::utils::ErrorToStr(err);

        err = mImageManager.InstallUpdateItem(itemInfo);
        EXPECT_TRUE(err.IsNone()) << "Failed to install update item: " << tests::utils::ErrorToStr(err);
    }

    // Install one more item to exceed the limit

    auto err = itemInfo.mID.Format("service%d", cMaxNumStoredUpdateItems + 1);
    EXPECT_TRUE(err.IsNone()) << "Failed to format item version: " << tests::utils::ErrorToStr(err);

    err = mImageManager.InstallUpdateItem(itemInfo);
    EXPECT_TRUE(err.IsNone()) << "Failed to install update item: " << tests::utils::ErrorToStr(err);

    // Expect the first installed item to be removed

    auto storedItems = std::make_unique<UpdateItemDataStaticArray>();

    err = mStorageStub.GetAllUpdateItems(*storedItems);
    EXPECT_TRUE(err.IsNone()) << "Failed to get all installed items: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(storedItems->Size(), cMaxNumStoredUpdateItems) << "Unexpected number of installed items";
    EXPECT_EQ(
        storedItems->FindIf([&](const UpdateItemData& item) { return item.mID == "service1"; }), storedItems->end())
        << "Unexpected installed item service1 found";

    // Remove specific version and install again to verify space is freed

    err = mImageManager.RemoveUpdateItem("service42", "1.0.0");
    EXPECT_TRUE(err.IsNone()) << "Failed to remove update item: " << tests::utils::ErrorToStr(err);

    err = itemInfo.mID.Format("service%d", cMaxNumStoredUpdateItems + 2);
    EXPECT_TRUE(err.IsNone()) << "Failed to format item version: " << tests::utils::ErrorToStr(err);

    err = mImageManager.InstallUpdateItem(itemInfo);
    EXPECT_TRUE(err.IsNone()) << "Failed to install update item: " << tests::utils::ErrorToStr(err);

    // Expect the removed item to be gone

    storedItems->Clear();

    err = mStorageStub.GetAllUpdateItems(*storedItems);
    EXPECT_TRUE(err.IsNone()) << "Failed to get all installed items: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(storedItems->Size(), cMaxNumStoredUpdateItems) << "Unexpected number of installed items";
    EXPECT_EQ(
        storedItems->FindIf([&](const UpdateItemData& item) { return item.mID == "service42"; }), storedItems->end())
        << "Unexpected installed item service42 found";
}

TEST_F(ImageManagerTest, ValidateIntegrity)
{
    auto itemsInfo = std::vector<UpdateItemData> {
        {"service1", UpdateItemTypeEnum::eService, "1.0.0",
            "sha256:0000000000000000000000000000000000000000000000000000000000000000", ItemStateEnum::eInstalled,
            Time::Now()},
        {"service2", UpdateItemTypeEnum::eService, "1.0.0",
            "sha256:1111111111111111111111111111111111111111111111111111111111111111", ItemStateEnum::eInstalled,
            Time::Now()},
        {"component1", UpdateItemTypeEnum::eComponent, "1.0.0",
            "sha256:2222222222222222222222222222222222222222222222222222222222222222", ItemStateEnum::eInstalled,
            Time::Now()},
        {"component2", UpdateItemTypeEnum::eComponent, "1.0.0",
            "sha256:3333333333333333333333333333333333333333333333333333333333333333", ItemStateEnum::eInstalled,
            Time::Now()},
    };

    mStorageStub.Init(itemsInfo);

    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(Invoke([&](const String& path, oci::ImageManifest& manifest) -> Error {
            auto fileName = std::filesystem::path(path.CStr()).filename().string();

            auto it = std::find_if(itemsInfo.begin(), itemsInfo.end(), [&](const UpdateItemData& item) {
                return std::string(item.mManifestDigest.CStr()).compare(7, fileName.size(), fileName) == 0;
            });
            if (it == itemsInfo.end()) {
                return ErrorEnum::eNotFound;
            }

            if (it->mType == UpdateItemTypeEnum::eService) {
                manifest.mConfig = oci::ContentDescriptor {"application/vnd.oci.image.config.v1+json",
                    "sha256:4444444444444444444444444444444444444444444444444444444444444444", 512};
            } else {
                manifest.mConfig = oci::ContentDescriptor {"application/vnd.oci.empty.v1+json",
                    "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a", 2};
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillRepeatedly(Invoke([&](const String& path, fs::FileInfo& fileInfo) -> Error {
            auto fileName = std::filesystem::path(path.CStr()).filename().string();

            auto it = std::find_if(itemsInfo.begin(), itemsInfo.end(), [&](const UpdateItemData& item) {
                return std::string(item.mManifestDigest.CStr()).compare(7, fileName.size(), fileName) == 0;
            });
            if (it != itemsInfo.end()) {
                if (it->mID == "service2" || it->mID == "component2") {
                    fileInfo.mSHA256
                        = Array<uint8_t>(reinterpret_cast<const uint8_t*>("invalidinvalidinvalidinvalidinva"), 32);
                    fileInfo.mSize = 1024;

                    return ErrorEnum::eNone;
                }
            }

            fileInfo = GetFileInfoByPath(path);

            return ErrorEnum::eNone;
        }));

    std::vector<std::future<UpdateItemData>> futures;

    for (size_t i = 0; i < 2; ++i) {
        futures.emplace_back(mStorageStub.GetRemoveFuture());
    }

    auto err = mImageManager.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start image manager: " << tests::utils::ErrorToStr(err);

    for (size_t i = 0; i < 2; ++i) {
        ASSERT_EQ(
            futures[i].wait_for(std::chrono::milliseconds(cUpdateItemTTL.Milliseconds())), std::future_status::ready);

        auto itemData = futures[i].get();

        EXPECT_TRUE(itemData.mID == "service2" || itemData.mID == "component2")
            << "Unexpected item ID: " << itemData.mID.CStr();
    }

    err = mImageManager.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop image manager: " << tests::utils::ErrorToStr(err);
}

TEST_F(ImageManagerTest, RemoveOrphanBlobs)
{
    auto itemsInfo = std::vector<UpdateItemData> {
        {"service1", UpdateItemTypeEnum::eService, "1.0.0",
            "sha256:0000000000000000000000000000000000000000000000000000000000000000", ItemStateEnum::eInstalled,
            Time::Now()},
        {"service2", UpdateItemTypeEnum::eService, "1.0.0",
            "sha256:1111111111111111111111111111111111111111111111111111111111111111", ItemStateEnum::eInstalled,
            Time::Now()},
        {"component1", UpdateItemTypeEnum::eComponent, "1.0.0",
            "sha256:2222222222222222222222222222222222222222222222222222222222222222", ItemStateEnum::eInstalled,
            Time::Now()},
        {"component2", UpdateItemTypeEnum::eComponent, "1.0.0",
            "sha256:3333333333333333333333333333333333333333333333333333333333333333", ItemStateEnum::eInstalled,
            Time::Now()},
    };

    auto imageManifest = std::make_unique<oci::ImageManifest>();

    imageManifest->mConfig.mMediaType = "application/vnd.oci.image.config.v1+json";
    imageManifest->mConfig.mDigest    = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
    imageManifest->mConfig.mSize      = 512;

    mStorageStub.Init(itemsInfo);

    auto blobsPath = fs::JoinPath(cTestImagePath, "blobs", "sha256");

    std::filesystem::create_directories(blobsPath.CStr());

    // Create image config blob

    CreateFile(GetBlobPath(imageManifest->mConfig.mDigest).CStr());

    // Create used blobs

    for (const auto& item : itemsInfo) {
        CreateFile(GetBlobPath(item.mManifestDigest).CStr());
    }

    // Create orphan blobs

    for (size_t i = 0; i < 10; i++) {
        StaticString<oci::cDigestLen> digest;

        digest.Format("sha256:%064d", i + 1);

        CreateFile(GetBlobPath(digest).CStr());
    }

    std::promise<size_t> promise;

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillRepeatedly(Invoke([&](const String& path, fs::FileInfo& fileInfo) -> Error {
            fileInfo = GetFileInfoByPath(path);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(*imageManifest), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mSpaceAllocatorMock, FreeSpace(_)).WillOnce(Invoke([&](size_t size) -> Error {
        promise.set_value(size);

        return ErrorEnum::eNone;
    }));

    auto err = mImageManager.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start image manager: " << tests::utils::ErrorToStr(err);

    ASSERT_TRUE(promise.get_future().wait_for(std::chrono::milliseconds(cUpdateItemTTL.Milliseconds()))
        == std::future_status::ready);

    err = mImageManager.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop image manager: " << tests::utils::ErrorToStr(err);

    for (const auto& entry : std::filesystem::directory_iterator(blobsPath.CStr())) {
        bool found = false;

        for (const auto& item : itemsInfo) {
            if (entry.path().c_str() == GetBlobPath(item.mManifestDigest)) {
                found = true;

                break;
            }
        }

        if (!found) {
            if (entry.path().c_str() == GetBlobPath(imageManifest->mConfig.mDigest)) {
                found = true;
            }
        }

        EXPECT_TRUE(found) << "Orphan blob not removed: " << entry.path().c_str();
    }
}

TEST_F(ImageManagerTest, RemoveOrphanLayers)
{
    auto itemsInfo = std::vector<UpdateItemData> {
        {"service1", UpdateItemTypeEnum::eService, "1.0.0",
            "sha256:0000000000000000000000000000000000000000000000000000000000000000", ItemStateEnum::eInstalled,
            Time::Now()},
        {"service2", UpdateItemTypeEnum::eService, "1.0.0",
            "sha256:1111111111111111111111111111111111111111111111111111111111111111", ItemStateEnum::eInstalled,
            Time::Now()},
        {"service3", UpdateItemTypeEnum::eService, "1.0.0",
            "sha256:2222222222222222222222222222222222222222222222222222222222222222", ItemStateEnum::eInstalled,
            Time::Now()},
        {"service4", UpdateItemTypeEnum::eService, "1.0.0",
            "sha256:3333333333333333333333333333333333333333333333333333333333333333", ItemStateEnum::eInstalled,
            Time::Now()},
    };

    auto imageManifest = std::make_unique<oci::ImageManifest>();

    imageManifest->mConfig.mMediaType = "application/vnd.oci.image.config.v1+json";
    imageManifest->mConfig.mDigest    = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
    imageManifest->mConfig.mSize      = 512;
    imageManifest->mLayers.EmplaceBack(oci::ContentDescriptor {"vnd.aos.image.component.full.v1+gzip",
        "sha256:4444444444444444444444444444444444444444444444444444444444444444", 1024});

    auto imageConfig = std::make_unique<oci::ImageConfig>();

    imageConfig->mRootfs.mDiffIDs.EmplaceBack(
        "sha256:5555555555555555555555555555555555555555555555555555555555555555");

    mStorageStub.Init(itemsInfo);

    auto blobsPath  = fs::JoinPath(cTestImagePath, "blobs", "sha256");
    auto layersPath = fs::JoinPath(cTestImagePath, "layers", "sha256");

    std::filesystem::create_directories(blobsPath.CStr());
    std::filesystem::create_directories(layersPath.CStr());

    // Create image config blob

    CreateFile(GetBlobPath(imageManifest->mConfig.mDigest).CStr());

    // Create layer blob

    CreateFile(GetBlobPath(imageManifest->mLayers[0].mDigest).CStr(), imageConfig->mRootfs.mDiffIDs[0].CStr());

    // Create used layers

    auto layerPath = GetLayerPath(imageConfig->mRootfs.mDiffIDs[0]);

    std::filesystem::create_directories(layerPath.CStr());

    // Create layer digest

    CreateFile(fs::JoinPath(layerPath, "digest").CStr(), imageConfig->mRootfs.mDiffIDs[0].CStr());

    // Create orphan layers

    for (size_t i = 0; i < 10; i++) {
        StaticString<oci::cDigestLen> digest;

        digest.Format("sha256:%064d", i + 1);

        auto layerPath = GetLayerPath(digest);

        std::filesystem::create_directories(layerPath.CStr());
    }

    std::promise<size_t> promise;

    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillRepeatedly(Invoke([&](const String& path, fs::FileInfo& fileInfo) -> Error {
            fileInfo = GetFileInfoByPath(path);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mOCISpecMock, LoadImageManifest(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(*imageManifest), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mSpaceAllocatorMock, FreeSpace(_)).WillOnce(Invoke([&](size_t size) -> Error {
        promise.set_value(size);

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mOCISpecMock, LoadImageConfig(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(*imageConfig), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mImageHandlerMock, GetUnpackedLayerDigest(_))
        .WillRepeatedly(Return(StaticString<oci::cDigestLen>(imageConfig->mRootfs.mDiffIDs[0])));

    auto err = mImageManager.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start image manager: " << tests::utils::ErrorToStr(err);

    ASSERT_TRUE(promise.get_future().wait_for(std::chrono::milliseconds(cUpdateItemTTL.Milliseconds()))
        == std::future_status::ready);

    err = mImageManager.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop image manager: " << tests::utils::ErrorToStr(err);

    for (const auto& entry : std::filesystem::directory_iterator(layersPath.CStr())) {
        EXPECT_EQ(GetLayerPath(imageConfig->mRootfs.mDiffIDs[0]), entry.path().c_str())
            << "Orphan layer not removed: " << entry.path().c_str();
    }
}

} // namespace aos::sm::imagemanager
