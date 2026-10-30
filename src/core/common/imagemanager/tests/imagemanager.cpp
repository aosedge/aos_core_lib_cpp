/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

constexpr auto cTestDir       = "/tmp/common_imagemanager_test";
constexpr auto cTestImagePath = "/tmp/common_imagemanager_test/images";
constexpr auto cTestHash      = "a3b2c1d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90";
constexpr auto cTestDigest    = "sha256:a3b2c1d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90";
constexpr auto cOtherHash     = "0000000000000000000000000000000000000000000000000000000000000000";
constexpr auto cOtherDigest   = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
constexpr auto cTestBlobPath  = "/tmp/common_imagemanager_test/images/blobs/sha256/"
                                "a3b2c1d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90";
constexpr auto cTestLayerType = "application/vnd.oci.image.layer.v1.tar+gzip";

constexpr char   cTestBlobContent[] = "test blob content";
constexpr size_t cTestBlobSize      = sizeof(cTestBlobContent) - 1;

constexpr auto cTestItemID      = "item1";
constexpr auto cTestVersion     = "1.0.0";
constexpr auto cImageConfigType = "application/vnd.oci.image.config.v1+json";
constexpr auto cItemConfigType  = "application/vnd.aos.item.config.v1+json";
constexpr auto cInstallBlobSize = 64;

// Same as the image manager private constants.
constexpr auto cMaxNumStoredItems = cMaxNumUpdateItems * 2;
constexpr auto cMaxNumItemBlobs   = 1 + oci::cMaxNumManifests * (3 + oci::cMaxNumLayers);

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

void CreateFile(const String& path, const String& content)
{
    auto err = fs::MakeDirAll(fs::Dir(path));
    ASSERT_TRUE(err.IsNone()) << "Failed to create dir: " << tests::utils::ErrorToStr(err);

    err = fs::WriteStringToFile(path, content, 0600);
    ASSERT_TRUE(err.IsNone()) << "Failed to create file: " << tests::utils::ErrorToStr(err);
}

bool FileExists(const String& path)
{
    auto [exists, err] = fs::FileExist(path);
    EXPECT_TRUE(err.IsNone()) << "Failed to check file: " << tests::utils::ErrorToStr(err);

    return exists;
}

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

/**
 * Test blob: the blob content is its hash, so the checksum is calculated from the content.
 */
struct TestBlob {
    std::string mHash;
    std::string mDigest;
    std::string mPath;
};

TestBlob MakeTestBlob(uint32_t id)
{
    char hash[65];

    std::snprintf(hash, sizeof(hash), "%064x", id);

    TestBlob blob;

    blob.mHash   = hash;
    blob.mDigest = std::string("sha256:") + hash;
    blob.mPath   = std::string(cTestImagePath) + "/blobs/sha256/" + hash;

    return blob;
}

oci::ContentDescriptor MakeDescriptor(const TestBlob& blob, const char* mediaType)
{
    return oci::ContentDescriptor {mediaType, blob.mDigest.c_str(), cInstallBlobSize};
}

std::string ReadFile(const std::string& path)
{
    std::ifstream     file(path);
    std::stringstream content;

    content << file.rdbuf();

    return content.str();
}

Error GetFileInfoFromContent(const String& path, fs::FileInfo& info, const crypto::Hash&)
{
    auto content = ReadFile(path.CStr());

    info.mCheckSum.Clear();
    info.mSize = content.size();

    // Content which is not a hash gives an empty checksum, so the validation fails.
    if (content.size() == crypto::cSHA256Size * 2) {
        StaticString<crypto::cSHA256Size * 2> hash {content.c_str()};

        if (!hash.HexToByteArray(info.mCheckSum).IsNone()) {
            info.mCheckSum.Clear();
        }
    }

    return ErrorEnum::eNone;
}

Error GetTestBlobInfo(const String& digest, BlobInfo& blobInfo)
{
    blobInfo.mDigest = digest;
    blobInfo.mSize   = cInstallBlobSize;

    return ErrorEnum::eNone;
}

Error DownloadTestBlob(const String& path, const BlobInfo& blobInfo)
{
    // Write the blob hash as the blob content.
    return fs::WriteStringToFile(path, String(blobInfo.mDigest.CStr() + std::strlen("sha256:")), 0600);
}

/***********************************************************************************************************************
 * Test image manager
 **********************************************************************************************************************/

class TestImageManager : public ImageManager {
public:
    using ImageManager::Init;
    using ImageManager::InstallItemInfo;

    struct LayerCall {
        std::string mDigest;
        std::string mDiffDigest;

        bool operator==(const LayerCall& rhs) const { return mDigest == rhs.mDigest && mDiffDigest == rhs.mDiffDigest; }
    };

    Error Install(const InstallItemInfo& itemInfo) { return InstallItem(itemInfo); }
    Error Remove(const String& itemID, const String& version) { return RemoveItem(itemID, version); }

    Error ValidateLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffID) override
    {
        {
            std::lock_guard lock {mLayerCallsMutex};

            mValidateLayerCalls.push_back({descriptor.mDigest.CStr(), diffID.CStr()});
        }

        return CheckLayer(path, descriptor, diffID);
    }

    std::vector<LayerCall> GetValidateLayerCalls()
    {
        std::lock_guard lock {mLayerCallsMutex};

        return mValidateLayerCalls;
    }

    MOCK_METHOD(Error, DownloadBlob, (const String& path, const BlobInfo& blobInfo), (override));
    MOCK_METHOD(Error, CancelDownload, (const String& digest), (override));
    MOCK_METHOD(
        Error, CheckLayer, (const String& path, const oci::ContentDescriptor& descriptor, const String& diffID));
    MOCK_METHOD(Error, UnpackLayer,
        (const String& path, const oci::ContentDescriptor& descriptor, const String& diffID), (override));
    MOCK_METHOD((RetWithError<size_t>), RemoveLayer,
        (const String& path, const oci::ContentDescriptor& descriptor, const String& diffID), (override));

private:
    std::mutex             mLayerCallsMutex;
    std::vector<LayerCall> mValidateLayerCalls;
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CommonImageManagerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        auto err = fs::RemoveAll(cTestDir);
        ASSERT_TRUE(err.IsNone()) << "Failed to remove test dir: " << tests::utils::ErrorToStr(err);

        // By default, the layers are kept as packed blobs only.
        ON_CALL(mImageManager, CheckLayer(_, _, _)).WillByDefault(Return(ErrorEnum::eNotSupported));
        ON_CALL(mImageManager, UnpackLayer(_, _, _)).WillByDefault(Return(ErrorEnum::eNotSupported));
        ON_CALL(mImageManager, RemoveLayer(_, _, _))
            .WillByDefault(Return(RetWithError<size_t>(0, ErrorEnum::eNotSupported)));
    }

    void TearDown() override
    {
        auto err = fs::RemoveAll(cTestDir);
        EXPECT_TRUE(err.IsNone()) << "Failed to remove test dir: " << tests::utils::ErrorToStr(err);
    }

    Error InitImageManager(const String& imagePath = cTestImagePath)
    {
        Config config {imagePath, 0, 10 * Time::cSeconds, 5 * Time::cSeconds};

        return mImageManager.Init(
            config, mBlobInfoProvider, mStorage, mSpaceAllocator, mFileInfoProvider, mOCISpec, mFSPlatform, mAllocator);
    }

    void AddStoredItem(
        const String& itemID, const String& version, ItemStateEnum state, const Error& error = ErrorEnum::eNone)
    {
        auto err = mStorage.AddDeployableItem(CreateDeployableItem(itemID, version, state, error));
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    // mAllocator must be declared before any member that allocates from it, so it is destroyed after them, since
    // members are destroyed in reverse declaration order.
    HeapAllocator                                mAllocator;
    NiceMock<BlobInfoProviderMock>               mBlobInfoProvider;
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

    auto [exists, existErr] = fs::DirExist(fs::JoinPath(cTestImagePath, "blobs"));
    ASSERT_TRUE(existErr.IsNone()) << tests::utils::ErrorToStr(existErr);
    EXPECT_TRUE(exists);
}

TEST_F(CommonImageManagerTest, InitFailsIfImageDirCantBeCreated)
{
    const auto filePath = fs::JoinPath(cTestDir, "file");

    CreateFile(filePath, "content");

    auto err = InitImageManager(fs::JoinPath(filePath, "images"));
    EXPECT_FALSE(err.IsNone());
}

TEST_F(CommonImageManagerTest, GetBlobPath)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    StaticString<cFilePathLen> path;

    auto err = mImageManager.GetBlobPath(cTestDigest, path);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(path, cTestBlobPath);

    EXPECT_FALSE(FileExists(path));
}

TEST_F(CommonImageManagerTest, GetLayerPathNotSupported)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    StaticString<cFilePathLen> path;

    auto err = mImageManager.GetLayerPath(cTestDigest, path);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotSupported)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, GetBlobPathMalformedDigest)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    for (const auto& digest : {"", "sha256", "sha256:", ":abc", "sha256:ab:cd", "sha256:../abc", "../x:abc",
             "sha256:ab/cd", "SHA256:abc", ".:abc", "sha256:ab-cd", "sha256:ab_cd", "sha256:abc="}) {
        StaticString<cFilePathLen> path;

        auto err = mImageManager.GetBlobPath(digest, path);
        EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << digest << ": " << tests::utils::ErrorToStr(err);
    }
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

TEST_F(CommonImageManagerTest, StartRemovesInterruptedItemsAndAddsOutdatedItems)
{
    ASSERT_TRUE(InitImageManager().IsNone());
    ASSERT_TRUE(mImageManager.SubscribeListener(mListener).IsNone());

    AddStoredItem("item1", "1.0.0", ItemStateEnum::eInstalled);
    AddStoredItem("item1", "2.0.0", ItemStateEnum::eInstalling);
    AddStoredItem("item2", "1.0.0", ItemStateEnum::eRemoved);

    ASSERT_TRUE(
        mStorage.AddItemBlob("item1", "2.0.0", ItemBlobInfo {{cTestLayerType, cTestDigest, 1}, true, ""}).IsNone());
    AddStoredItem("item3", "1.0.0", ItemStateEnum::eFailed, Error(ErrorEnum::eFailed, "download failed"));
    AddStoredItem("item4", "1.0.0", ItemStateEnum::eRemoved);

    std::map<std::string, DeployableItemInfo> storedItems;

    for (const auto& [itemID, version] : std::vector<std::pair<const char*, const char*>>(
             {{"item1", "1.0.0"}, {"item2", "1.0.0"}, {"item3", "1.0.0"}, {"item4", "1.0.0"}})) {
        DeployableItemInfo item;

        ASSERT_TRUE(mStorage.GetDeployableItem(itemID, version, item).IsNone());

        storedItems[std::string(itemID) + ":" + version] = item;
    }

    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).Times(0);
    EXPECT_CALL(
        mSpaceAllocator, AddOutdatedItem(String("item2"), String("1.0.0"), storedItems["item2:1.0.0"].mTimestamp))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(
        mSpaceAllocator, AddOutdatedItem(String("item4"), String("1.0.0"), storedItems["item4:1.0.0"].mTimestamp))
        .WillOnce(Return(ErrorEnum::eNone));

    Time interruptedTimestamp;

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String("item1"), String("2.0.0"), _))
        .WillOnce(Invoke([&](const String&, const String&, const Time& timestamp) {
            interruptedTimestamp = timestamp;

            return ErrorEnum::eNone;
        }));

    auto err = mImageManager.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    DeployableItemInfo item;

    ASSERT_TRUE(mStorage.GetDeployableItem("item1", "2.0.0", item).IsNone());
    EXPECT_EQ(item.mState, ItemStateEnum::eRemoved);
    EXPECT_TRUE(item.mError.IsNone()) << tests::utils::ErrorToStr(item.mError);
    EXPECT_EQ(item.mTimestamp, interruptedTimestamp);

    // The blob references of the interrupted item are kept.
    auto blobs = std::make_unique<StaticArray<ItemBlobInfo, cMaxNumItemBlobs>>();

    ASSERT_TRUE(mStorage.GetItemBlobs("item1", "2.0.0", *blobs).IsNone());
    EXPECT_EQ(blobs->Size(), 1);

    // The other items are not changed.
    for (const auto& [key, storedItem] : storedItems) {
        ASSERT_TRUE(mStorage.GetDeployableItem(storedItem.mItemID, storedItem.mVersion, item).IsNone());
        EXPECT_EQ(item, storedItem) << key;
    }
}

TEST_F(CommonImageManagerTest, StartAddOutdatedItemFails)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    AddStoredItem("item1", "1.0.0", ItemStateEnum::eRemoved);
    AddStoredItem("item2", "1.0.0", ItemStateEnum::eRemoved);

    // The failed item is only logged and the next items are still added.
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String("item1"), String("1.0.0"), _))
        .WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String("item2"), String("1.0.0"), _))
        .WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, StartUpdateItemFails)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    AddStoredItem("item1", "1.0.0", ItemStateEnum::eInstalling);

    mStorage.SetFailUpdateStates({ItemStateEnum::eRemoved});

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).Times(0);

    auto err = mImageManager.Start();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    DeployableItemInfo item;

    ASSERT_TRUE(mStorage.GetDeployableItem("item1", "1.0.0", item).IsNone());
    EXPECT_EQ(item.mState, ItemStateEnum::eInstalling);
}

TEST_F(CommonImageManagerTest, GetItemsStatusesEmpty)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    StaticArray<UpdateItemStatus, cMaxNumStoredItems> statuses;

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

    StaticArray<UpdateItemStatus, cMaxNumStoredItems + 1> statuses;

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

/***********************************************************************************************************************
 * Install item suite
 **********************************************************************************************************************/

class CommonImageManagerInstallTest : public CommonImageManagerTest {
protected:
    struct TestManifest {
        TestBlob              mManifest;
        TestBlob              mImageConfig;
        TestBlob              mItemConfig;
        std::vector<TestBlob> mLayers;
        std::vector<TestBlob> mDiffIDs;
    };

    void SetUp() override
    {
        CommonImageManagerTest::SetUp();

        ON_CALL(mFileInfoProvider, GetFileInfo(_, _, _)).WillByDefault(Invoke(GetFileInfoFromContent));
        ON_CALL(mBlobInfoProvider, GetBlobInfo(_, _)).WillByDefault(Invoke(GetTestBlobInfo));
        ON_CALL(mImageManager, DownloadBlob(_, _)).WillByDefault(Invoke(DownloadTestBlob));

        ON_CALL(mSpaceAllocator, AllocateSpace(_))
            .WillByDefault(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
                auto space = MakeUnique<NiceMock<spaceallocator::SpaceMock>>(&mAllocator);

                ON_CALL(*space, Accept()).WillByDefault(Invoke([this]() {
                    mAcceptCount++;

                    return ErrorEnum::eNone;
                }));
                ON_CALL(*space, Release()).WillByDefault(Invoke([this]() {
                    mReleaseCount++;

                    return ErrorEnum::eNone;
                }));

                return {Move(space), ErrorEnum::eNone};
            }));

        ON_CALL(mOCISpec, LoadImageIndex(_, _))
            .WillByDefault(Invoke([this](const String& path, oci::ImageIndex& index) -> Error {
                std::lock_guard lock {mOCIMutex};

                auto it = mIndexes.find(path.CStr());
                if (it == mIndexes.end()) {
                    return ErrorEnum::eNotFound;
                }

                index = *it->second;

                return ErrorEnum::eNone;
            }));
        ON_CALL(mOCISpec, LoadImageManifest(_, _))
            .WillByDefault(Invoke([this](const String& path, oci::ImageManifest& manifest) -> Error {
                std::lock_guard lock {mOCIMutex};

                auto it = mManifests.find(path.CStr());
                if (it == mManifests.end()) {
                    return ErrorEnum::eNotFound;
                }

                manifest = *it->second;

                return ErrorEnum::eNone;
            }));
        ON_CALL(mOCISpec, LoadImageConfig(_, _))
            .WillByDefault(Invoke([this](const String& path, oci::ImageConfig& config) -> Error {
                std::lock_guard lock {mOCIMutex};

                auto it = mImageConfigs.find(path.CStr());
                if (it == mImageConfigs.end()) {
                    return ErrorEnum::eNotFound;
                }

                config = *it->second;

                return ErrorEnum::eNone;
            }));

        ON_CALL(mListener, OnItemStatusChanged(_)).WillByDefault(Invoke([this](const UpdateItemStatus& status) {
            std::lock_guard lock {mStatusesMutex};

            mStatuses.push_back(status);
        }));

        ASSERT_TRUE(InitImageManager().IsNone());
        ASSERT_TRUE(mImageManager.SubscribeListener(mListener).IsNone());
    }

    // Registers manifest with image config, optional item config and layers. Blob IDs start from base.
    TestManifest AddManifest(uint32_t base, size_t numLayers, size_t numDiffIDs, bool withItemConfig = false,
        const std::vector<TestBlob>& sharedLayers = {})
    {
        TestManifest testManifest;

        testManifest.mManifest    = MakeTestBlob(base);
        testManifest.mImageConfig = MakeTestBlob(base + 1);
        testManifest.mItemConfig  = MakeTestBlob(base + 2);

        auto manifest = std::make_shared<oci::ImageManifest>();

        manifest->mMediaType = oci::cMediaTypeImageManifest;
        manifest->mConfig    = MakeDescriptor(testManifest.mImageConfig, cImageConfigType);

        if (withItemConfig) {
            manifest->mItemConfig.SetValue(MakeDescriptor(testManifest.mItemConfig, cItemConfigType));
        }

        testManifest.mLayers = sharedLayers;

        for (size_t i = 0; i < numLayers; ++i) {
            testManifest.mLayers.push_back(MakeTestBlob(base + 10 + i));
        }

        for (const auto& layer : testManifest.mLayers) {
            EXPECT_TRUE(manifest->mLayers.PushBack(MakeDescriptor(layer, cTestLayerType)).IsNone());
        }

        auto imageConfig = std::make_shared<oci::ImageConfig>();

        for (size_t i = 0; i < numDiffIDs; ++i) {
            testManifest.mDiffIDs.push_back(MakeTestBlob(base + 100 + i));

            EXPECT_TRUE(
                imageConfig->mRootfs.mDiffIDs.EmplaceBack(testManifest.mDiffIDs.back().mDigest.c_str()).IsNone());
        }

        std::lock_guard lock {mOCIMutex};

        mManifests[testManifest.mManifest.mPath]       = manifest;
        mImageConfigs[testManifest.mImageConfig.mPath] = imageConfig;

        return testManifest;
    }

    TestBlob AddIndex(uint32_t id, const std::vector<TestManifest>& manifests)
    {
        auto blob  = MakeTestBlob(id);
        auto index = std::make_shared<oci::ImageIndex>();

        index->mMediaType = oci::cMediaTypeImageIndex;

        for (const auto& manifest : manifests) {
            oci::IndexContentDescriptor descriptor;

            static_cast<oci::ContentDescriptor&>(descriptor)
                = MakeDescriptor(manifest.mManifest, oci::cMediaTypeImageManifest);

            EXPECT_TRUE(index->mManifests.PushBack(descriptor).IsNone());
        }

        std::lock_guard lock {mOCIMutex};

        mIndexes[blob.mPath] = index;

        return blob;
    }

    void CreateValidBlobs(const TestManifest& manifest)
    {
        for (const auto* blob : {&manifest.mManifest, &manifest.mImageConfig, &manifest.mItemConfig}) {
            CreateFile(blob->mPath.c_str(), blob->mHash.c_str());
        }

        for (const auto& layer : manifest.mLayers) {
            CreateFile(layer.mPath.c_str(), layer.mHash.c_str());
        }
    }

    static TestImageManager::InstallItemInfo MakeInstallInfo(const TestBlob& root, bool isRootIndex = false,
        UpdateItemTypeEnum type = UpdateItemTypeEnum::eService, const char* itemID = cTestItemID,
        const char* version = cTestVersion)
    {
        TestImageManager::InstallItemInfo itemInfo;

        itemInfo.mItemID      = itemID;
        itemInfo.mType        = type;
        itemInfo.mVersion     = version;
        itemInfo.mRootDigest  = root.mDigest.c_str();
        itemInfo.mIsRootIndex = isRootIndex;

        return itemInfo;
    }

    void StoreItem(const String& itemID, const String& version, const String& rootDigest, ItemStateEnum state)
    {
        auto item = CreateDeployableItem(itemID, version, state);

        item.mRootDigest = rootDigest;

        auto err = mStorage.AddDeployableItem(item);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    std::optional<DeployableItemInfo> GetStoredItem(const String& itemID, const String& version)
    {
        DeployableItemInfo item;

        if (!mStorage.GetDeployableItem(itemID, version, item).IsNone()) {
            return std::nullopt;
        }

        return item;
    }

    std::vector<UpdateItemStatus> GetNotifiedStatuses()
    {
        std::lock_guard lock {mStatusesMutex};

        return mStatuses;
    }

    std::vector<ItemStateEnum> GetNotifiedStates()
    {
        std::vector<ItemStateEnum> states;

        for (const auto& status : GetNotifiedStatuses()) {
            states.push_back(status.mState.GetValue());
        }

        return states;
    }

    std::optional<ItemStateEnum> GetReportedState(const String& itemID, const String& version)
    {
        auto statuses = std::make_unique<StaticArray<UpdateItemStatus, cMaxNumStoredItems>>();

        auto err = mImageManager.GetItemsStatuses(*statuses);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        const auto* status = FindStatus(*statuses, itemID, version);
        if (status == nullptr) {
            return std::nullopt;
        }

        return status->mState.GetValue();
    }

    static ItemBlobInfo MakeBlobInfo(const TestBlob& blob, const char* mediaType, bool isLayer = false,
        const std::string& diffID = "", size_t size = cInstallBlobSize)
    {
        ItemBlobInfo info;

        info.mDigest    = blob.mDigest.c_str();
        info.mSize      = size;
        info.mMediaType = mediaType;
        info.mDiffID    = diffID.c_str();
        info.mIsLayer   = isLayer;

        return info;
    }

    std::atomic<int> mAcceptCount {0};
    std::atomic<int> mReleaseCount {0};

    std::mutex                                                 mOCIMutex;
    std::map<std::string, std::shared_ptr<oci::ImageIndex>>    mIndexes;
    std::map<std::string, std::shared_ptr<oci::ImageManifest>> mManifests;
    std::map<std::string, std::shared_ptr<oci::ImageConfig>>   mImageConfigs;

    std::mutex                    mStatusesMutex;
    std::vector<UpdateItemStatus> mStatuses;
};

TEST_F(CommonImageManagerInstallTest, InstallNotifiesSubscribedListeners)
{
    const auto manifest = AddManifest(1, 1, 1);

    NiceMock<ItemStatusListenerMock> otherListener;

    ASSERT_TRUE(mImageManager.SubscribeListener(otherListener).IsNone());

    const auto expectedStates = std::vector<ItemStateEnum>(
        {ItemStateEnum::eInstalling, ItemStateEnum::eDownloading, ItemStateEnum::eInstalled});

    std::vector<ItemStateEnum> otherStates;

    EXPECT_CALL(otherListener, OnItemStatusChanged(_)).WillRepeatedly(Invoke([&](const UpdateItemStatus& status) {
        otherStates.push_back(status.mState.GetValue());
    }));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), expectedStates);
    EXPECT_EQ(otherStates, expectedStates);

    ASSERT_TRUE(mImageManager.UnsubscribeListener(otherListener).IsNone());

    const auto otherManifest = AddManifest(2, 1, 1);

    err = mImageManager.Install(MakeInstallInfo(otherManifest.mManifest, false, UpdateItemTypeEnum::eService, "item2"));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates().size(), 2 * expectedStates.size());
    EXPECT_EQ(otherStates, expectedStates);
}

TEST_F(CommonImageManagerInstallTest, InstallManifestRoot)
{
    const auto manifest = AddManifest(1, 2, 2, true);
    const auto itemInfo = MakeInstallInfo(manifest.mManifest);

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(5);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cInstallBlobSize)).Times(5);
    EXPECT_CALL(mFSPlatform, Sync(String(cTestImagePath), true)).Times(1);

    auto err = mImageManager.Install(itemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>(
            {ItemStateEnum::eInstalling, ItemStateEnum::eDownloading, ItemStateEnum::eInstalled}));

    for (const auto& status : GetNotifiedStatuses()) {
        EXPECT_EQ(status.mItemID, cTestItemID);
        EXPECT_EQ(status.mVersion, cTestVersion);
        EXPECT_EQ(status.mType, UpdateItemTypeEnum::eService);
        EXPECT_TRUE(status.mError.IsNone());
    }

    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({
            MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest, false, "", 0),
            MakeBlobInfo(manifest.mItemConfig, cItemConfigType),
            MakeBlobInfo(manifest.mImageConfig, cImageConfigType),
            MakeBlobInfo(manifest.mLayers[0], cTestLayerType, true, manifest.mDiffIDs[0].mDigest),
            MakeBlobInfo(manifest.mLayers[1], cTestLayerType, true, manifest.mDiffIDs[1].mDigest),
        }));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
    EXPECT_EQ(item->mRootDigest, manifest.mManifest.mDigest.c_str());
    EXPECT_EQ(item->mType, UpdateItemTypeEnum::eService);
    EXPECT_TRUE(item->mError.IsNone());

    EXPECT_EQ(mAcceptCount.load(), 5);
    EXPECT_EQ(mReleaseCount.load(), 0);

    for (const auto& layer : manifest.mLayers) {
        EXPECT_TRUE(FileExists(layer.mPath.c_str()));
    }

    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerInstallTest, InstallIndexRoot)
{
    const auto manifest = AddManifest(1, 1, 1);
    const auto index    = AddIndex(1000, {manifest});

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(4);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cInstallBlobSize)).Times(4);

    auto err = mImageManager.Install(MakeInstallInfo(index, true));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>(
            {ItemStateEnum::eInstalling, ItemStateEnum::eDownloading, ItemStateEnum::eInstalled}));

    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({
            MakeBlobInfo(index, oci::cMediaTypeImageIndex, false, "", 0),
            MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest),
            MakeBlobInfo(manifest.mImageConfig, cImageConfigType),
            MakeBlobInfo(manifest.mLayers[0], cTestLayerType, true, manifest.mDiffIDs[0].mDigest),
        }));

    EXPECT_TRUE(FileExists(index.mPath.c_str()));
    EXPECT_TRUE(FileExists(manifest.mManifest.mPath.c_str()));
    EXPECT_TRUE(FileExists(manifest.mImageConfig.mPath.c_str()));
    EXPECT_TRUE(FileExists(manifest.mLayers[0].mPath.c_str()));
    EXPECT_EQ(mAcceptCount.load(), 4);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerInstallTest, InstallAllBlobsValid)
{
    const auto manifest = AddManifest(1, 2, 2, true);

    CreateValidBlobs(manifest);

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(_)).Times(0);
    EXPECT_CALL(mSpaceAllocator, FreeSpace(_)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eInstalled}));
    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion).size(), 5);
}

TEST_F(CommonImageManagerInstallTest, InstallInvalidLayer)
{
    const auto manifest = AddManifest(1, 2, 2);

    CreateValidBlobs(manifest);
    CreateFile(manifest.mLayers[1].mPath.c_str(), cOtherHash);

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mLayers[1].mPath.c_str()), _)).Times(1);
    EXPECT_CALL(mSpaceAllocator, FreeSpace(cInstallBlobSize)).Times(1);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cInstallBlobSize)).Times(1);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(manifest.mLayers[1].mPath), manifest.mLayers[1].mHash);
    EXPECT_EQ(mAcceptCount.load(), 1);
    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>(
            {ItemStateEnum::eInstalling, ItemStateEnum::eDownloading, ItemStateEnum::eInstalled}));
}

TEST_F(CommonImageManagerInstallTest, InstallInvalidMetadataBlob)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);
    CreateFile(manifest.mImageConfig.mPath.c_str(), cOtherHash);

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mImageConfig.mPath.c_str()), _)).Times(1);
    EXPECT_CALL(mSpaceAllocator, FreeSpace(cInstallBlobSize)).Times(1);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cInstallBlobSize)).Times(1);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(manifest.mImageConfig.mPath), manifest.mImageConfig.mHash);
    EXPECT_EQ(mAcceptCount.load(), 1);
}

TEST_F(CommonImageManagerInstallTest, InstallServiceDiffIDs)
{
    const auto manifest = AddManifest(1, 2, 2);

    CreateValidBlobs(manifest);
    ASSERT_TRUE(fs::RemoveAll(manifest.mLayers[1].mPath.c_str()).IsNone());

    EXPECT_CALL(mOCISpec, LoadImageConfig(String(manifest.mImageConfig.mPath.c_str()), _)).Times(1);
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mLayers[1].mPath.c_str()), _)).Times(1);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    using LayerCall = TestImageManager::LayerCall;

    EXPECT_EQ(mImageManager.GetValidateLayerCalls(),
        std::vector<LayerCall>({{manifest.mLayers[0].mDigest, manifest.mDiffIDs[0].mDigest},
            {manifest.mLayers[1].mDigest, manifest.mDiffIDs[1].mDigest}}));
}

TEST_F(CommonImageManagerInstallTest, InstallLayerValid)
{
    const auto  manifest = AddManifest(1, 1, 1);
    const auto& layer    = manifest.mLayers[0];

    // The layer blob is removed after the layer is unpacked.
    CreateValidBlobs(manifest);
    ASSERT_TRUE(fs::RemoveAll(layer.mPath.c_str()).IsNone());

    EXPECT_CALL(mImageManager, CheckLayer(String(layer.mPath.c_str()), _, String(manifest.mDiffIDs[0].mDigest.c_str())))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mImageManager, RemoveLayer(_, _, _)).Times(0);
    EXPECT_CALL(mImageManager, UnpackLayer(_, _, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(layer.mPath.c_str()));
}

TEST_F(CommonImageManagerInstallTest, InstallLayerNotSupported)
{
    const auto manifest = AddManifest(1, 2, 2);

    CreateValidBlobs(manifest);
    ASSERT_TRUE(fs::RemoveAll(manifest.mLayers[1].mPath.c_str()).IsNone());

    EXPECT_CALL(mImageManager, CheckLayer(_, _, _)).Times(2);
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mLayers[1].mPath.c_str()), _)).Times(1);
    EXPECT_CALL(mImageManager, RemoveLayer(_, _, _)).Times(0);
    EXPECT_CALL(mImageManager, UnpackLayer(_, _, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerInstallTest, InstallLayerInvalid)
{
    const auto  manifest = AddManifest(1, 1, 1);
    const auto& layer    = manifest.mLayers[0];
    const auto  diffID   = String(manifest.mDiffIDs[0].mDigest.c_str());

    constexpr size_t cUnpackedSize = 100;

    CreateValidBlobs(manifest);

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);

    {
        InSequence seq;

        EXPECT_CALL(mImageManager, CheckLayer(String(layer.mPath.c_str()), _, diffID))
            .WillOnce(Return(ErrorEnum::eInvalidChecksum));
        EXPECT_CALL(mImageManager, RemoveLayer(String(layer.mPath.c_str()), _, diffID))
            .WillOnce(Return(RetWithError<size_t>(cUnpackedSize)));
        EXPECT_CALL(mSpaceAllocator, FreeSpace(cUnpackedSize));
        EXPECT_CALL(mImageManager, UnpackLayer(String(layer.mPath.c_str()), _, diffID))
            .WillOnce(Return(ErrorEnum::eNone));
    }

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerInstallTest, InstallLayerMissing)
{
    const auto  manifest = AddManifest(1, 1, 1);
    const auto& layer    = manifest.mLayers[0];

    CreateValidBlobs(manifest);
    ASSERT_TRUE(fs::RemoveAll(layer.mPath.c_str()).IsNone());

    {
        InSequence seq;

        EXPECT_CALL(mImageManager, CheckLayer(String(layer.mPath.c_str()), _, _))
            .WillOnce(Return(ErrorEnum::eNotFound));
        EXPECT_CALL(mImageManager, RemoveLayer(String(layer.mPath.c_str()), _, _));
        EXPECT_CALL(mImageManager, DownloadBlob(String(layer.mPath.c_str()), _));
        EXPECT_CALL(mImageManager, UnpackLayer(String(layer.mPath.c_str()), _, _)).WillOnce(Return(ErrorEnum::eNone));
    }

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerInstallTest, InstallRemoveLayerFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);

    EXPECT_CALL(mImageManager, CheckLayer(_, _, _)).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mImageManager, RemoveLayer(_, _, _))
        .WillOnce(Return(RetWithError<size_t>(0, Error(ErrorEnum::eFailed, "remove failed"))));
    EXPECT_CALL(mImageManager, UnpackLayer(_, _, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_STREQ(item->mError.Message(), "remove failed");
}

TEST_F(CommonImageManagerInstallTest, InstallUnpackLayerFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);

    EXPECT_CALL(mImageManager, CheckLayer(_, _, _)).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mImageManager, UnpackLayer(_, _, _)).WillOnce(Return(Error(ErrorEnum::eFailed, "unpack failed")));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_STREQ(item->mError.Message(), "unpack failed");
}

TEST_F(CommonImageManagerInstallTest, InstallWithoutImageConfig)
{
    const auto manifest = AddManifest(1, 2, 0);

    mManifests[manifest.mManifest.mPath]->mConfig = oci::ContentDescriptor {};

    EXPECT_CALL(mOCISpec, LoadImageConfig(_, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(manifest.mImageConfig.mPath.c_str()));

    using LayerCall = TestImageManager::LayerCall;

    const std::vector<LayerCall> expectedCalls {{manifest.mLayers[0].mDigest, ""}, {manifest.mLayers[1].mDigest, ""}};

    EXPECT_EQ(mImageManager.GetValidateLayerCalls(), expectedCalls);

    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({
            MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest, false, "", 0),
            MakeBlobInfo(manifest.mLayers[0], cTestLayerType, true),
            MakeBlobInfo(manifest.mLayers[1], cTestLayerType, true),
        }));
}

TEST_F(CommonImageManagerInstallTest, InstallMissingDiffID)
{
    const auto manifest = AddManifest(1, 2, 1);

    CreateValidBlobs(manifest);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mImageManager.GetValidateLayerCalls().empty());

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(statuses[1].mError.Is(ErrorEnum::eInvalidArgument));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CommonImageManagerInstallTest, InstallRootDigestMismatch)
{
    const auto manifest = AddManifest(1, 1, 1);

    StoreItem(cTestItemID, cTestVersion, cOtherDigest, ItemStateEnum::eInstalled);

    const auto storedItems = mStorage.GetItems();

    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mFSPlatform, Sync(_, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorage.GetItems(), storedItems);
    EXPECT_TRUE(mStorage.GetBlobs(cTestItemID, cTestVersion).empty());
}

TEST_F(CommonImageManagerInstallTest, InstallMalformedRootDigest)
{
    auto itemInfo = MakeInstallInfo(MakeTestBlob(1));

    itemInfo.mRootDigest = "sha256:../malformed";

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(_)).Times(0);

    auto err = mImageManager.Install(itemInfo);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(statuses[1].mError.Is(ErrorEnum::eInvalidArgument));

    EXPECT_TRUE(mStorage.GetBlobs(cTestItemID, cTestVersion).empty());

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CommonImageManagerInstallTest, InstallMalformedLayerDigest)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);

    constexpr auto cMalformedDigest = "sha256:../malformed";

    mManifests[manifest.mManifest.mPath]->mLayers[0].mDigest = cMalformedDigest;

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(_)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mImageManager.GetValidateLayerCalls().empty());

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(statuses[1].mError.Is(ErrorEnum::eInvalidArgument));

    // No blob reference is stored for the layer with the malformed digest.
    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({
            MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest, false, "", 0),
            MakeBlobInfo(manifest.mImageConfig, cImageConfigType),
        }));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CommonImageManagerInstallTest, InstallUnsupportedDigestAlgorithm)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);

    mManifests[manifest.mManifest.mPath]->mLayers[0].mDigest = "md5:0123456789abcdef0123456789abcdef";

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eNotSupported)) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eNotSupported));
}

TEST_F(CommonImageManagerInstallTest, InstallDownloadedBlobInvalid)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);
    ASSERT_TRUE(fs::RemoveAll(manifest.mLayers[0].mPath.c_str()).IsNone());

    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mLayers[0].mPath.c_str()), _))
        .WillOnce(
            Invoke([](const String& path, const BlobInfo&) { return fs::WriteStringToFile(path, cOtherHash, 0600); }));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eInvalidChecksum));
}

TEST_F(CommonImageManagerInstallTest, InstallCorruptManifest)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mOCISpec, LoadImageManifest(String(manifest.mManifest.mPath.c_str()), _))
        .WillOnce(Return(Error(ErrorEnum::eFailed, "corrupt manifest")));
    EXPECT_CALL(mFSPlatform, Sync(String(cTestImagePath), true)).Times(1);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(statuses[1].mError.Is(ErrorEnum::eFailed));
    EXPECT_STREQ(statuses[1].mError.Message(), "corrupt manifest");

    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest, false, "", 0)}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eFailed));
    EXPECT_STREQ(item->mError.Message(), "corrupt manifest");
}

TEST_F(CommonImageManagerInstallTest, InstallAllocateSpaceFails)
{
    const auto manifest = AddManifest(1, 2, 2);

    CreateValidBlobs(manifest);
    ASSERT_TRUE(fs::RemoveAll(manifest.mLayers[1].mPath.c_str()).IsNone());

    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cInstallBlobSize))
        .WillOnce(Invoke([](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            return {nullptr, ErrorEnum::eNoMemory};
        }));
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mFSPlatform, Sync(String(cTestImagePath), true)).Times(1);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(statuses[1].mError.Is(ErrorEnum::eNoMemory));

    EXPECT_FALSE(FileExists(manifest.mLayers[1].mPath.c_str()));

    auto blobs = mStorage.GetBlobs(cTestItemID, cTestVersion);
    ASSERT_EQ(blobs.size(), 4);
    EXPECT_EQ(blobs.back(), MakeBlobInfo(manifest.mLayers[1], cTestLayerType, true, manifest.mDiffIDs[1].mDigest));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eNoMemory));
}

TEST_F(CommonImageManagerInstallTest, InstallRemovedItemRestoresOutdatedItem)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);
    StoreItem(cTestItemID, cTestVersion, manifest.mManifest.mDigest.c_str(), ItemStateEnum::eRemoved);

    EXPECT_CALL(mSpaceAllocator, RestoreOutdatedItem(String(cTestItemID), String(cTestVersion)))
        .WillOnce(Return(ErrorEnum::eNotFound));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerInstallTest, InstallRemovedItemSetInstallingStateFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);
    StoreItem(cTestItemID, cTestVersion, manifest.mManifest.mDigest.c_str(), ItemStateEnum::eRemoved);

    mStorage.SetFailUpdateStates({ItemStateEnum::eInstalling});

    // The item stays in the eRemoved state, so it stays outdated.
    EXPECT_CALL(mSpaceAllocator, RestoreOutdatedItem(_, _)).Times(0);
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
}

TEST_F(CommonImageManagerInstallTest, InstallRemovedItemRestoreFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);
    StoreItem(cTestItemID, cTestVersion, manifest.mManifest.mDigest.c_str(), ItemStateEnum::eRemoved);

    EXPECT_CALL(mSpaceAllocator, RestoreOutdatedItem(String(cTestItemID), String(cTestVersion)))
        .WillOnce(Return(ErrorEnum::eFailed));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eInstalled}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerInstallTest, InstallStoredItemDoesNotRestoreOutdatedItem)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);
    StoreItem(cTestItemID, cTestVersion, manifest.mManifest.mDigest.c_str(), ItemStateEnum::eFailed);

    EXPECT_CALL(mSpaceAllocator, RestoreOutdatedItem(_, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
    EXPECT_TRUE(item->mError.IsNone());
}

TEST_F(CommonImageManagerInstallTest, InstallStorageFullRemovesOutdatedItem)
{
    const auto manifest = AddManifest(1, 1, 1);

    for (size_t i = 0; i < cMaxNumStoredItems - 1; ++i) {
        AddStoredItem(("item" + std::to_string(i + 100)).c_str(), cTestVersion, ItemStateEnum::eInstalled);
    }

    AddStoredItem("outdated", cTestVersion, ItemStateEnum::eRemoved);

    EXPECT_CALL(mSpaceAllocator, RemoveOutdatedItem()).WillOnce(Invoke([this]() {
        return mImageManager.GetItemRemover().RemoveItem("outdated", cTestVersion);
    }));
    EXPECT_CALL(mListener, OnItemRemovedFromCache(String("outdated"), String(cTestVersion)));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorage.GetItems().size(), cMaxNumStoredItems);
    EXPECT_FALSE(GetStoredItem("outdated", cTestVersion).has_value());

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerInstallTest, InstallStorageFullOutdatedItemSkipped)
{
    const auto manifest = AddManifest(1, 1, 1);

    for (size_t i = 0; i < cMaxNumStoredItems - 1; ++i) {
        AddStoredItem(("item" + std::to_string(i + 100)).c_str(), cTestVersion, ItemStateEnum::eInstalled);
    }

    AddStoredItem("outdated", cTestVersion, ItemStateEnum::eRemoved);

    // The first outdated item is skipped by the item remover, so the next one is removed.
    EXPECT_CALL(mSpaceAllocator, RemoveOutdatedItem())
        .WillOnce(Return(RetWithError<size_t>(0, ErrorEnum::eNone)))
        .WillOnce(Invoke([this]() { return mImageManager.GetItemRemover().RemoveItem("outdated", cTestVersion); }));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorage.GetItems().size(), cMaxNumStoredItems);
    EXPECT_FALSE(GetStoredItem("outdated", cTestVersion).has_value());
}

TEST_F(CommonImageManagerInstallTest, InstallStorageFullNoOutdatedItem)
{
    const auto manifest = AddManifest(1, 1, 1);

    for (size_t i = 0; i < cMaxNumStoredItems; ++i) {
        AddStoredItem(("item" + std::to_string(i + 100)).c_str(), cTestVersion, ItemStateEnum::eInstalled);
    }

    const auto storedItems = mStorage.GetItems();

    EXPECT_CALL(mSpaceAllocator, RemoveOutdatedItem()).WillOnce(Return(RetWithError<size_t>(0, ErrorEnum::eNotFound)));
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorage.GetItems(), storedItems);
}

TEST_F(CommonImageManagerInstallTest, InstallStorageFullRemoveOutdatedItemFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    for (size_t i = 0; i < cMaxNumStoredItems; ++i) {
        AddStoredItem(("item" + std::to_string(i + 100)).c_str(), cTestVersion, ItemStateEnum::eRemoved);
    }

    const auto storedItems = mStorage.GetItems();

    EXPECT_CALL(mSpaceAllocator, RemoveOutdatedItem()).WillOnce(Return(RetWithError<size_t>(0, ErrorEnum::eFailed)));
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorage.GetItems(), storedItems);
}

TEST_F(CommonImageManagerInstallTest, InstallStoredItemStorageFull)
{
    const auto manifest = AddManifest(1, 1, 1);

    StoreItem(cTestItemID, cTestVersion, manifest.mManifest.mDigest.c_str(), ItemStateEnum::eFailed);

    for (size_t i = 0; i < cMaxNumStoredItems - 1; ++i) {
        AddStoredItem(("item" + std::to_string(i + 100)).c_str(), cTestVersion, ItemStateEnum::eInstalled);
    }

    // The stored item already has its storage record.
    EXPECT_CALL(mSpaceAllocator, RemoveOutdatedItem()).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerInstallTest, InstallDownloadFails)
{
    const auto manifest = AddManifest(1, 2, 2);

    CreateValidBlobs(manifest);
    ASSERT_TRUE(fs::RemoveAll(manifest.mLayers[0].mPath.c_str()).IsNone());
    ASSERT_TRUE(fs::RemoveAll(manifest.mLayers[1].mPath.c_str()).IsNone());

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).WillRepeatedly(Invoke(DownloadTestBlob));
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mLayers[1].mPath.c_str()), _))
        .WillOnce(Invoke([](const String& path, const BlobInfo&) {
            EXPECT_TRUE(fs::WriteStringToFile(path, "partial", 0600).IsNone());

            return Error(ErrorEnum::eFailed, "download failed");
        }));
    EXPECT_CALL(mFSPlatform, Sync(String(cTestImagePath), true)).Times(1);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 3);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eDownloading);
    EXPECT_EQ(statuses[2].mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(statuses[2].mError.Is(ErrorEnum::eFailed));
    EXPECT_STREQ(statuses[2].mError.Message(), "download failed");

    EXPECT_TRUE(FileExists(manifest.mLayers[0].mPath.c_str()));
    EXPECT_FALSE(FileExists(manifest.mLayers[1].mPath.c_str()));
    EXPECT_EQ(mAcceptCount.load(), 1);
    EXPECT_EQ(mReleaseCount.load(), 1);

    auto blobs = mStorage.GetBlobs(cTestItemID, cTestVersion);
    ASSERT_EQ(blobs.size(), 4);
    EXPECT_EQ(blobs.back(), MakeBlobInfo(manifest.mLayers[1], cTestLayerType, true, manifest.mDiffIDs[1].mDigest));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eFailed));
    EXPECT_STREQ(item->mError.Message(), "download failed");

    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eFailed);
}

TEST_F(CommonImageManagerInstallTest, InstallMetadataDownloadFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mManifest.mPath.c_str()), _))
        .WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cInstallBlobSize)).Times(1);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(manifest.mManifest.mPath.c_str()));
    EXPECT_EQ(mReleaseCount.load(), 1);
    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest, false, "", 0)}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
}

TEST_F(CommonImageManagerInstallTest, InstallSyncFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);

    EXPECT_CALL(mFSPlatform, Sync(String(cTestImagePath), true)).WillOnce(Return(ErrorEnum::eFailed));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eFailed}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eFailed));
}

TEST_F(CommonImageManagerInstallTest, InstallSetInstalledStateFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);

    mStorage.SetFailUpdateStates({ItemStateEnum::eInstalled});

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eFailed}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_STREQ(item->mError.Message(), "update failed");
}

TEST_F(CommonImageManagerInstallTest, InstallSetFailedStateFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    // Both the eInstalled and the eFailed state updates fail. The blobs are downloaded, so eDownloading is notified.
    mStorage.SetFailUpdateStates({ItemStateEnum::eInstalled, ItemStateEnum::eFailed});

    std::optional<ItemStateEnum> reportedOnFailed;

    EXPECT_CALL(mListener, OnItemStatusChanged(_)).WillRepeatedly(Invoke([&](const UpdateItemStatus& status) {
        {
            std::lock_guard lock {mStatusesMutex};

            mStatuses.push_back(status);
        }

        if (status.mState == ItemStateEnum::eFailed) {
            reportedOnFailed = GetReportedState(cTestItemID, cTestVersion);
        }
    }));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 3);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eDownloading);
    EXPECT_EQ(statuses[2].mState, ItemStateEnum::eFailed);
    EXPECT_STREQ(statuses[2].mError.Message(), "update failed");

    // While the final eFailed state is notified, the notified eFailed state is reported.
    EXPECT_EQ(reportedOnFailed, ItemStateEnum::eFailed);

    // The item is left in eInstalling state.
    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eInstalling);
}

TEST_F(CommonImageManagerInstallTest, GetItemsStatusesDuringInstall)
{
    const auto manifest = AddManifest(1, 1, 1);

    std::promise<void> downloadStarted;
    std::promise<void> downloadReleased;
    auto               downloadReleasedFuture = downloadReleased.get_future();
    std::atomic<bool>  installingChecked {false};

    EXPECT_CALL(mListener, OnItemStatusChanged(_)).WillRepeatedly(Invoke([&](const UpdateItemStatus& status) {
        if (status.mState == ItemStateEnum::eInstalling) {
            EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eInstalling);

            installingChecked = true;
        }
    }));

    EXPECT_CALL(mImageManager, DownloadBlob(_, _))
        .WillOnce(Invoke([&](const String& path, const BlobInfo& blobInfo) {
            downloadStarted.set_value();
            downloadReleasedFuture.wait();

            return DownloadTestBlob(path, blobInfo);
        }))
        .WillRepeatedly(Invoke(DownloadTestBlob));

    Error installErr;

    std::thread installThread([&]() { installErr = mImageManager.Install(MakeInstallInfo(manifest.mManifest)); });

    downloadStarted.get_future().wait();

    EXPECT_TRUE(installingChecked.load());
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eDownloading);

    downloadReleased.set_value();
    installThread.join();

    ASSERT_TRUE(installErr.IsNone()) << tests::utils::ErrorToStr(installErr);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerInstallTest, GetItemsStatusesDuringStoredItemInstall)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);

    auto storedItem = CreateDeployableItem(cTestItemID, cTestVersion, ItemStateEnum::eFailed);

    storedItem.mType       = UpdateItemTypeEnum::eComponent;
    storedItem.mRootDigest = manifest.mManifest.mDigest.c_str();

    ASSERT_TRUE(mStorage.AddDeployableItem(storedItem).IsNone());

    std::optional<UpdateItemStatus> reportedOnInstalling;

    EXPECT_CALL(mListener, OnItemStatusChanged(_)).WillRepeatedly(Invoke([&](const UpdateItemStatus& status) {
        EXPECT_EQ(status.mType, UpdateItemTypeEnum::eComponent);

        if (status.mState != ItemStateEnum::eInstalling) {
            return;
        }

        auto statuses = std::make_unique<StaticArray<UpdateItemStatus, cMaxNumStoredItems>>();

        EXPECT_TRUE(mImageManager.GetItemsStatuses(*statuses).IsNone());

        if (const auto* reported = FindStatus(*statuses, cTestItemID, cTestVersion); reported != nullptr) {
            reportedOnInstalling = *reported;
        }
    }));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest, false, UpdateItemTypeEnum::eComponent));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_TRUE(reportedOnInstalling.has_value());
    EXPECT_EQ(reportedOnInstalling->mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(reportedOnInstalling->mType, UpdateItemTypeEnum::eComponent);
}

TEST_F(CommonImageManagerInstallTest, InstallSameVersionConcurrently)
{
    const auto manifest = AddManifest(1, 1, 1);

    std::promise<void> downloadStarted;
    std::promise<void> downloadReleased;
    auto               downloadReleasedFuture = downloadReleased.get_future();
    std::atomic<int>   downloadCount {0};

    EXPECT_CALL(mImageManager, DownloadBlob(_, _))
        .WillRepeatedly(Invoke([&](const String& path, const BlobInfo& blobInfo) {
            if (downloadCount++ == 0) {
                downloadStarted.set_value();
                downloadReleasedFuture.wait();
            }

            return DownloadTestBlob(path, blobInfo);
        }));

    Error firstErr, secondErr;

    std::thread firstThread([&]() { firstErr = mImageManager.Install(MakeInstallInfo(manifest.mManifest)); });

    downloadStarted.get_future().wait();

    std::thread secondThread([&]() { secondErr = mImageManager.Install(MakeInstallInfo(manifest.mManifest)); });

    // Gives the second install time to reach LockItem: without waiting there, it would notify eInstalling and fail
    // the checks below.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(
        GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eDownloading}));
    EXPECT_EQ(downloadCount.load(), 1);

    downloadReleased.set_value();

    firstThread.join();
    secondThread.join();

    ASSERT_TRUE(firstErr.IsNone()) << tests::utils::ErrorToStr(firstErr);
    ASSERT_TRUE(secondErr.IsNone()) << tests::utils::ErrorToStr(secondErr);

    // The second install starts after the first one is finished, including its final notification, and finds all
    // blobs valid.
    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eDownloading, ItemStateEnum::eInstalled,
            ItemStateEnum::eInstalling, ItemStateEnum::eInstalled}));
    EXPECT_EQ(downloadCount.load(), 3);
}

TEST_F(CommonImageManagerInstallTest, InstallSharedBlobConcurrently)
{
    const auto sharedLayer = MakeTestBlob(500);
    const auto manifestA   = AddManifest(1, 0, 1, false, {sharedLayer});
    const auto manifestB   = AddManifest(200, 0, 1, false, {sharedLayer});

    CreateValidBlobs(manifestA);
    CreateValidBlobs(manifestB);
    ASSERT_TRUE(fs::RemoveAll(sharedLayer.mPath.c_str()).IsNone());

    std::promise<void> downloadStarted;
    std::promise<void> downloadReleased;
    auto               downloadReleasedFuture = downloadReleased.get_future();
    std::atomic<int>   sharedDownloadCount {0};

    const auto countSharedLayerCalls = [&sharedLayer](const std::vector<TestImageManager::LayerCall>& calls) {
        return std::count_if(calls.begin(), calls.end(),
            [&sharedLayer](const TestImageManager::LayerCall& call) { return call.mDigest == sharedLayer.mDigest; });
    };

    EXPECT_CALL(mImageManager, DownloadBlob(String(sharedLayer.mPath.c_str()), _))
        .WillOnce(Invoke([&](const String& path, const BlobInfo& blobInfo) {
            sharedDownloadCount++;
            downloadStarted.set_value();
            downloadReleasedFuture.wait();

            return DownloadTestBlob(path, blobInfo);
        }));

    Error errA, errB;

    std::thread threadA([&]() {
        errA
            = mImageManager.Install(MakeInstallInfo(manifestA.mManifest, false, UpdateItemTypeEnum::eService, "itemA"));
    });

    downloadStarted.get_future().wait();

    std::thread threadB([&]() {
        errB
            = mImageManager.Install(MakeInstallInfo(manifestB.mManifest, false, UpdateItemTypeEnum::eService, "itemB"));
    });

    // Gives item B time to reach LockBlob of the shared layer: without waiting there, it would validate and download
    // the shared layer a second time and fail the checks below.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(sharedDownloadCount.load(), 1);

    downloadReleased.set_value();

    threadA.join();
    threadB.join();

    ASSERT_TRUE(errA.IsNone()) << tests::utils::ErrorToStr(errA);
    ASSERT_TRUE(errB.IsNone()) << tests::utils::ErrorToStr(errB);

    // Item B finds the shared layer downloaded by item A valid.
    EXPECT_EQ(countSharedLayerCalls(mImageManager.GetValidateLayerCalls()), 2);
    EXPECT_EQ(sharedDownloadCount.load(), 1);

    for (const auto& itemID : {"itemA", "itemB"}) {
        auto blobs = mStorage.GetBlobs(itemID, cTestVersion);
        ASSERT_EQ(blobs.size(), 3);
        EXPECT_EQ(blobs.back().mDigest, sharedLayer.mDigest.c_str());

        auto item = GetStoredItem(itemID, cTestVersion);
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
    }

    EXPECT_EQ(ReadFile(sharedLayer.mPath), sharedLayer.mHash);
}

/***********************************************************************************************************************
 * Remove item suite
 **********************************************************************************************************************/

class CommonImageManagerRemoveTest : public CommonImageManagerInstallTest { };

TEST_F(CommonImageManagerRemoveTest, RemoveInstalledItem)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);
    StoreItem(cTestItemID, cTestVersion, manifest.mManifest.mDigest.c_str(), ItemStateEnum::eInstalled);

    const auto blobInfo = MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest);

    ASSERT_TRUE(mStorage.AddItemBlob(cTestItemID, cTestVersion, blobInfo).IsNone());

    Time outdatedTimestamp;

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), _))
        .WillOnce(Invoke([&](const String&, const String&, const Time& timestamp) {
            outdatedTimestamp = timestamp;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManager, RemoveLayer(_, _, _)).Times(0);
    EXPECT_CALL(mSpaceAllocator, FreeSpace(_)).Times(0);

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eRemoving, ItemStateEnum::eRemoved}));

    for (const auto& status : GetNotifiedStatuses()) {
        EXPECT_EQ(status.mType, UpdateItemTypeEnum::eService);
        EXPECT_TRUE(status.mError.IsNone());
    }

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
    EXPECT_EQ(item->mTimestamp, outdatedTimestamp);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eRemoved);

    // The item blobs and blob references are kept in the cache.
    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion), std::vector<ItemBlobInfo>({blobInfo}));
    EXPECT_TRUE(FileExists(manifest.mManifest.mPath.c_str()));
}

TEST_F(CommonImageManagerRemoveTest, RemoveFailedAndInstallingItems)
{
    for (const auto state : {ItemStateEnum::eFailed, ItemStateEnum::eInstalling}) {
        const auto version = std::string("1.0.") + std::to_string(static_cast<int>(state));

        AddStoredItem(cTestItemID, version.c_str(), state, Error(ErrorEnum::eFailed, "install failed"));

        EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(version.c_str()), _))
            .WillOnce(Return(ErrorEnum::eNone));

        auto err = mImageManager.Remove(cTestItemID, version.c_str());
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        auto item = GetStoredItem(cTestItemID, version.c_str());
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
        EXPECT_TRUE(item->mError.IsNone());
    }

    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>(
            {ItemStateEnum::eRemoving, ItemStateEnum::eRemoved, ItemStateEnum::eRemoving, ItemStateEnum::eRemoved}));
}

TEST_F(CommonImageManagerRemoveTest, RemoveNotStoredItem)
{
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).Times(0);
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerRemoveTest, RemoveRemovedItem)
{
    AddStoredItem(cTestItemID, cTestVersion, ItemStateEnum::eRemoved);

    const auto storedItem = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(storedItem.has_value());

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).Times(0);
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetStoredItem(cTestItemID, cTestVersion), storedItem);
}

TEST_F(CommonImageManagerRemoveTest, RemoveSetRemovedStateFails)
{
    AddStoredItem(cTestItemID, cTestVersion, ItemStateEnum::eInstalled);

    mStorage.SetFailUpdateStates({ItemStateEnum::eRemoved});

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).Times(0);

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    const auto statuses = GetNotifiedStatuses();

    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eRemoving);
    EXPECT_TRUE(statuses[0].mError.IsNone());
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eInstalled);
    EXPECT_EQ(statuses[1].mType, UpdateItemTypeEnum::eService);
    EXPECT_TRUE(statuses[1].mError.Is(ErrorEnum::eFailed));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerRemoveTest, RemoveAddOutdatedItemFails)
{
    AddStoredItem(cTestItemID, cTestVersion, ItemStateEnum::eInstalled);

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), _))
        .WillOnce(Return(ErrorEnum::eFailed));

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eRemoving, ItemStateEnum::eRemoved}));

    // The item stays in the eRemoved state and is registered as outdated on the next start.
    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
}

TEST_F(CommonImageManagerRemoveTest, GetItemsStatusesDuringRemove)
{
    AddStoredItem(cTestItemID, cTestVersion, ItemStateEnum::eInstalled);

    std::optional<ItemStateEnum> reportedOnRemoving;

    EXPECT_CALL(mListener, OnItemStatusChanged(_)).WillRepeatedly(Invoke([&](const UpdateItemStatus& status) {
        if (status.mState == ItemStateEnum::eRemoving) {
            reportedOnRemoving = GetReportedState(cTestItemID, cTestVersion);
        }
    }));

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(reportedOnRemoving, ItemStateEnum::eRemoving);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eRemoved);
}

TEST_F(CommonImageManagerRemoveTest, RemoveWaitsForInstall)
{
    const auto manifest = AddManifest(1, 1, 1);

    std::promise<void> downloadStarted;
    std::promise<void> downloadReleased;
    auto               downloadReleasedFuture = downloadReleased.get_future();

    EXPECT_CALL(mImageManager, DownloadBlob(_, _))
        .WillOnce(Invoke([&](const String& path, const BlobInfo& blobInfo) {
            downloadStarted.set_value();
            downloadReleasedFuture.wait();

            return DownloadTestBlob(path, blobInfo);
        }))
        .WillRepeatedly(Invoke(DownloadTestBlob));
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), _))
        .WillOnce(Return(ErrorEnum::eNone));

    Error installErr, removeErr;

    std::thread installThread([&]() { installErr = mImageManager.Install(MakeInstallInfo(manifest.mManifest)); });

    downloadStarted.get_future().wait();

    std::thread removeThread([&]() { removeErr = mImageManager.Remove(cTestItemID, cTestVersion); });

    // Gives the remove time to reach LockItem: without waiting there, it would notify eRemoving and fail the check
    // below.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(
        GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eDownloading}));

    downloadReleased.set_value();

    installThread.join();
    removeThread.join();

    ASSERT_TRUE(installErr.IsNone()) << tests::utils::ErrorToStr(installErr);
    ASSERT_TRUE(removeErr.IsNone()) << tests::utils::ErrorToStr(removeErr);

    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eDownloading, ItemStateEnum::eInstalled,
            ItemStateEnum::eRemoving, ItemStateEnum::eRemoved}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
}

/***********************************************************************************************************************
 * Remove outdated item suite
 **********************************************************************************************************************/

class CommonImageManagerRemoveOutdatedTest : public CommonImageManagerInstallTest {
protected:
    void SetUp() override
    {
        CommonImageManagerInstallTest::SetUp();

        ON_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).WillByDefault(Return(ErrorEnum::eNone));
    }

    void InstallAndRemove(const TestManifest& manifest, const char* itemID = cTestItemID)
    {
        auto err = mImageManager.Install(
            MakeInstallInfo(manifest.mManifest, false, UpdateItemTypeEnum::eService, itemID, cTestVersion));
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        err = mImageManager.Remove(itemID, cTestVersion);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    RetWithError<size_t> RemoveOutdated(const char* itemID = cTestItemID)
    {
        return mImageManager.GetItemRemover().RemoveItem(itemID, cTestVersion);
    }
};

TEST_F(CommonImageManagerRemoveOutdatedTest, RemoveUnusedBlobs)
{
    const auto sharedLayer = MakeTestBlob(500);
    const auto manifest    = AddManifest(1, 1, 2, false, {sharedLayer});
    const auto other       = AddManifest(200, 0, 1, false, {sharedLayer});

    InstallAndRemove(manifest);
    InstallAndRemove(other, "item2");

    const auto& ownLayer = manifest.mLayers[1];

    EXPECT_CALL(
        mImageManager, RemoveLayer(String(ownLayer.mPath.c_str()), _, String(manifest.mDiffIDs[1].mDigest.c_str())))
        .WillOnce(Return(RetWithError<size_t>(10, ErrorEnum::eNone)));
    EXPECT_CALL(mImageManager, RemoveLayer(String(sharedLayer.mPath.c_str()), _, _)).Times(0);
    EXPECT_CALL(mListener, OnItemRemovedFromCache(String(cTestItemID), String(cTestVersion)));
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mSpaceAllocator, FreeSpace(_)).Times(0);

    auto [size, err] = RemoveOutdated();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // Manifest, image config and own layer blobs plus the unpacked layer data.
    EXPECT_EQ(size, 3 * cInstallBlobSize + 10);

    EXPECT_FALSE(GetStoredItem(cTestItemID, cTestVersion).has_value());

    for (const auto* blob : {&manifest.mManifest, &manifest.mImageConfig, &ownLayer}) {
        EXPECT_FALSE(FileExists(blob->mPath.c_str())) << blob->mDigest;
    }

    // The shared layer is used by item2.
    EXPECT_TRUE(FileExists(sharedLayer.mPath.c_str()));
}

TEST_F(CommonImageManagerRemoveOutdatedTest, SkipNotRemovedItem)
{
    AddStoredItem(cTestItemID, cTestVersion, ItemStateEnum::eInstalled);

    EXPECT_CALL(mListener, OnItemRemovedFromCache(_, _)).Times(0);

    auto [size, err] = RemoveOutdated();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(size, 0);
    EXPECT_TRUE(GetStoredItem(cTestItemID, cTestVersion).has_value());

    // Not stored item is already removed from the cache.
    Tie(size, err) = RemoveOutdated("item2");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(size, 0);
}

TEST_F(CommonImageManagerRemoveOutdatedTest, SkipItemInProgress)
{
    AddStoredItem(cTestItemID, cTestVersion, ItemStateEnum::eInstalled);

    std::optional<RetWithError<size_t>> removeResult;

    // The item is already in the eRemoved state in the storage, but it is still being removed by RemoveItem.
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).WillRepeatedly(Invoke([&](const UpdateItemStatus& status) {
        if (status.mState == ItemStateEnum::eRemoved) {
            removeResult = RemoveOutdated();
        }
    }));
    EXPECT_CALL(mListener, OnItemRemovedFromCache(_, _)).Times(0);

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_TRUE(removeResult.has_value());
    EXPECT_TRUE(removeResult->mError.IsNone()) << tests::utils::ErrorToStr(removeResult->mError);
    EXPECT_EQ(removeResult->mValue, 0);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
}

TEST_F(CommonImageManagerRemoveOutdatedTest, SkipLockedBlob)
{
    const auto sharedLayer = MakeTestBlob(500);
    const auto manifest    = AddManifest(1, 0, 1, false, {sharedLayer});
    const auto other       = AddManifest(200, 0, 1, false, {sharedLayer});

    InstallAndRemove(manifest);

    ASSERT_TRUE(fs::RemoveAll(sharedLayer.mPath.c_str()).IsNone());

    std::promise<void> downloadStarted;
    std::promise<void> downloadReleased;
    auto               downloadReleasedFuture = downloadReleased.get_future();

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).WillRepeatedly(Invoke(DownloadTestBlob));
    EXPECT_CALL(mImageManager, DownloadBlob(String(sharedLayer.mPath.c_str()), _))
        .WillOnce(Invoke([&](const String& path, const BlobInfo& blobInfo) {
            downloadStarted.set_value();
            downloadReleasedFuture.wait();

            return DownloadTestBlob(path, blobInfo);
        }));

    Error installErr;

    std::thread installThread([&]() {
        installErr
            = mImageManager.Install(MakeInstallInfo(other.mManifest, false, UpdateItemTypeEnum::eService, "item2"));
    });

    downloadStarted.get_future().wait();

    // The shared layer is locked by item2 being downloaded: the item remover doesn't wait for it.
    auto [size, err] = RemoveOutdated();

    downloadReleased.set_value();
    installThread.join();

    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(size, 2 * cInstallBlobSize);
    EXPECT_FALSE(GetStoredItem(cTestItemID, cTestVersion).has_value());

    ASSERT_TRUE(installErr.IsNone()) << tests::utils::ErrorToStr(installErr);
    EXPECT_EQ(ReadFile(sharedLayer.mPath), sharedLayer.mHash);
}

TEST_F(CommonImageManagerRemoveOutdatedTest, RemoveLayerFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    InstallAndRemove(manifest);

    EXPECT_CALL(mImageManager, RemoveLayer(_, _, _)).WillOnce(Return(RetWithError<size_t>(10, ErrorEnum::eFailed)));
    EXPECT_CALL(mListener, OnItemRemovedFromCache(String(cTestItemID), String(cTestVersion)));

    auto [size, err] = RemoveOutdated();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The layer blob is still removed, the failed unpacked layer data is not counted.
    EXPECT_EQ(size, 3 * cInstallBlobSize);
    EXPECT_FALSE(FileExists(manifest.mLayers[0].mPath.c_str()));
}

TEST_F(CommonImageManagerRemoveOutdatedTest, RemoveDeployableItemFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    InstallAndRemove(manifest);

    mStorage.SetFailRemoveItem(true);

    EXPECT_CALL(mImageManager, RemoveLayer(_, _, _)).Times(0);
    EXPECT_CALL(mListener, OnItemRemovedFromCache(_, _)).Times(0);

    auto [size, err] = RemoveOutdated();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(size, 0);

    // No blob is removed and the item stays in the eRemoved state.
    for (const auto* blob : {&manifest.mManifest, &manifest.mImageConfig, &manifest.mLayers[0]}) {
        EXPECT_TRUE(FileExists(blob->mPath.c_str())) << blob->mDigest;
    }

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
}

} // namespace aos::common::imagemanager
