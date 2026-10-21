/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

/***********************************************************************************************************************
 * Test image manager
 **********************************************************************************************************************/

class TestImageManager : public ImageManager {
public:
    using ImageManager::Init;
    using ImageManager::InstallItemInfo;
    using ImageManager::InstallLayer;
    using ImageManager::RemoveLayer;
    using ImageManager::ValidateBlob;
    using ImageManager::ValidateLayer;

    Error Install(const InstallItemInfo& itemInfo) { return InstallItem(itemInfo); }
    Error Remove(const String& itemID, const String& version) { return RemoveItem(itemID, version); }

    MOCK_METHOD(Error, DownloadBlob, (const String& digest, const String& path), (override));
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

TEST_F(CommonImageManagerTest, ValidateLayerSucceeds)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    CreateFile(cTestBlobPath, cTestBlobContent);

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _))
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = mImageManager.ValidateLayer(GetLayerDescriptor(), cOtherDigest, cTestBlobPath);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CommonImageManagerTest, ValidateLayerNotFound)
{
    ASSERT_TRUE(InitImageManager().IsNone());

    EXPECT_CALL(mFileInfoProvider, GetFileInfo(String(cTestBlobPath), _, _)).WillOnce(Return(ErrorEnum::eNotFound));

    auto err = mImageManager.ValidateLayer(GetLayerDescriptor(), cOtherDigest, cTestBlobPath);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
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
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = mImageManager.InstallLayer(GetLayerDescriptor(), cOtherDigest, cTestBlobPath);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(FileExists(cTestBlobPath));
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
    EXPECT_CALL(mFileInfoProvider, GetFileInfo(_, _, _)).Times(0);

    auto err = mImageManager.InstallLayer(GetLayerDescriptor(), cOtherDigest, cTestBlobPath);
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
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cOtherHash)), Return(ErrorEnum::eNone)));

    auto err = mImageManager.InstallLayer(GetLayerDescriptor(), cOtherDigest, cTestBlobPath);
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
        .WillOnce(DoAll(SetArgReferee<1>(GetFileInfoByHash(cTestHash)), Return(ErrorEnum::eNone)));

    auto err = mImageManager.InstallLayer(GetLayerDescriptor(), cOtherDigest, cTestBlobPath);
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

    auto err = mImageManager.InstallLayer(GetLayerDescriptor(), cOtherDigest, cTestBlobPath);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(cTestBlobPath));
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

} // namespace aos::common::imagemanager
