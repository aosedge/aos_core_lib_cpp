/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <core/common/tests/mocks/blobinfoprovidermock.hpp>
#include <core/common/tests/mocks/downloadermock.hpp>
#include <core/common/tests/mocks/fileinfoprovidermock.hpp>
#include <core/common/tests/mocks/fsmock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tests/stubs/failingallocator.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>

#include <core/sm/imagemanager/imagemanager.hpp>

#include "mocks/imagehandlermock.hpp"
#include "stubs/storagestub.hpp"

using namespace testing;

namespace aos::sm::imagemanager {
namespace {
/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestDir              = "/tmp/sm_imagemanager_test";
constexpr auto cTestImagePath        = "/tmp/sm_imagemanager_test/images";
constexpr auto cItemCacheTTL         = 10 * Time::cSeconds;
constexpr auto cRemoveOutdatedPeriod = 5 * Time::cSeconds;

constexpr auto cManifestDigest  = "sha256:1000000000000000000000000000000000000000000000000000000000000000";
constexpr auto cManifest2Digest = "sha256:1000000000000000000000000000000000000000000000000000000000000001";
constexpr auto cConfigDigest    = "sha256:2000000000000000000000000000000000000000000000000000000000000000";
constexpr auto cLayerDigest     = "sha256:3000000000000000000000000000000000000000000000000000000000000000";
constexpr auto cLayer2Digest    = "sha256:3000000000000000000000000000000000000000000000000000000000000001";
constexpr auto cDiffID          = "sha256:4000000000000000000000000000000000000000000000000000000000000000";
constexpr auto cOrphanDiffID    = "sha256:4000000000000000000000000000000000000000000000000000000000000001";
constexpr auto cEmptyDigest     = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
constexpr auto cLayerType       = "application/vnd.oci.image.layer.v1.tar+gzip";
constexpr auto cConfigType      = "application/vnd.oci.image.config.v1+json";
constexpr auto cBlobContent     = "blob";
constexpr auto cBlobSize        = 4;
constexpr auto cUnpackedData    = "layer data";
constexpr auto cUnpackedSize    = 1024;

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

std::string DigestPath(const std::string& folder, const std::string& digest)
{
    auto pos = digest.find(':');

    return std::string(cTestImagePath) + "/" + folder + "/" + digest.substr(0, pos) + "/" + digest.substr(pos + 1);
}

std::string BlobPath(const std::string& digest)
{
    return DigestPath("blobs", digest);
}

std::string LayerPath(const std::string& diffID)
{
    return DigestPath("layers", diffID);
}

std::string UnpackedPath(const std::string& diffID)
{
    return LayerPath(diffID) + "/layer";
}

bool Exists(const std::string& path)
{
    auto [exists, err] = fs::FileExist(path.c_str());
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    if (exists) {
        return true;
    }

    Tie(exists, err) = fs::DirExist(path.c_str());
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    return exists;
}

void WriteFile(const std::string& path, const std::string& content)
{
    auto err = fs::MakeDirAll(fs::Dir(path.c_str()));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    std::ofstream file(path);
    ASSERT_TRUE(file.is_open()) << path;

    file << content;
}

std::string ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

oci::ContentDescriptor Descriptor(const String& mediaType, const String& digest)
{
    return oci::ContentDescriptor {mediaType, digest, cBlobSize};
}

using aos::tests::FailingAllocator;

class TestStorageStub : public StorageStub {
public:
    RetWithError<bool> IsLayerUsed(const String& diffID) override
    {
        if (mOnIsLayerUsed) {
            mOnIsLayerUsed();
        }

        if (mFailIsLayerUsed) {
            return {false, Error(ErrorEnum::eFailed, "layer used check failed")};
        }

        return StorageStub::IsLayerUsed(diffID);
    }

    void SetFailIsLayerUsed(bool fail) { mFailIsLayerUsed = fail; }
    void SetOnIsLayerUsed(std::function<void()> callback) { mOnIsLayerUsed = std::move(callback); }

private:
    std::atomic<bool>     mFailIsLayerUsed {false};
    std::function<void()> mOnIsLayerUsed;
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class SMImageManagerTest : public Test {
protected:
    static void SetUpTestSuite()
    {
        tests::utils::InitLog();

        LOG_INF() << "Image manager size" << Log::Field("size", sizeof(ImageManager));
    }

    void SetUp() override
    {
        auto err = fs::RemoveAll(cTestDir);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        err = mImageManager.Init(CreateConfig(cTestImagePath), mBlobInfoProvider, mStorage, mSpaceAllocator,
            mDownloader, mImageHandler, mFileInfoProvider, mOCISpec, mFSPlatform, mAllocator);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        ON_CALL(mBlobInfoProvider, GetBlobInfo).WillByDefault(Invoke([](const String& digest, BlobInfo& info) -> Error {
            info.mDigest = digest;
            info.mSize   = cBlobSize;

            auto err = info.mURLs.EmplaceBack("https://cm/blob");
            if (!err.IsNone()) {
                return err;
            }

            return info.mURLs.EmplaceBack("https://cm/fallback");
        }));

        ON_CALL(mDownloader, Download)
            .WillByDefault(Invoke([this](CancelContext&, const String& digest, const Array<StaticString<cURLLen>>&,
                                      const String& path) -> Error {
                {
                    std::lock_guard lock {mMutex};

                    mDownloaded.insert(digest.CStr());
                }

                WriteFile(path.CStr(), cBlobContent);

                return ErrorEnum::eNone;
            }));

        ON_CALL(mFileInfoProvider, GetFileInfo)
            .WillByDefault(Invoke([this](const String& path, fs::FileInfo& info, crypto::Hash) -> Error {
                const std::string filePath = path.CStr();

                StaticString<oci::cDigestLen> hash;

                hash = filePath.substr(filePath.rfind('/') + 1).c_str();

                if (auto err = hash.HexToByteArray(info.mCheckSum); !err.IsNone()) {
                    return err;
                }

                std::lock_guard lock {mMutex};

                if (mBrokenBlobs.count(filePath) != 0) {
                    info.mCheckSum.Clear();
                }

                info.mSize = ReadFile(filePath).size();

                return ErrorEnum::eNone;
            }));

        ON_CALL(mOCISpec, LoadImageManifest)
            .WillByDefault(Invoke([this](const String& path, oci::ImageManifest& manifest) -> Error {
                manifest = *mManifests.at(path.CStr());

                return ErrorEnum::eNone;
            }));

        ON_CALL(mOCISpec, LoadImageConfig)
            .WillByDefault(Invoke([this](const String&, oci::ImageConfig& config) -> Error {
                config = *mImageConfig;

                return ErrorEnum::eNone;
            }));

        ON_CALL(mImageHandler, GetUnpackedLayerSize).WillByDefault(Return(RetWithError<size_t>(cUnpackedSize)));

        ON_CALL(mImageHandler, UnpackLayer)
            .WillByDefault(Invoke([](const String&, const String& dst, const String&) -> Error {
                WriteFile(std::string(dst.CStr()) + "/data", cUnpackedData);

                return ErrorEnum::eNone;
            }));

        ON_CALL(mImageHandler, GetUnpackedLayerDigest)
            .WillByDefault(Invoke([](const String& path) -> RetWithError<StaticString<oci::cDigestLen>> {
                StaticString<oci::cDigestLen> digest;

                digest = ReadFile(std::string(path.CStr()) + "/data").c_str();

                return digest;
            }));

        ON_CALL(mSpaceAllocator, AllocateSpace)
            .WillByDefault(Invoke([this](size_t size) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
                auto space = MakeUnique<NiceMock<spaceallocator::SpaceMock>>(&mAllocator);

                ON_CALL(*space, Accept()).WillByDefault(Invoke([this, size]() {
                    std::lock_guard lock {mMutex};

                    mAcceptedAllocations.push_back(size);

                    return size == mFailAcceptSize ? mAcceptError : Error(ErrorEnum::eNone);
                }));
                ON_CALL(*space, Release()).WillByDefault(Invoke([this]() {
                    mReleasedSpaces++;

                    return mReleaseError;
                }));
                ON_CALL(*space, Resize(_)).WillByDefault(Invoke([this](size_t resized) {
                    mLastResize = resized;

                    return resized > cBlobSize ? mResizeError : Error(ErrorEnum::eNone);
                }));

                return {Move(space), ErrorEnum::eNone};
            }));

        AddService(cManifestDigest, cLayerDigest);

        mImageConfig = std::make_unique<oci::ImageConfig>();
        mImageConfig->mRootfs.mDiffIDs.EmplaceBack(cDiffID);
    }

    void TearDown() override
    {
        auto err = fs::RemoveAll(cTestDir);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    static common::imagemanager::Config CreateConfig(const String& imagePath)
    {
        common::imagemanager::Config config;

        config.mImagePath            = imagePath;
        config.mItemCacheTTL         = cItemCacheTTL;
        config.mRemoveOutdatedPeriod = cRemoveOutdatedPeriod;

        return config;
    }

    void AddService(const std::string& manifestDigest, const std::string& layerDigest)
    {
        auto manifest = std::make_unique<oci::ImageManifest>();

        manifest->mConfig = Descriptor(cConfigType, cConfigDigest);
        manifest->mLayers.EmplaceBack(Descriptor(cLayerType, layerDigest.c_str()));

        mManifests[BlobPath(manifestDigest)] = std::move(manifest);
    }

    void AddComponent(const std::string& manifestDigest, const std::string& layerDigest)
    {
        auto manifest = std::make_unique<oci::ImageManifest>();

        manifest->mConfig = oci::ContentDescriptor {oci::cMediaTypeEmptyBlob, cEmptyDigest, 0};
        manifest->mLayers.EmplaceBack(Descriptor(cLayerType, layerDigest.c_str()));

        mManifests[BlobPath(manifestDigest)] = std::move(manifest);
    }

    static UpdateItemInfo ServiceInfo(const String& itemID, const String& digest = cManifestDigest)
    {
        return UpdateItemInfo {itemID, UpdateItemTypeEnum::eService, "1.0.0", digest};
    }

    std::optional<ItemState> GetItemState(const String& itemID)
    {
        auto statuses = std::make_unique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems * 2>>();

        auto err = mImageManager.GetItemsStatuses(*statuses);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        for (const auto& status : *statuses) {
            if (status.mItemID == itemID) {
                return status.mState;
            }
        }

        return std::nullopt;
    }

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    FailingAllocator mAllocator;

    ImageManager                                         mImageManager;
    NiceMock<common::imagemanager::BlobInfoProviderMock> mBlobInfoProvider;
    NiceMock<spaceallocator::SpaceAllocatorMock>         mSpaceAllocator;
    NiceMock<downloader::DownloaderMock>                 mDownloader;
    NiceMock<fs::FileInfoProviderMock>                   mFileInfoProvider;
    NiceMock<oci::OCISpecMock>                           mOCISpec;
    NiceMock<ImageHandlerMock>                           mImageHandler;
    NiceMock<FSPlatformMock>                             mFSPlatform;
    TestStorageStub                                      mStorage;

    std::mutex                                                 mMutex;
    std::set<std::string>                                      mDownloaded;
    std::set<std::string>                                      mBrokenBlobs;
    std::map<std::string, std::unique_ptr<oci::ImageManifest>> mManifests;
    std::unique_ptr<oci::ImageConfig>                          mImageConfig;
    std::vector<size_t>                                        mAcceptedAllocations;
    std::atomic<size_t>                                        mReleasedSpaces {};
    std::atomic<size_t>                                        mLastResize {};

    size_t mFailAcceptSize {};
    Error  mAcceptError;
    Error  mReleaseError;
    Error  mResizeError;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(SMImageManagerTest, InitFailsIfImagePathIsTooLongForLayerPath)
{
    // The path fits the blob path, but not the longest layer path.
    const auto maxBlobImagePathSize = cFilePathLen - (sizeof("/blobs/") - 1) - oci::cDigestLen;

    StaticString<cFilePathLen> imagePath;

    imagePath.Resize(maxBlobImagePathSize);

    for (auto& ch : imagePath) {
        ch = 'a';
    }

    ImageManager imageManager;

    auto err = imageManager.Init(CreateConfig(imagePath), mBlobInfoProvider, mStorage, mSpaceAllocator, mDownloader,
        mImageHandler, mFileInfoProvider, mOCISpec, mFSPlatform, mAllocator);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);
}

TEST_F(SMImageManagerTest, InstallServiceUnpacksLayerAndRemovesBlob)
{
    EXPECT_CALL(mDownloader,
        Download(_, _, Truly([](const Array<StaticString<cURLLen>>& urls) { return urls.Size() == 2; }), _))
        .Times(3);
    std::string unpackSrc;
    std::string unpackDst;
    std::string unpackType;

    EXPECT_CALL(mImageHandler, UnpackLayer)
        .WillOnce(Invoke([&](const String& src, const String& dst, const String& mediaType) -> Error {
            unpackSrc  = src.CStr();
            unpackDst  = dst.CStr();
            unpackType = mediaType.CStr();

            WriteFile(unpackDst + "/data", cUnpackedData);

            return ErrorEnum::eNone;
        }));

    // The layer blob is removed after the layer is unpacked and its space is freed.
    EXPECT_CALL(mSpaceAllocator, FreeSpace(strlen(cBlobContent)));

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(unpackSrc, BlobPath(cLayerDigest));
    EXPECT_EQ(unpackDst, UnpackedPath(cDiffID));
    EXPECT_EQ(unpackType, cLayerType);

    EXPECT_EQ(GetItemState("service"), ItemState(ItemStateEnum::eInstalled));
    EXPECT_FALSE(Exists(BlobPath(cLayerDigest)));
    EXPECT_TRUE(Exists(BlobPath(cConfigDigest)));
    EXPECT_EQ(ReadFile(LayerPath(cDiffID) + "/digest"), cUnpackedData);
    EXPECT_EQ(ReadFile(LayerPath(cDiffID) + "/size"), std::to_string(cUnpackedSize));

    // Metadata blobs and the layer blob are allocated with the blob size, the layer with its unpacked size.
    EXPECT_EQ(mAcceptedAllocations.back(), cUnpackedSize);

    // The space is resized to the real size of the layer files, as this is what the removal frees.
    EXPECT_EQ(mLastResize, strlen(cUnpackedData) * 2 + std::to_string(cUnpackedSize).size());

    StaticString<cFilePathLen> path;

    err = mImageManager.GetLayerPath(cDiffID, path);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(UnpackedPath(cDiffID), path.CStr());
}

TEST_F(SMImageManagerTest, InstallComponentKeepsLayerPacked)
{
    AddComponent(cManifest2Digest, cLayerDigest);

    EXPECT_CALL(mImageHandler, UnpackLayer).Times(0);

    auto err = mImageManager.InstallItem(
        UpdateItemInfo {"component", UpdateItemTypeEnum::eComponent, "1.0.0", cManifest2Digest});
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetItemState("component"), ItemState(ItemStateEnum::eInstalled));
    EXPECT_TRUE(Exists(BlobPath(cLayerDigest)));
    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
}

TEST_F(SMImageManagerTest, InstallAgainKeepsValidLayer)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The packed blob is not needed if the unpacked layer is valid.
    ASSERT_FALSE(Exists(BlobPath(cLayerDigest)));

    mDownloaded.clear();

    EXPECT_CALL(mImageHandler, UnpackLayer).Times(0);

    err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mDownloaded.empty());
}

TEST_F(SMImageManagerTest, InstallAgainRepairsBrokenLayer)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    WriteFile(UnpackedPath(cDiffID) + "/data", "broken");
    WriteFile(UnpackedPath(cDiffID) + "/garbage", "garbage");

    const auto oldSize
        = strlen("broken") + strlen("garbage") + strlen(cUnpackedData) + std::to_string(cUnpackedSize).size();

    mDownloaded.clear();

    // The layer is referenced by the item itself, so RemoveLayer keeps it and it is replaced by UnpackLayer. The
    // layer blob, removed by the first install, is downloaded again and removed after the layer is unpacked.
    {
        InSequence seq;

        EXPECT_CALL(mSpaceAllocator, FreeSpace(oldSize));
        EXPECT_CALL(mSpaceAllocator, FreeSpace(strlen(cBlobContent)));
    }

    EXPECT_CALL(mImageHandler, UnpackLayer);

    err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mDownloaded, std::set<std::string> {cLayerDigest});
    EXPECT_EQ(ReadFile(UnpackedPath(cDiffID) + "/data"), cUnpackedData);
    EXPECT_FALSE(Exists(UnpackedPath(cDiffID) + "/garbage"));
    EXPECT_FALSE(Exists(BlobPath(cLayerDigest)));
}

TEST_F(SMImageManagerTest, InstallAgainFinishesInterruptedUnpack)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_TRUE(fs::RemoveAll((LayerPath(cDiffID) + "/digest").c_str()).IsNone());

    EXPECT_CALL(mImageHandler, UnpackLayer);

    err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(Exists(LayerPath(cDiffID) + "/digest"));
}

TEST_F(SMImageManagerTest, UnpackFailureLeavesNoLayer)
{
    EXPECT_CALL(mImageHandler, UnpackLayer)
        .WillOnce(Invoke([](const String&, const String& dst, const String&) -> Error {
            WriteFile(std::string(dst.CStr()) + "/partial", "partial");

            return ErrorEnum::eFailed;
        }));

    // The layer blob is kept on a failed unpack.
    EXPECT_CALL(mSpaceAllocator, FreeSpace).Times(0);

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
    EXPECT_TRUE(Exists(BlobPath(cLayerDigest)));
    EXPECT_EQ(mReleasedSpaces, 1);
    EXPECT_EQ(GetItemState("service"), ItemState(ItemStateEnum::eFailed));
}

TEST_F(SMImageManagerTest, SpaceAllocationFailureLeavesNoLayer)
{
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(_)).WillRepeatedly(DoDefault());
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(static_cast<size_t>(cUnpackedSize)))
        .WillOnce(Return(RetWithError<UniquePtr<spaceallocator::SpaceItf>>(nullptr, ErrorEnum::eNoMemory)));

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
}

TEST_F(SMImageManagerTest, UnsupportedLayerMediaTypeFails)
{
    auto manifest = std::make_unique<oci::ImageManifest>();

    manifest->mConfig = Descriptor(cConfigType, cConfigDigest);
    manifest->mLayers.EmplaceBack(Descriptor("application/vnd.oci.image.layer.v1.tar+zstd", cLayerDigest));

    mManifests[BlobPath(cManifestDigest)] = std::move(manifest);

    EXPECT_CALL(mImageHandler, UnpackLayer).Times(0);

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
    EXPECT_EQ(GetItemState("service"), ItemState(ItemStateEnum::eFailed));
}

TEST_F(SMImageManagerTest, BrokenDownloadFails)
{
    {
        std::lock_guard lock {mMutex};

        mBrokenBlobs.insert(BlobPath(cManifestDigest));
    }

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetItemState("service"), ItemState(ItemStateEnum::eFailed));
}

TEST_F(SMImageManagerTest, DownloadErrorIsReturned)
{
    EXPECT_CALL(mDownloader, Download).WillOnce(Return(ErrorEnum::eIntegrity));

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eIntegrity)) << tests::utils::ErrorToStr(err);
}

TEST_F(SMImageManagerTest, CancelStopsDownload)
{
    std::promise<void> started;

    EXPECT_CALL(mDownloader, Download)
        .WillOnce(Invoke([&](CancelContext& ctx, const String&, const Array<StaticString<cURLLen>>&, const String&) {
            started.set_value();

            return ctx.Wait();
        }))
        .WillRepeatedly(DoDefault());

    auto install
        = std::async(std::launch::async, [this]() { return mImageManager.InstallItem(ServiceInfo("service")); });

    started.get_future().wait();

    auto err = mImageManager.Cancel();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = install.get();
    EXPECT_TRUE(err.Is(ErrorEnum::eCanceled)) << tests::utils::ErrorToStr(err);

    // The items installed after the cancel are not affected.
    err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(SMImageManagerTest, SameDiffIDIsUnpackedOnce)
{
    AddService(cManifest2Digest, cLayer2Digest);

    EXPECT_CALL(mImageHandler, UnpackLayer)
        .WillOnce(Invoke([](const String&, const String& dst, const String&) -> Error {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            WriteFile(std::string(dst.CStr()) + "/data", cUnpackedData);

            return ErrorEnum::eNone;
        }));

    auto install1
        = std::async(std::launch::async, [this]() { return mImageManager.InstallItem(ServiceInfo("service1")); });
    auto install2 = std::async(
        std::launch::async, [this]() { return mImageManager.InstallItem(ServiceInfo("service2", cManifest2Digest)); });

    auto err = install1.get();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = install2.get();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The blob of the layer unpacked or found valid is not kept.
    EXPECT_FALSE(Exists(BlobPath(cLayerDigest)));
    EXPECT_FALSE(Exists(BlobPath(cLayer2Digest)));
}

TEST_F(SMImageManagerTest, CancelStopsWaitForLayer)
{
    AddService(cManifest2Digest, cLayer2Digest);

    std::promise<void> unpackStarted;
    std::promise<void> unpackReleased;
    auto               unpackReleasedFuture = unpackReleased.get_future();

    EXPECT_CALL(mImageHandler, UnpackLayer)
        .WillOnce(Invoke([&](const String&, const String& dst, const String&) -> Error {
            unpackStarted.set_value();
            unpackReleasedFuture.wait();

            WriteFile(std::string(dst.CStr()) + "/data", cUnpackedData);

            return ErrorEnum::eNone;
        }));

    auto install1
        = std::async(std::launch::async, [this]() { return mImageManager.InstallItem(ServiceInfo("service1")); });

    unpackStarted.get_future().wait();

    // The second item waits for the layer with the same diff ID being unpacked by the first one.
    auto install2 = std::async(
        std::launch::async, [this]() { return mImageManager.InstallItem(ServiceInfo("service2", cManifest2Digest)); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto cancel = std::async(std::launch::async, [this]() { return mImageManager.Cancel(); });

    // The wait for the layer is canceled while the first item still unpacks it.
    auto status = install2.wait_for(std::chrono::seconds(5));

    unpackReleased.set_value();

    EXPECT_EQ(status, std::future_status::ready);

    auto err = install2.get();
    EXPECT_TRUE(err.Is(ErrorEnum::eCanceled)) << tests::utils::ErrorToStr(err);

    // The first item may finish the unpack or fail with eCanceled on the next blob.
    install1.wait();

    err = cancel.get();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(SMImageManagerTest, RemoveItemKeepsLayerInCache)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String("service"), String("1.0.0"), _));

    err = mImageManager.RemoveItem("service", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetItemState("service"), ItemState(ItemStateEnum::eRemoved));
    EXPECT_TRUE(Exists(UnpackedPath(cDiffID)));
}

TEST_F(SMImageManagerTest, ItemRemoverRemovesUnusedLayer)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.RemoveItem("service", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    const auto layerSize = strlen(cUnpackedData) * 2 + std::to_string(cUnpackedSize).size();

    size_t size = 0;

    // The space allocator frees the returned size, so nothing is freed by the image manager.
    EXPECT_CALL(mSpaceAllocator, FreeSpace).Times(0);

    Tie(size, err) = mImageManager.GetItemRemover().RemoveItem("service", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The layer blob is already removed after the unpack, so only the manifest and config blobs are freed.
    EXPECT_EQ(size, layerSize + 2 * cBlobSize);
    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
    EXPECT_FALSE(Exists(BlobPath(cLayerDigest)));
}

TEST_F(SMImageManagerTest, ItemRemoverKeepsLayerUsedByAnotherItem)
{
    AddService(cManifest2Digest, cLayer2Digest);

    auto err = mImageManager.InstallItem(ServiceInfo("service1"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.InstallItem(ServiceInfo("service2", cManifest2Digest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.RemoveItem("service1", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    size_t size = 0;

    Tie(size, err) = mImageManager.GetItemRemover().RemoveItem("service1", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(BlobPath(cLayerDigest)));
    EXPECT_TRUE(Exists(UnpackedPath(cDiffID)));

    err = mImageManager.RemoveItem("service2", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    Tie(size, err) = mImageManager.GetItemRemover().RemoveItem("service2", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
}

TEST_F(SMImageManagerTest, CleanupRemovesOrphanLayers)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    const auto orphanFile = UnpackedPath(cOrphanDiffID) + "/data";
    const auto strayFile  = std::string(cTestImagePath) + "/layers/stray";

    WriteFile(orphanFile, "orphan");
    WriteFile(strayFile, "stray");

    std::promise<void> freed;

    EXPECT_CALL(mSpaceAllocator, FreeSpace(strlen("orphan") + strlen("stray"))).WillOnce(InvokeWithoutArgs([&]() {
        freed.set_value();
    }));

    err = mImageManager.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(freed.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);

    err = mImageManager.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cOrphanDiffID)));
    EXPECT_FALSE(Exists(strayFile));
    EXPECT_TRUE(Exists(UnpackedPath(cDiffID)));
}

TEST_F(SMImageManagerTest, GetLayerPathErrors)
{
    StaticString<cFilePathLen> path;

    auto err = mImageManager.GetLayerPath("malformed", path);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    err = mImageManager.GetLayerPath(cDiffID, path);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(SMImageManagerTest, GetBlobPath)
{
    StaticString<cFilePathLen> path;

    auto err = mImageManager.GetBlobPath(cLayerDigest, path);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(BlobPath(cLayerDigest), path.CStr());
}

TEST_F(SMImageManagerTest, InitFailsIfImagePathIsFile)
{
    const auto filePath = std::string(cTestDir) + "/file";

    WriteFile(filePath, "content");

    ImageManager imageManager;

    auto err = imageManager.Init(CreateConfig((filePath + "/images").c_str()), mBlobInfoProvider, mStorage,
        mSpaceAllocator, mDownloader, mImageHandler, mFileInfoProvider, mOCISpec, mFSPlatform, mAllocator);
    EXPECT_FALSE(err.IsNone());
}

TEST_F(SMImageManagerTest, GetLayerPathSmallBuffer)
{
    StaticString<5> path;

    auto err = mImageManager.GetLayerPath(cDiffID, path);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);
}

TEST_F(SMImageManagerTest, GetLayerPathDirCheckFails)
{
    // The algorithm folder is a file, so the layer folder can't be checked.
    WriteFile(std::string(cTestImagePath) + "/layers/sha256", "content");

    StaticString<cFilePathLen> path;

    auto err = mImageManager.GetLayerPath(cDiffID, path);
    EXPECT_FALSE(err.IsNone());
    EXPECT_FALSE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(SMImageManagerTest, InstallMalformedDiffIDFails)
{
    mImageConfig->mRootfs.mDiffIDs.Clear();
    mImageConfig->mRootfs.mDiffIDs.EmplaceBack("malformed");

    EXPECT_CALL(mImageHandler, UnpackLayer).Times(0);

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetItemState("service"), ItemState(ItemStateEnum::eFailed));
}

TEST_F(SMImageManagerTest, InstallLayerFolderIsFileFails)
{
    WriteFile(std::string(cTestImagePath) + "/layers/sha256", "content");

    EXPECT_CALL(mImageHandler, UnpackLayer).Times(0);

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_FALSE(err.IsNone());

    EXPECT_EQ(GetItemState("service"), ItemState(ItemStateEnum::eFailed));
}

TEST_F(SMImageManagerTest, ItemRemoverComponentLayerWithoutDiffID)
{
    AddComponent(cManifest2Digest, cLayerDigest);

    auto err = mImageManager.InstallItem(
        UpdateItemInfo {"component", UpdateItemTypeEnum::eComponent, "1.0.0", cManifest2Digest});
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.RemoveItem("component", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    size_t size = 0;

    Tie(size, err) = mImageManager.GetItemRemover().RemoveItem("component", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(BlobPath(cLayerDigest)));
    EXPECT_GE(size, cBlobSize);
}

TEST_F(SMImageManagerTest, InstallAgainBrokenLayerDigestFile)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    WriteFile(LayerPath(cDiffID) + "/digest", std::string(10 * oci::cDigestLen, 'a'));

    EXPECT_CALL(mImageHandler, UnpackLayer);

    err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(LayerPath(cDiffID) + "/digest"), cUnpackedData);
}

TEST_F(SMImageManagerTest, InstallAgainLayerDigestCalculationFails)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mImageHandler, GetUnpackedLayerDigest)
        .WillOnce(Return(RetWithError<StaticString<oci::cDigestLen>>({}, ErrorEnum::eFailed)))
        .WillRepeatedly(DoDefault());

    // The layer is valid at the second check, so it is not unpacked again, only its blob is downloaded and removed.
    EXPECT_CALL(mImageHandler, UnpackLayer).Times(0);

    err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(BlobPath(cLayerDigest)));
}

TEST_F(SMImageManagerTest, UnpackedLayerSizeFails)
{
    EXPECT_CALL(mImageHandler, GetUnpackedLayerSize).WillOnce(Return(RetWithError<size_t>(0, ErrorEnum::eFailed)));
    EXPECT_CALL(mImageHandler, UnpackLayer).Times(0);

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
}

TEST_F(SMImageManagerTest, AcceptUnpackedLayerSpaceFails)
{
    mFailAcceptSize = cUnpackedSize;
    mAcceptError    = ErrorEnum::eFailed;

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetItemState("service"), ItemState(ItemStateEnum::eFailed));
}

TEST_F(SMImageManagerTest, ReleaseSpaceFailureIsIgnored)
{
    mReleaseError = ErrorEnum::eFailed;

    EXPECT_CALL(mImageHandler, UnpackLayer).WillOnce(Return(ErrorEnum::eTimeout));

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eTimeout)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
    EXPECT_EQ(mReleasedSpaces, 1);
}

TEST_F(SMImageManagerTest, LayerMetadataCreationFails)
{
    EXPECT_CALL(mImageHandler, GetUnpackedLayerDigest)
        .WillOnce(Return(RetWithError<StaticString<oci::cDigestLen>>({}, ErrorEnum::eFailed)));

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
}

TEST_F(SMImageManagerTest, ResizeUnpackedLayerSpaceFails)
{
    mResizeError = ErrorEnum::eNoMemory;

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
}

TEST_F(SMImageManagerTest, ItemRemoverLayerUsageCheckFails)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.RemoveItem("service", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    mStorage.SetFailIsLayerUsed(true);

    size_t size = 0;

    Tie(size, err) = mImageManager.GetItemRemover().RemoveItem("service", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(Exists(UnpackedPath(cDiffID)));
}

TEST_F(SMImageManagerTest, CleanupOrphanLayerUsageCheckFails)
{
    const auto orphanFile = UnpackedPath(cOrphanDiffID) + "/data";
    const auto strayFile  = std::string(cTestImagePath) + "/layers/stray";

    WriteFile(orphanFile, "orphan");
    WriteFile(strayFile, "stray");

    mStorage.SetFailIsLayerUsed(true);

    std::promise<void> freed;

    // Only the stray file is removed, as the usage of the orphan layer can't be checked.
    EXPECT_CALL(mSpaceAllocator, FreeSpace(strlen("stray"))).WillOnce(InvokeWithoutArgs([&]() { freed.set_value(); }));

    auto err = mImageManager.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(freed.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);

    err = mImageManager.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(Exists(orphanFile));
    EXPECT_FALSE(Exists(strayFile));
}

TEST_F(SMImageManagerTest, InstallAgainMissingLayerMetadata)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mImageHandler, UnpackLayer).Times(2).WillRepeatedly(DoDefault());

    for (const auto* name : {"size", "digest"}) {
        const auto path = LayerPath(cDiffID) + "/" + name;

        ASSERT_TRUE(fs::RemoveAll(path.c_str()).IsNone());

        err = mImageManager.InstallItem(ServiceInfo("service"));
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        EXPECT_TRUE(Exists(path)) << path;
    }
}

TEST_F(SMImageManagerTest, InstallAgainMissingUnpackedLayer)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_TRUE(fs::RemoveAll(UnpackedPath(cDiffID).c_str()).IsNone());

    EXPECT_CALL(mImageHandler, UnpackLayer);

    err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(Exists(UnpackedPath(cDiffID)));
}

TEST_F(SMImageManagerTest, UnpackFailsAndLayerCantBeRemoved)
{
    const auto dir = std::string(cTestImagePath) + "/layers/sha256";

    EXPECT_CALL(mImageHandler, UnpackLayer).WillOnce(Invoke([&](const String&, const String&, const String&) -> Error {
        EXPECT_TRUE(fs::RemoveAll(dir.c_str()).IsNone());
        WriteFile(dir, "content");

        return ErrorEnum::eFailed;
    }));

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    // The space stays accepted as the layer stays.
    EXPECT_EQ(mReleasedSpaces, 0);
    EXPECT_NE(
        std::find(mAcceptedAllocations.begin(), mAcceptedAllocations.end(), cUnpackedSize), mAcceptedAllocations.end());
}

TEST_F(SMImageManagerTest, LayerSizeCalculationFails)
{
    EXPECT_CALL(mImageHandler, GetUnpackedLayerDigest)
        .WillOnce(Invoke([this](const String& path) -> RetWithError<StaticString<oci::cDigestLen>> {
            mAllocator.FailAll();

            StaticString<oci::cDigestLen> digest;

            digest = ReadFile(std::string(path.CStr()) + "/data").c_str();

            return digest;
        }));

    auto err = mImageManager.InstallItem(ServiceInfo("service"));

    mAllocator.Reset();

    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);
    EXPECT_FALSE(Exists(LayerPath(cDiffID)));
}

TEST_F(SMImageManagerTest, LayerMetadataWriteFails)
{
    for (const auto* name : {"size", "digest"}) {
        // The metadata file is a folder, so it can't be written.
        EXPECT_CALL(mImageHandler, GetUnpackedLayerDigest)
            .WillOnce(Invoke([name](const String& path) -> RetWithError<StaticString<oci::cDigestLen>> {
                const auto layerPath = std::string(path.CStr()).substr(0, std::string(path.CStr()).rfind('/'));

                EXPECT_TRUE(fs::MakeDirAll((layerPath + "/" + name).c_str()).IsNone());

                StaticString<oci::cDigestLen> digest;

                digest = ReadFile(std::string(path.CStr()) + "/data").c_str();

                return digest;
            }))
            .RetiresOnSaturation();

        auto err = mImageManager.InstallItem(ServiceInfo("service"));
        EXPECT_FALSE(err.IsNone()) << name;

        EXPECT_FALSE(Exists(LayerPath(cDiffID))) << name;
    }
}

TEST_F(SMImageManagerTest, LayerBlobCantBeRemoved)
{
    const auto dir = std::string(cTestImagePath) + "/blobs/sha256";

    EXPECT_CALL(mImageHandler, UnpackLayer)
        .WillOnce(Invoke([&](const String&, const String& dst, const String&) -> Error {
            WriteFile(std::string(dst.CStr()) + "/data", cUnpackedData);

            EXPECT_TRUE(fs::RemoveAll(dir.c_str()).IsNone());
            WriteFile(dir, "content");

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mSpaceAllocator, FreeSpace).Times(0);

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(Exists(UnpackedPath(cDiffID)));
}

TEST_F(SMImageManagerTest, LayerBlobAlreadyRemoved)
{
    EXPECT_CALL(mImageHandler, UnpackLayer)
        .WillOnce(Invoke([&](const String& src, const String& dst, const String&) -> Error {
            WriteFile(std::string(dst.CStr()) + "/data", cUnpackedData);

            EXPECT_TRUE(fs::RemoveAll(src).IsNone());

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mSpaceAllocator, FreeSpace).Times(0);

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(SMImageManagerTest, ItemRemoverMalformedLayerDiffID)
{
    common::imagemanager::DeployableItemInfo item;

    item.mItemID     = "service";
    item.mType       = UpdateItemTypeEnum::eService;
    item.mVersion    = "1.0.0";
    item.mRootDigest = cManifestDigest;
    item.mState      = ItemStateEnum::eRemoved;
    item.mTimestamp  = Time::Now();

    ASSERT_TRUE(mStorage.AddDeployableItem(item).IsNone());

    common::imagemanager::ItemBlobInfo blob;

    blob.mDigest    = cLayerDigest;
    blob.mMediaType = cLayerType;
    blob.mDiffID    = "malformed";
    blob.mIsLayer   = true;

    ASSERT_TRUE(mStorage.AddItemBlob("service", "1.0.0", blob).IsNone());

    auto [size, err] = mImageManager.GetItemRemover().RemoveItem("service", "1.0.0");
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(size, 0);
}

TEST_F(SMImageManagerTest, ItemRemoverLayerRemoveFails)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.RemoveItem("service", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    const auto dir = std::string(cTestImagePath) + "/layers/sha256";

    mStorage.SetOnIsLayerUsed([&]() {
        EXPECT_TRUE(fs::RemoveAll(dir.c_str()).IsNone());
        WriteFile(dir, "content");
    });

    size_t size = 0;

    Tie(size, err) = mImageManager.GetItemRemover().RemoveItem("service", "1.0.0");
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(size, 2 * cBlobSize);
}

TEST_F(SMImageManagerTest, ItemRemoverLayerSizeCalculationFails)
{
    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.RemoveItem("service", "1.0.0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    mStorage.SetOnIsLayerUsed([&]() { mAllocator.FailAll(); });

    size_t size = 0;

    Tie(size, err) = mImageManager.GetItemRemover().RemoveItem("service", "1.0.0");

    mAllocator.Reset();

    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(size, 2 * cBlobSize);
    EXPECT_TRUE(Exists(UnpackedPath(cDiffID)));
}

TEST_F(SMImageManagerTest, CleanupSkipsLockedLayer)
{
    const auto strayFile = std::string(cTestImagePath) + "/layers/stray";

    WriteFile(strayFile, "stray");

    std::promise<void> freed;

    EXPECT_CALL(mSpaceAllocator, FreeSpace(strlen("stray"))).WillOnce(InvokeWithoutArgs([&]() { freed.set_value(); }));
    EXPECT_CALL(mSpaceAllocator, FreeSpace(strlen(cBlobContent)));

    // The cleanup is started when the layer is being unpacked, so the layer is locked.
    EXPECT_CALL(mImageHandler, UnpackLayer)
        .WillOnce(Invoke([&](const String&, const String& dst, const String&) -> Error {
            WriteFile(std::string(dst.CStr()) + "/data", cUnpackedData);

            EXPECT_TRUE(mImageManager.Start().IsNone());
            EXPECT_EQ(freed.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);

            return ErrorEnum::eNone;
        }));

    auto err = mImageManager.InstallItem(ServiceInfo("service"));
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.Stop();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(Exists(strayFile));
    EXPECT_TRUE(Exists(UnpackedPath(cDiffID)));
}

} // namespace aos::sm::imagemanager
