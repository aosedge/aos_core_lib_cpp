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
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <core/common/tests/mocks/fileinfoprovidermock.hpp>
#include <core/common/tests/mocks/fsmock.hpp>
#include <core/common/tests/mocks/itemstatuslistenermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tests/stubs/imagemanagerstoragestub.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>

#include <core/common/imagemanager/imagemanager.hpp>

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

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

fs::FileInfo GetFileInfoByHash(const String& hash)
{
    fs::FileInfo fileInfo;

    auto err = hash.HexToByteArray(fileInfo.mCheckSum);
    EXPECT_TRUE(err.IsNone()) << "Failed to convert hash to byte array: " << tests::utils::ErrorToStr(err);

    fileInfo.mSize = cTestBlobSize;

    return fileInfo;
}

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

oci::ContentDescriptor GetLayerDescriptor()
{
    return oci::ContentDescriptor {cTestLayerType, cTestDigest, cTestBlobSize};
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

Error DownloadTestBlob(const String& digest, const String& path)
{
    // Write the blob hash as the blob content.
    return fs::WriteStringToFile(path, String(digest.CStr() + std::strlen("sha256:")), 0600);
}

/***********************************************************************************************************************
 * Test storage
 **********************************************************************************************************************/

class TestStorage : public StorageStub {
public:
    Error UpdateDeployableItem(const DeployableItemInfo& item) override
    {
        if (mFailUpdateStates.count(item.mState.GetValue()) != 0) {
            return Error(ErrorEnum::eFailed, "update failed");
        }

        return StorageStub::UpdateDeployableItem(item);
    }

    std::vector<ItemBlobInfo> GetBlobs(const String& itemID, const String& version)
    {
        auto blobs = std::make_unique<StaticArray<ItemBlobInfo, cMaxNumItemBlobs>>();

        auto err = GetItemBlobs(itemID, version, *blobs);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        return std::vector<ItemBlobInfo>(blobs->begin(), blobs->end());
    }

    std::set<ItemStateEnum> mFailUpdateStates;
};

/***********************************************************************************************************************
 * Test image manager
 **********************************************************************************************************************/

class TestImageManager : public ImageManager {
public:
    using ImageManager::Init;
    using ImageManager::InstallItemInfo;
    using ImageManager::RemoveLayer;
    using ImageManager::ValidateBlob;

    struct LayerCall {
        std::string mDigest;
        std::string mDiffDigest;

        bool operator==(const LayerCall& rhs) const { return mDigest == rhs.mDigest && mDiffDigest == rhs.mDiffDigest; }
    };

    Error Install(const InstallItemInfo& itemInfo) { return InstallItem(itemInfo); }
    Error Remove(const String& itemID, const String& version) { return RemoveItem(itemID, version); }

    Error InstallLayer(
        const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, const String& diffDigest) override
    {
        {
            std::lock_guard lock {mLayerCallsMutex};

            mInstallLayerCalls.push_back({descriptor.mDigest.CStr(), diffDigest.CStr()});
        }

        return ImageManager::InstallLayer(itemInfo, descriptor, diffDigest);
    }

    std::vector<LayerCall> GetInstallLayerCalls()
    {
        std::lock_guard lock {mLayerCallsMutex};

        return mInstallLayerCalls;
    }

    MOCK_METHOD(Error, DownloadBlob, (const String& digest, const String& path), (override));

private:
    std::mutex             mLayerCallsMutex;
    std::vector<LayerCall> mInstallLayerCalls;
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
            config, mStorage, mSpaceAllocator, mFileInfoProvider, mOCISpec, mFSPlatform, mAllocator);
    }

    void AddStoredItem(
        const String& itemID, const String& version, ItemStateEnum state, const Error& error = ErrorEnum::eNone)
    {
        auto err = mStorage.AddDeployableItem(CreateDeployableItem(itemID, version, state, error));
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    Error InstallTestLayer(const oci::ContentDescriptor& descriptor = GetLayerDescriptor())
    {
        TestImageManager::InstallItemInfo itemInfo;

        itemInfo.mItemID     = cTestItemID;
        itemInfo.mType       = UpdateItemTypeEnum::eService;
        itemInfo.mVersion    = cTestVersion;
        itemInfo.mRootDigest = cTestDigest;

        AddStoredItem(cTestItemID, cTestVersion, ItemStateEnum::eInstalling);

        return mImageManager.InstallLayer(itemInfo, descriptor, cOtherDigest);
    }

    void ExpectAllocateSpace(bool accept)
    {
        EXPECT_CALL(mSpaceAllocator, AllocateSpace(cTestBlobSize))
            .WillOnce(Invoke([this, accept](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
                auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);

                EXPECT_CALL(*space, Accept()).Times(accept ? 1 : 0).WillRepeatedly(Return(ErrorEnum::eNone));
                EXPECT_CALL(*space, Release()).Times(accept ? 0 : 1).WillRepeatedly(Return(ErrorEnum::eNone));

                return {Move(space), ErrorEnum::eNone};
            }));
    }

    // mAllocator must be declared before any member that allocates from it, so it is destroyed after them, since
    // members are destroyed in reverse declaration order.
    HeapAllocator                                mAllocator;
    TestStorage                                  mStorage;
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

TEST_F(CommonImageManagerTest, ValidateBlobMalformedDigest)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(_, _, _)).Times(0);

    CreateFile(cTestBlobPath, cTestBlobContent);

    for (const auto& digest : {"", "sha256", "sha256:", ":abc", "sha256:xyz", "sha256:ab:cd"}) {
        auto err = mImageManager.ValidateBlob(digest, cTestBlobPath);
        EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << digest << ": " << tests::utils::ErrorToStr(err);
    }
}

TEST_F(CommonImageManagerTest, ValidateBlobShortHash)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    CreateFile(cTestBlobPath, cTestBlobContent);

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = mImageManager.ValidateBlob("sha256:abc", cTestBlobPath);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, ValidateBlobUppercaseHash)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(_, _, _)).Times(0);

    CreateFile(cTestBlobPath, cTestBlobContent);

    StaticString<oci::cDigestLen> hash {cTestHash};
    StaticString<oci::cDigestLen> digest {"sha256:"};

    digest.Append(hash.ToUpper());

    auto err = mImageManager.ValidateBlob(digest, cTestBlobPath);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    StaticString<cFilePathLen> path;

    err = mImageManager.GetBlobPath(digest, path);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, ValidateBlobUnsupportedAlgorithm)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(_, _, _)).Times(0);

    CreateFile(cTestBlobPath, cTestBlobContent);

    StaticString<oci::cDigestLen> digest {"sha512:"};

    digest.Append(cTestHash);

    auto err = mImageManager.ValidateBlob(digest, cTestBlobPath);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotSupported)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, ValidateBlobNotFound)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _)).WillOnce(Return(ErrorEnum::eNotFound));

    auto err = mImageManager.ValidateBlob(cTestDigest, cTestBlobPath);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, ValidateBlobInvalidChecksum)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    CreateFile(cTestBlobPath, cTestBlobContent);

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = mImageManager.ValidateBlob(cOtherDigest, cTestBlobPath);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(FileExists(cTestBlobPath));
}

TEST_F(CommonImageManagerTest, ValidateBlobSucceeds)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    CreateFile(cTestBlobPath, cTestBlobContent);

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = mImageManager.ValidateBlob(cTestDigest, cTestBlobPath);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, ValidateBlobFileInfoError)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    CreateFile(cTestBlobPath, cTestBlobContent);

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _)).WillOnce(Return(ErrorEnum::eFailed));

    auto err = mImageManager.ValidateBlob(cTestDigest, cTestBlobPath);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, RemoveLayer)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    EXPECT_CALL(mSpaceAllocator, FreeSpace(_)).Times(0);

    CreateFile(cTestBlobPath, cTestBlobContent);

    auto [size, err] = mImageManager.RemoveLayer(cTestDigest, cTestLayerType, cOtherDigest, cTestBlobPath);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(size, cTestBlobSize);

    EXPECT_FALSE(FileExists(cTestBlobPath));
}

TEST_F(CommonImageManagerTest, RemoveLayerNotExist)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    auto [size, err] = mImageManager.RemoveLayer(cTestDigest, cTestLayerType, cOtherDigest, cTestBlobPath);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(size, 0);
}

TEST_F(CommonImageManagerTest, InstallLayer)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    ExpectAllocateSpace(true);

    EXPECT_CALL(mImageManager, DownloadBlob(String(cTestDigest), String(cTestBlobPath)))
        .WillOnce(Invoke(
            [](const String&, const String& path) { return fs::WriteStringToFile(path, cTestBlobContent, 0600); }));
    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _))
        .WillOnce(Return(ErrorEnum::eNotFound))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = InstallTestLayer();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(FileExists(cTestBlobPath));

    ItemBlobInfo blob;

    blob.mDigest    = cTestDigest;
    blob.mMediaType = cTestLayerType;
    blob.mDiffID    = cOtherDigest;
    blob.mIsLayer   = true;

    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion), std::vector<ItemBlobInfo>({blob}));
}

TEST_F(CommonImageManagerTest, InstallLayerDownloadFails)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    ExpectAllocateSpace(false);

    EXPECT_CALL(mImageManager, DownloadBlob(String(cTestDigest), String(cTestBlobPath)))
        .WillOnce(Invoke([](const String&, const String& path) {
            EXPECT_TRUE(fs::WriteStringToFile(path, "partial", 0600).IsNone());

            return Error(ErrorEnum::eFailed);
        }));
    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _)).WillOnce(Return(ErrorEnum::eNotFound));

    auto err = InstallTestLayer();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(cTestBlobPath));
}

TEST_F(CommonImageManagerTest, InstallLayerValidationFails)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    ExpectAllocateSpace(false);

    EXPECT_CALL(mImageManager, DownloadBlob(String(cTestDigest), String(cTestBlobPath)))
        .WillOnce(Invoke(
            [](const String&, const String& path) { return fs::WriteStringToFile(path, cTestBlobContent, 0600); }));
    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _))
        .WillOnce(Return(ErrorEnum::eNotFound))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cOtherHash)), Return(ErrorEnum::eNone)));

    auto err = InstallTestLayer();
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(cTestBlobPath));
}

TEST_F(CommonImageManagerTest, InstallLayerAcceptFails)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cTestBlobSize))
        .WillOnce(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            auto space = MakeUnique<spaceallocator::SpaceMock>(&mAllocator);

            EXPECT_CALL(*space, Accept()).WillOnce(Return(ErrorEnum::eFailed));
            EXPECT_CALL(*space, Release()).Times(0);

            return {Move(space), ErrorEnum::eNone};
        }));
    EXPECT_CALL(mImageManager, DownloadBlob(String(cTestDigest), String(cTestBlobPath)))
        .WillOnce(Invoke(
            [](const String&, const String& path) { return fs::WriteStringToFile(path, cTestBlobContent, 0600); }));
    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _))
        .WillOnce(Return(ErrorEnum::eNotFound))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = InstallTestLayer();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(FileExists(cTestBlobPath));
}

TEST_F(CommonImageManagerTest, InstallLayerAllocateSpaceFails)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cTestBlobSize))
        .WillOnce(Invoke([](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            return {nullptr, ErrorEnum::eNoMemory};
        }));
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);
    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _)).WillOnce(Return(ErrorEnum::eNotFound));

    auto err = InstallTestLayer();
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(cTestBlobPath));
}

TEST_F(CommonImageManagerTest, InstallLayerZeroSize)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    auto descriptor = GetLayerDescriptor();

    descriptor.mSize = 0;

    EXPECT_CALL(mSpaceAllocator, AllocateSpace(_)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(String(cTestDigest), String(cTestBlobPath)))
        .WillOnce(Invoke(
            [](const String&, const String& path) { return fs::WriteStringToFile(path, cTestBlobContent, 0600); }));
    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _))
        .WillOnce(Return(ErrorEnum::eNotFound))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = InstallTestLayer(descriptor);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(FileExists(cTestBlobPath));
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

    static ItemBlobInfo MakeBlobInfo(
        const TestBlob& blob, const char* mediaType, bool isLayer = false, const std::string& diffID = "")
    {
        ItemBlobInfo info;

        info.mDigest    = blob.mDigest.c_str();
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

TEST_F(CommonImageManagerInstallTest, UnsubscribeFromCallbackDuringNotification)
{
    const auto manifest = AddManifest(1, 1, 1);

    StrictMock<ItemStatusListenerMock> listenerA;
    StrictMock<ItemStatusListenerMock> listenerB;

    ASSERT_TRUE(mImageManager.SubscribeListener(listenerA).IsNone());
    ASSERT_TRUE(mImageManager.SubscribeListener(listenerB).IsNone());

    // Listener A unsubscribes itself during the first notification. Listener B must still be called in the same
    // notification, i.e. the notification iterates over a copy of the listeners, and in the following ones.
    EXPECT_CALL(listenerA, OnItemStatusChanged(_)).WillOnce(Invoke([this, &listenerA](const UpdateItemStatus&) {
        EXPECT_TRUE(mImageManager.UnsubscribeListener(listenerA).IsNone());
    }));
    EXPECT_CALL(listenerB, OnItemStatusChanged(_)).Times(3);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerInstallTest, InstallManifestRoot)
{
    const auto manifest = AddManifest(1, 2, 2, true);
    const auto itemInfo = MakeInstallInfo(manifest.mManifest);

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(5);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(0)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cInstallBlobSize)).Times(4);
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
            MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest),
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

    EXPECT_EQ(mAcceptCount.load(), 4);
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
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(0)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(cInstallBlobSize)).Times(3);

    auto err = mImageManager.Install(MakeInstallInfo(index, true));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>(
            {ItemStateEnum::eInstalling, ItemStateEnum::eDownloading, ItemStateEnum::eInstalled}));

    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({
            MakeBlobInfo(index, oci::cMediaTypeImageIndex),
            MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest),
            MakeBlobInfo(manifest.mImageConfig, cImageConfigType),
            MakeBlobInfo(manifest.mLayers[0], cTestLayerType, true, manifest.mDiffIDs[0].mDigest),
        }));

    EXPECT_TRUE(FileExists(index.mPath.c_str()));
    EXPECT_TRUE(FileExists(manifest.mManifest.mPath.c_str()));
    EXPECT_TRUE(FileExists(manifest.mImageConfig.mPath.c_str()));
    EXPECT_TRUE(FileExists(manifest.mLayers[0].mPath.c_str()));
    EXPECT_EQ(mAcceptCount.load(), 3);

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
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mLayers[1].mDigest.c_str()), _)).Times(1);
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
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mImageConfig.mDigest.c_str()), _)).Times(1);
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
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mLayers[1].mDigest.c_str()), _)).Times(1);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    using LayerCall = TestImageManager::LayerCall;

    EXPECT_EQ(mImageManager.GetInstallLayerCalls(),
        std::vector<LayerCall>({{manifest.mLayers[0].mDigest, manifest.mDiffIDs[0].mDigest},
            {manifest.mLayers[1].mDigest, manifest.mDiffIDs[1].mDigest}}));
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

    EXPECT_EQ(mImageManager.GetInstallLayerCalls(), expectedCalls);

    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({
            MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest),
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

    EXPECT_TRUE(mImageManager.GetInstallLayerCalls().empty());

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

    EXPECT_EQ(mImageManager.GetInstallLayerCalls(),
        std::vector<TestImageManager::LayerCall>({{cMalformedDigest, manifest.mDiffIDs[0].mDigest}}));

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0].mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(statuses[1].mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(statuses[1].mError.Is(ErrorEnum::eInvalidArgument));

    // No blob reference is stored for the layer with the malformed digest.
    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({
            MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest),
            MakeBlobInfo(manifest.mImageConfig, cImageConfigType),
        }));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(item->mError.Is(ErrorEnum::eInvalidArgument));
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
        std::vector<ItemBlobInfo>({MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest)}));

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

TEST_F(CommonImageManagerInstallTest, InstallStorageFullNoOutdatedItem)
{
    const auto manifest = AddManifest(1, 1, 1);

    for (size_t i = 0; i < cMaxNumStoredItems; ++i) {
        StoreItem(("item" + std::to_string(i + 100)).c_str(), cTestVersion, cTestDigest, ItemStateEnum::eInstalled);
    }

    EXPECT_CALL(mSpaceAllocator, RemoveOutdatedItem()).WillOnce(Return(RetWithError<size_t>(0, ErrorEnum::eNotFound)));
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorage.GetItems().size(), cMaxNumStoredItems);
    EXPECT_FALSE(GetStoredItem(cTestItemID, cTestVersion).has_value());
}

TEST_F(CommonImageManagerInstallTest, InstallStorageFullRemovesOutdatedItem)
{
    const auto manifest = AddManifest(1, 1, 1);

    for (size_t i = 0; i < cMaxNumStoredItems; ++i) {
        StoreItem(("item" + std::to_string(i + 100)).c_str(), cTestVersion, cTestDigest, ItemStateEnum::eRemoved);
    }

    EXPECT_CALL(mSpaceAllocator, RemoveOutdatedItem()).WillOnce(Invoke([this]() -> RetWithError<size_t> {
        EXPECT_TRUE(mStorage.RemoveDeployableItem("item100", cTestVersion).IsNone());

        return {0, ErrorEnum::eNone};
    }));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorage.GetItems().size(), cMaxNumStoredItems);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerInstallTest, InstallStorageFullRemoveOutdatedItemFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    for (size_t i = 0; i < cMaxNumStoredItems; ++i) {
        StoreItem(("item" + std::to_string(i + 100)).c_str(), cTestVersion, cTestDigest, ItemStateEnum::eRemoved);
    }

    EXPECT_CALL(mSpaceAllocator, RemoveOutdatedItem()).WillOnce(Return(RetWithError<size_t>(0, ErrorEnum::eFailed)));
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(GetStoredItem(cTestItemID, cTestVersion).has_value());
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

TEST_F(CommonImageManagerInstallTest, InstallRemovedItemRestoreFails)
{
    const auto manifest = AddManifest(1, 1, 1);

    CreateValidBlobs(manifest);
    StoreItem(cTestItemID, cTestVersion, manifest.mManifest.mDigest.c_str(), ItemStateEnum::eRemoved);

    EXPECT_CALL(mSpaceAllocator, RestoreOutdatedItem(String(cTestItemID), String(cTestVersion)))
        .WillOnce(Return(ErrorEnum::eFailed));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eInstalling, ItemStateEnum::eFailed}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
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

TEST_F(CommonImageManagerInstallTest, InstallDownloadFails)
{
    const auto manifest = AddManifest(1, 2, 2);

    CreateValidBlobs(manifest);
    ASSERT_TRUE(fs::RemoveAll(manifest.mLayers[0].mPath.c_str()).IsNone());
    ASSERT_TRUE(fs::RemoveAll(manifest.mLayers[1].mPath.c_str()).IsNone());

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).WillRepeatedly(Invoke(DownloadTestBlob));
    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mLayers[1].mDigest.c_str()), _))
        .WillOnce(Invoke([](const String&, const String& path) {
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

    EXPECT_CALL(mImageManager, DownloadBlob(String(manifest.mManifest.mDigest.c_str()), _))
        .WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mSpaceAllocator, AllocateSpace(_)).Times(0);

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(manifest.mManifest.mPath.c_str()));
    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion),
        std::vector<ItemBlobInfo>({MakeBlobInfo(manifest.mManifest, oci::cMediaTypeImageManifest)}));

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

    mStorage.mFailUpdateStates = {ItemStateEnum::eInstalled};

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
    mStorage.mFailUpdateStates = {ItemStateEnum::eInstalled, ItemStateEnum::eFailed};

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

    // While the final eFailed state is notified, the stored eInstalling state is reported, not the last notified
    // eDownloading state.
    EXPECT_EQ(reportedOnFailed, ItemStateEnum::eInstalling);

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
        .WillOnce(Invoke([&](const String& digest, const String& path) {
            downloadStarted.set_value();
            downloadReleasedFuture.wait();

            return DownloadTestBlob(digest, path);
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

TEST_F(CommonImageManagerInstallTest, InstallSameVersionConcurrently)
{
    const auto manifest = AddManifest(1, 1, 1);

    std::promise<void> downloadStarted;
    std::promise<void> downloadReleased;
    auto               downloadReleasedFuture = downloadReleased.get_future();
    std::atomic<int>   downloadCount {0};

    EXPECT_CALL(mImageManager, DownloadBlob(_, _)).WillRepeatedly(Invoke([&](const String& digest, const String& path) {
        if (downloadCount++ == 0) {
            downloadStarted.set_value();
            downloadReleasedFuture.wait();
        }

        return DownloadTestBlob(digest, path);
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

    EXPECT_CALL(mImageManager, DownloadBlob(String(sharedLayer.mDigest.c_str()), _))
        .WillOnce(Invoke([&](const String& digest, const String& path) {
            sharedDownloadCount++;
            downloadStarted.set_value();
            downloadReleasedFuture.wait();

            return DownloadTestBlob(digest, path);
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
    EXPECT_EQ(countSharedLayerCalls(mImageManager.GetInstallLayerCalls()), 2);
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

class CommonImageManagerRemoveTest : public CommonImageManagerInstallTest {
protected:
    TestManifest InstallTestItem()
    {
        auto manifest = AddManifest(1, 1, 1);

        CreateValidBlobs(manifest);

        auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        ClearNotifiedStatuses();

        return manifest;
    }

    void StoreItemWithBlob(ItemStateEnum state, UpdateItemTypeEnum type, const Error& error = ErrorEnum::eNone)
    {
        auto item = CreateDeployableItem(cTestItemID, cTestVersion, state, error);

        item.mType = type;

        auto err = mStorage.AddDeployableItem(item);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        err = mStorage.AddItemBlob(cTestItemID, cTestVersion, MakeBlobInfo(MakeTestBlob(1), cTestLayerType, true));
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    void ClearNotifiedStatuses()
    {
        std::lock_guard lock {mStatusesMutex};

        mStatuses.clear();
    }

    static UpdateItemStatus MakeStatus(ItemStateEnum state, UpdateItemTypeEnum type = UpdateItemTypeEnum::eService,
        const Error& error = ErrorEnum::eNone)
    {
        auto status = CreateItemStatus(cTestItemID, cTestVersion, state);

        status.mType  = type;
        status.mError = error;

        return status;
    }

    void ExpectRemoved(UpdateItemTypeEnum type, const std::vector<ItemBlobInfo>& blobs)
    {
        const auto startTime = Time::Now();
        Time       outdatedTimestamp;

        {
            InSequence seq;

            EXPECT_CALL(mListener, OnItemStatusChanged(MakeStatus(ItemStateEnum::eRemoving, type)));
            EXPECT_CALL(mListener, OnItemStatusChanged(MakeStatus(ItemStateEnum::eRemoved, type)));
            EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), _))
                .WillOnce(Invoke([&outdatedTimestamp](const String&, const String&, const Time& timestamp) {
                    outdatedTimestamp = timestamp;

                    return ErrorEnum::eNone;
                }));
        }

        auto err = mImageManager.Remove(cTestItemID, cTestVersion);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eRemoving, ItemStateEnum::eRemoved}));

        auto item = GetStoredItem(cTestItemID, cTestVersion);
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
        EXPECT_EQ(item->mType, type);
        EXPECT_TRUE(item->mError.IsNone());
        EXPECT_FALSE(item->mTimestamp < startTime);
        EXPECT_EQ(item->mTimestamp, outdatedTimestamp);

        EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion), blobs);
        EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eRemoved);
    }
};

TEST_F(CommonImageManagerRemoveTest, RemoveInstalledItem)
{
    const auto manifest = InstallTestItem();
    const auto blobs    = mStorage.GetBlobs(cTestItemID, cTestVersion);

    ASSERT_EQ(blobs.size(), 3);

    ExpectRemoved(UpdateItemTypeEnum::eService, blobs);

    for (const auto& layer : manifest.mLayers) {
        EXPECT_TRUE(FileExists(layer.mPath.c_str()));
    }
}

TEST_F(CommonImageManagerRemoveTest, RemoveFailedItem)
{
    StoreItemWithBlob(ItemStateEnum::eFailed, UpdateItemTypeEnum::eComponent, Error(ErrorEnum::eFailed, "failed"));

    ExpectRemoved(UpdateItemTypeEnum::eComponent, mStorage.GetBlobs(cTestItemID, cTestVersion));
}

TEST_F(CommonImageManagerRemoveTest, RemoveInstallingItem)
{
    StoreItemWithBlob(ItemStateEnum::eInstalling, UpdateItemTypeEnum::eService);

    ExpectRemoved(UpdateItemTypeEnum::eService, mStorage.GetBlobs(cTestItemID, cTestVersion));
}

TEST_F(CommonImageManagerRemoveTest, RemoveNotStoredItem)
{
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).Times(0);

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mStorage.GetItems().empty());
}

TEST_F(CommonImageManagerRemoveTest, RemoveRemovedItem)
{
    StoreItemWithBlob(ItemStateEnum::eRemoved, UpdateItemTypeEnum::eService);

    const auto before = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(before.has_value());

    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), before->mTimestamp))
        .WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
    EXPECT_EQ(item->mTimestamp, before->mTimestamp);
    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion).size(), 1);
}

TEST_F(CommonImageManagerRemoveTest, RemoveRemovedItemAfterAddOutdatedItemFailure)
{
    InstallTestItem();

    Time firstTimestamp;

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), _))
        .WillOnce(Invoke([&firstTimestamp](const String&, const String&, const Time& timestamp) {
            firstTimestamp = timestamp;

            return ErrorEnum::eNoMemory;
        }));

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eRemoving, ItemStateEnum::eRemoved}));

    const auto removed = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(removed->mState, ItemStateEnum::eRemoved);
    EXPECT_EQ(removed->mTimestamp, firstTimestamp);

    const auto blobs = mStorage.GetBlobs(cTestItemID, cTestVersion);

    Mock::VerifyAndClearExpectations(&mSpaceAllocator);
    Mock::VerifyAndClearExpectations(&mListener);
    ClearNotifiedStatuses();

    // The second call registers the item with the stored timestamp, without notifications.
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).Times(0);
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), removed->mTimestamp))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mImageManager.Remove(cTestItemID, cTestVersion);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(GetNotifiedStates().empty());

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
    EXPECT_EQ(item->mTimestamp, removed->mTimestamp);
    EXPECT_EQ(mStorage.GetBlobs(cTestItemID, cTestVersion), blobs);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eRemoved);
}

TEST_F(CommonImageManagerRemoveTest, RemoveStorageUpdateFails)
{
    InstallTestItem();

    const auto before = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(before.has_value());

    mStorage.mFailUpdateStates = {ItemStateEnum::eRemoved};

    std::optional<ItemStateEnum> reportedOnPrevState;

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).Times(0);
    EXPECT_CALL(mListener, OnItemStatusChanged(_)).WillRepeatedly(Invoke([&](const UpdateItemStatus& status) {
        {
            std::lock_guard lock {mStatusesMutex};

            mStatuses.push_back(status);
        }

        if (status.mState == ItemStateEnum::eInstalled) {
            reportedOnPrevState = GetReportedState(cTestItemID, cTestVersion);
        }
    }));

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0], MakeStatus(ItemStateEnum::eRemoving));
    EXPECT_EQ(statuses[1], MakeStatus(ItemStateEnum::eInstalled, UpdateItemTypeEnum::eService, ErrorEnum::eFailed));
    EXPECT_STREQ(statuses[1].mError.Message(), "update failed");

    EXPECT_EQ(reportedOnPrevState, ItemStateEnum::eInstalled);

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
    EXPECT_EQ(item->mTimestamp, before->mTimestamp);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eInstalled);
}

TEST_F(CommonImageManagerRemoveTest, RemoveInstallingItemStorageUpdateFails)
{
    StoreItemWithBlob(ItemStateEnum::eInstalling, UpdateItemTypeEnum::eService);

    mStorage.mFailUpdateStates = {ItemStateEnum::eRemoved};

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(_, _, _)).Times(0);

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    auto statuses = GetNotifiedStatuses();
    ASSERT_EQ(statuses.size(), 2);
    EXPECT_EQ(statuses[0], MakeStatus(ItemStateEnum::eRemoving));
    EXPECT_EQ(statuses[1], MakeStatus(ItemStateEnum::eFailed, UpdateItemTypeEnum::eService, ErrorEnum::eFailed));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalling);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eInstalling);
}

TEST_F(CommonImageManagerRemoveTest, RemoveAddOutdatedItemFails)
{
    InstallTestItem();

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), _))
        .WillOnce(Return(ErrorEnum::eNoMemory));

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eRemoving, ItemStateEnum::eRemoved}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eRemoved);
    EXPECT_EQ(GetReportedState(cTestItemID, cTestVersion), ItemStateEnum::eRemoved);
}

TEST_F(CommonImageManagerRemoveTest, GetItemsStatusesDuringRemove)
{
    InstallTestItem();

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

TEST_F(CommonImageManagerRemoveTest, AddOutdatedItemCalledAfterItemReleased)
{
    InstallTestItem();

    std::optional<ItemStateEnum> reportedOnAdd;
    std::future<Error>           nestedRemove;
    bool                         nestedRemoveDone = false;

    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), _))
        .WillOnce(Invoke([&](const String&, const String&, const Time&) {
            reportedOnAdd = GetReportedState(cTestItemID, cTestVersion);

            // RemoveItem of the same version waits while the item is in progress, so it completes only if the item
            // is already released.
            nestedRemove
                = std::async(std::launch::async, [this]() { return mImageManager.Remove(cTestItemID, cTestVersion); });

            nestedRemoveDone = nestedRemove.wait_for(std::chrono::seconds(10)) == std::future_status::ready;

            return ErrorEnum::eNone;
        }))
        // The nested RemoveItem of the removed item adds it as outdated again.
        .WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.Remove(cTestItemID, cTestVersion);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(reportedOnAdd, ItemStateEnum::eRemoved);
    EXPECT_TRUE(nestedRemoveDone);
    ASSERT_TRUE(nestedRemove.valid());

    auto nestedErr = nestedRemove.get();
    EXPECT_TRUE(nestedErr.IsNone()) << tests::utils::ErrorToStr(nestedErr);

    EXPECT_EQ(GetNotifiedStates(), std::vector<ItemStateEnum>({ItemStateEnum::eRemoving, ItemStateEnum::eRemoved}));
}

TEST_F(CommonImageManagerRemoveTest, RemoveWaitsForInstallOfSameVersion)
{
    const auto manifest = AddManifest(1, 1, 1);

    std::promise<void> downloadStarted;
    std::promise<void> downloadReleased;
    auto               downloadReleasedFuture = downloadReleased.get_future();

    EXPECT_CALL(mImageManager, DownloadBlob(_, _))
        .WillOnce(Invoke([&](const String& digest, const String& path) {
            downloadStarted.set_value();
            downloadReleasedFuture.wait();

            return DownloadTestBlob(digest, path);
        }))
        .WillRepeatedly(Invoke(DownloadTestBlob));
    EXPECT_CALL(mSpaceAllocator, AddOutdatedItem(String(cTestItemID), String(cTestVersion), _)).Times(1);

    Error installErr, removeErr;

    std::thread installThread([&]() { installErr = mImageManager.Install(MakeInstallInfo(manifest.mManifest)); });

    downloadStarted.get_future().wait();

    std::thread removeThread([&]() { removeErr = mImageManager.Remove(cTestItemID, cTestVersion); });

    // Gives the remove time to reach LockItem: without waiting there, it would notify eRemoving and fail the checks
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

TEST_F(CommonImageManagerRemoveTest, InstallRemovedItem)
{
    const auto manifest = InstallTestItem();

    ASSERT_TRUE(mImageManager.Remove(cTestItemID, cTestVersion).IsNone());

    EXPECT_CALL(mSpaceAllocator, RestoreOutdatedItem(String(cTestItemID), String(cTestVersion)))
        .WillOnce(Return(ErrorEnum::eNone));

    auto err = mImageManager.Install(MakeInstallInfo(manifest.mManifest));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>({ItemStateEnum::eRemoving, ItemStateEnum::eRemoved, ItemStateEnum::eInstalling,
            ItemStateEnum::eInstalled}));

    auto item = GetStoredItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eInstalled);
}

} // namespace aos::common::imagemanager
