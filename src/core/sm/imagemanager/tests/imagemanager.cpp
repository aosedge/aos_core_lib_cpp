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

using namespace testing;

namespace aos::sm::imagemanager {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestImagePath = "/tmp/imagemanager_test/images";

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

fs::FileInfo GetFileInfo(const String& digest, size_t size = 1024)
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
        Config config {cTestImagePath, 0, 5 * Time::cSeconds, 1 * Time::cSeconds};

        auto err = mImageManager.Init(
            config, mBlobInfoProviderMock, mSpaceAllocatorMock, mDownloaderMock, mFileInfoProviderMock, mOCISpecMock);
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
    }

    void TearDown() override
    {
        auto err = fs::RemoveAll(cTestImagePath);
        EXPECT_TRUE(err.IsNone()) << "Failed to remove test image path: " << tests::utils::ErrorToStr(err);
    }

    ImageManager                                       mImageManager;
    NiceMock<BlobInfoProviderMock>                     mBlobInfoProviderMock;
    NiceMock<spaceallocator::SpaceAllocatorMock>       mSpaceAllocatorMock;
    NiceMock<downloader::DownloaderMock>               mDownloaderMock;
    NiceMock<fs::FileInfoProviderMock>                 mFileInfoProviderMock;
    NiceMock<oci::OCISpecMock>                         mOCISpecMock;
    StaticAllocator<sizeof(spaceallocator::SpaceMock)> mAllocator;
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
        oci::ContentDescriptor {"application/vnd.oci.image.layer.v1.tar+gzip", cLayerDigest, 1024});

    // Expected calls

    EXPECT_CALL(mDownloaderMock, Download(String(cManifestDigest), _, manifestPath)).Times(1);
    EXPECT_CALL(mDownloaderMock, Download(String(cLayerDigest), _, layerPath)).Times(1);
    EXPECT_CALL(mFileInfoProviderMock, GetFileInfo(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfo(cManifestDigest)), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfo(cLayerDigest)), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mOCISpecMock, LoadImageManifest(manifestPath, _))
        .WillOnce(DoAll(SetArgReferee<1>(*imageManifest), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mSpaceAllocatorMock, AllocateSpace(_))
        .WillRepeatedly(Invoke([this](uint64_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);
            EXPECT_TRUE(space);

            EXPECT_CALL(*space, Accept()).Times(AtLeast(0));
            EXPECT_CALL(*space, Release()).Times(AtLeast(0));

            return {Move(space), ErrorEnum::eNone};
        }));

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

    std::ofstream blobFile(blobPath.CStr());

    if (blobFile.is_open()) {
        blobFile.close();
    } else {
        FAIL() << "Failed to create blob file at path: " << blobPath.CStr();
    }

    StaticString<cFilePathLen> path;

    auto err = mImageManager.GetBlobPath(cDigest, path);
    EXPECT_TRUE(err.IsNone()) << "Failed to get blob path: " << tests::utils::ErrorToStr(err);
    EXPECT_EQ(path, blobPath);
}

} // namespace aos::sm::imagemanager
