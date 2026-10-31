/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <core/cm/tests/mocks/fileservermock.hpp>
#include <core/common/crypto/cryptoprovider.hpp>
#include <core/common/imagemanager/tests/stubs/storagestub.hpp>
#include <core/common/tests/mocks/blobinfoprovidermock.hpp>
#include <core/common/tests/mocks/cryptomock.hpp>
#include <core/common/tests/mocks/downloadermock.hpp>
#include <core/common/tests/mocks/fileinfoprovidermock.hpp>
#include <core/common/tests/mocks/fsmock.hpp>
#include <core/common/tests/mocks/itemstatuslistenermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>

#include <core/cm/imagemanager/imagemanager.hpp>

using namespace testing;

namespace aos::cm::imagemanager {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestDir          = "/tmp/cm_imagemanager_test";
constexpr auto cTestImagePath    = "/tmp/cm_imagemanager_test/images";
constexpr auto cTestDownloadPath = "/tmp/cm_imagemanager_test/downloads";
constexpr auto cTestItemID       = "item1";
constexpr auto cTestItemID2      = "item2";
constexpr auto cTestVersion      = "1.0.0";
constexpr auto cImageConfigType  = "application/vnd.oci.image.config.v1+json";
constexpr auto cTestLayerType    = "application/vnd.oci.image.layer.v1.tar+gzip";
constexpr auto cTestDiffID       = "sha256:00000000000000000000000000000000000000000000000000000000000000ff";
constexpr auto cTestDiffID2      = "sha256:00000000000000000000000000000000000000000000000000000000000000fe";
constexpr auto cLeftoverHash1    = "1111111111111111111111111111111111111111111111111111111111111111";
constexpr auto cLeftoverHash2    = "2222222222222222222222222222222222222222222222222222222222222222";
constexpr auto cPastTime         = 1000000000;
constexpr auto cItemCacheTTL     = 10 * Time::cSeconds;
constexpr auto cWaitTimeout      = std::chrono::seconds(5);
constexpr auto cDownloadTimeout  = 5 * Time::cSeconds;

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

std::string ToHex(const std::vector<uint8_t>& data)
{
    std::string hex;

    for (const auto byte : data) {
        char buf[3];

        std::snprintf(buf, sizeof(buf), "%02x", byte);
        hex += buf;
    }

    return hex;
}

std::string ReadFile(const std::string& path)
{
    std::ifstream     file(path, std::ios::binary);
    std::stringstream content;

    content << file.rdbuf();

    return content.str();
}

void WriteFile(const std::string& path, const std::string& content, bool append = false)
{
    auto err = fs::MakeDirAll(fs::Dir(path.c_str()));
    ASSERT_TRUE(err.IsNone()) << "Failed to create dir: " << tests::utils::ErrorToStr(err);

    std::ofstream file(path, std::ios::binary | (append ? std::ios::app : std::ios::trunc));

    file << content;
}

bool FileExists(const std::string& path)
{
    auto [exists, err] = fs::FileExist(path.c_str());
    EXPECT_TRUE(err.IsNone()) << "Failed to check file: " << tests::utils::ErrorToStr(err);

    return exists;
}

void SetModifyTime(const std::string& path, time_t sec)
{
    auto err = fs::SetFileMTime(path.c_str(), Time::Unix(sec));
    ASSERT_TRUE(err.IsNone()) << "Failed to set modify time: " << tests::utils::ErrorToStr(err);
}

Time GetModifyTime(const std::string& path)
{
    auto [modifyTime, err] = fs::GetFileMTime(path.c_str());
    EXPECT_TRUE(err.IsNone()) << "Failed to get modify time: " << tests::utils::ErrorToStr(err);

    return modifyTime;
}

bool IsNow(const Time& time)
{
    return std::llabs(Time::Now().Sub(time).Nanoseconds()) <= (5 * Time::cSeconds).Nanoseconds();
}

/**
 * Test blob.
 */
struct TestBlob {
    std::string mContent;
    std::string mDigest;
    std::string mPath;
    std::string mDownloadPath;
};

/**
 * Space event.
 */
struct SpaceEvent {
    size_t      mAllocatedSize {};
    std::string mName;
    size_t      mArg {};

    bool operator==(const SpaceEvent& rhs) const
    {
        return mAllocatedSize == rhs.mAllocatedSize && mName == rhs.mName && mArg == rhs.mArg;
    }
};

std::ostream& operator<<(std::ostream& os, const SpaceEvent& event)
{
    return os << "{" << event.mAllocatedSize << ", " << event.mName << ", " << event.mArg << "}";
}

/**
 * Outdated download item.
 */
struct OutdatedItem {
    std::string mDigest;
    Time        mTimestamp;
    size_t      mSpaceEventIndex {};
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CMImageManagerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        auto err = fs::RemoveAll(cTestDir);
        ASSERT_TRUE(err.IsNone()) << "Failed to remove test dir: " << tests::utils::ErrorToStr(err);

        ASSERT_TRUE(mCryptoProvider.Init(mAllocator).IsNone());
        ASSERT_TRUE(mFileInfoProviderImpl.Init(mAllocator, mCryptoProvider).IsNone());

        mLayer    = AddBlob("layer blob content", {"https://example.com/layer1", "https://example.com/layer2"});
        mConfig   = AddBlob("image config blob content");
        mManifest = AddBlob("image manifest blob content: manifest");
        mIndex    = AddBlob("image index blob content: index of manifests");

        SetUpOCISpec();

        ON_CALL(mFileInfoProvider, GetFileInfo(_, _, _))
            .WillByDefault(Invoke([this](const String& path, fs::FileInfo& info, const crypto::Hash& hashAlg) {
                return mFileInfoProviderImpl.GetFileInfo(path, info, hashAlg);
            }));
        ON_CALL(mBlobInfoProvider, GetBlobInfo(_, _))
            .WillByDefault(Invoke([this](const String& digest, BlobInfo& blobInfo) -> Error {
                std::lock_guard lock {mMutex};

                auto it = mBlobInfos.find(digest.CStr());
                if (it == mBlobInfos.end()) {
                    return ErrorEnum::eNotFound;
                }

                blobInfo = *it->second;

                return ErrorEnum::eNone;
            }));
        ON_CALL(mDownloader, Download(_, _, _, _))
            .WillByDefault(Invoke([this](CancelContext&, const String& digest, const Array<StaticString<cURLLen>>&,
                                      const String& path) { return DownloadContent(digest, path); }));
        ON_CALL(mCryptoHelper, Decrypt(_, _, _))
            .WillByDefault(
                Invoke([this](const String& encryptedPath, const String& decryptedPath, const crypto::DecryptInfo&) {
                    return fs::CopyFile(mAllocator, encryptedPath, decryptedPath);
                }));
        ON_CALL(mSpaceAllocator, AllocateSpace(_))
            .WillByDefault(Invoke([this](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
                auto space = MakeUnique<NiceMock<spaceallocator::SpaceMock>>(&mAllocator);

                return {Move(space), ErrorEnum::eNone};
            }));
        ON_CALL(mDownloadSpaceAllocator, AllocateSpace(_))
            .WillByDefault(Invoke([this](size_t size) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
                return {CreateDownloadSpace(size), ErrorEnum::eNone};
            }));
        ON_CALL(mDownloadSpaceAllocator, AddOutdatedItem(_, _, _))
            .WillByDefault(Invoke([this](const String& id, const String&, const Time& timestamp) {
                std::lock_guard lock {mMutex};

                mOutdatedItems.push_back({id.CStr(), timestamp, mSpaceEvents.size()});

                return ErrorEnum::eNone;
            }));
        ON_CALL(mListener, OnItemStatusChanged(_)).WillByDefault(Invoke([this](const UpdateItemStatus& status) {
            std::lock_guard lock {mMutex};

            mStates.push_back(status.mState.GetValue());
        }));

        EXPECT_CALL(mDownloadSpaceAllocator, RestoreOutdatedItem(_, String(""))).Times(AnyNumber());

        Config config;

        config.mImagePath            = cTestImagePath;
        config.mItemCacheTTL         = cItemCacheTTL;
        config.mRemoveOutdatedPeriod = 5 * Time::cSeconds;
        config.mDownloadPath         = cTestDownloadPath;

        err = mImageManager.Init(config, mBlobInfoProvider, mStorage, mSpaceAllocator, mDownloadSpaceAllocator,
            mDownloader, mCryptoHelper, mFileServer, mFileInfoProvider, mOCISpec, mFSPlatform, mAllocator);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        ASSERT_TRUE(mImageManager.SubscribeListener(mListener).IsNone());
    }

    void TearDown() override
    {
        auto err = mImageManager.Stop();
        EXPECT_TRUE(err.IsNone()) << "Failed to stop image manager: " << tests::utils::ErrorToStr(err);

        err = fs::RemoveAll(cTestDir);
        EXPECT_TRUE(err.IsNone()) << "Failed to remove test dir: " << tests::utils::ErrorToStr(err);
    }

    std::vector<uint8_t> CalculateHash(const std::string& content, const crypto::Hash& alg)
    {
        auto [hash, err] = mCryptoProvider.CreateHash(alg);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        if (!err.IsNone()) {
            return {};
        }

        err = hash->Update(Array<uint8_t>(reinterpret_cast<const uint8_t*>(content.data()), content.size()));
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        StaticArray<uint8_t, crypto::cSHA512Size> result;

        err = hash->Finalize(result);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        return std::vector<uint8_t>(result.begin(), result.end());
    }

    TestBlob AddBlob(const std::string& content, const std::vector<std::string>& urls = {"https://example.com/blob"})
    {
        const auto hash = ToHex(CalculateHash(content, crypto::HashEnum::eSHA256));

        TestBlob blob;

        blob.mContent      = content;
        blob.mDigest       = "sha256:" + hash;
        blob.mPath         = std::string(cTestImagePath) + "/blobs/sha256/" + hash;
        blob.mDownloadPath = std::string(cTestDownloadPath) + "/sha256/" + hash;

        auto blobInfo = std::make_shared<BlobInfo>();

        blobInfo->mDigest = blob.mDigest.c_str();
        blobInfo->mSize   = content.size();

        for (const auto& url : urls) {
            EXPECT_TRUE(blobInfo->mURLs.EmplaceBack(url.c_str()).IsNone());
        }

        std::lock_guard lock {mMutex};

        mBlobInfos[blob.mDigest]        = blobInfo;
        mDownloadContents[blob.mDigest] = content;

        return blob;
    }

    BlobInfo& GetBlobInfo(const TestBlob& blob)
    {
        std::lock_guard lock {mMutex};

        return *mBlobInfos[blob.mDigest];
    }

    void SetDownloadContent(const TestBlob& blob, const std::string& content)
    {
        std::lock_guard lock {mMutex};

        mDownloadContents[blob.mDigest] = content;
    }

    void SetUpOCISpec()
    {
        AddImage(mIndex, mManifest, mConfig, mLayer, cTestDiffID);

        ON_CALL(mOCISpec, LoadImageIndex(_, _))
            .WillByDefault(Invoke([this](const String& path, oci::ImageIndex& result) -> Error {
                std::lock_guard lock {mMutex};

                auto it = mImageIndexes.find(path.CStr());
                if (it == mImageIndexes.end()) {
                    return ErrorEnum::eNotFound;
                }

                result = *it->second;

                return ErrorEnum::eNone;
            }));
        ON_CALL(mOCISpec, LoadImageManifest(_, _))
            .WillByDefault(Invoke([this](const String& path, oci::ImageManifest& result) -> Error {
                std::lock_guard lock {mMutex};

                auto it = mImageManifests.find(path.CStr());
                if (it == mImageManifests.end()) {
                    return ErrorEnum::eNotFound;
                }

                result = *it->second;

                return ErrorEnum::eNone;
            }));
        ON_CALL(mOCISpec, LoadImageConfig(_, _))
            .WillByDefault(Invoke([this](const String& path, oci::ImageConfig& result) -> Error {
                std::lock_guard lock {mMutex};

                auto it = mImageConfigs.find(path.CStr());
                if (it == mImageConfigs.end()) {
                    return ErrorEnum::eNotFound;
                }

                result = *it->second;

                return ErrorEnum::eNone;
            }));
    }

    void AddImage(const TestBlob& indexBlob, const TestBlob& manifestBlob, const TestBlob& configBlob,
        const TestBlob& layerBlob, const char* diffID)
    {
        auto index = std::make_shared<oci::ImageIndex>();

        index->mMediaType = oci::cMediaTypeImageIndex;

        oci::IndexContentDescriptor manifestDescriptor;

        static_cast<oci::ContentDescriptor&>(manifestDescriptor) = oci::ContentDescriptor {
            oci::cMediaTypeImageManifest, manifestBlob.mDigest.c_str(), manifestBlob.mContent.size()};

        EXPECT_TRUE(index->mManifests.PushBack(manifestDescriptor).IsNone());

        auto manifest = std::make_shared<oci::ImageManifest>();

        manifest->mMediaType = oci::cMediaTypeImageManifest;
        manifest->mConfig
            = oci::ContentDescriptor {cImageConfigType, configBlob.mDigest.c_str(), configBlob.mContent.size()};

        EXPECT_TRUE(
            manifest->mLayers
                .PushBack(oci::ContentDescriptor {cTestLayerType, layerBlob.mDigest.c_str(), layerBlob.mContent.size()})
                .IsNone());

        auto imageConfig = std::make_shared<oci::ImageConfig>();

        EXPECT_TRUE(imageConfig->mRootfs.mDiffIDs.EmplaceBack(diffID).IsNone());

        std::lock_guard lock {mMutex};

        mImageIndexes[indexBlob.mPath]      = index;
        mImageManifests[manifestBlob.mPath] = manifest;
        mImageConfigs[configBlob.mPath]     = imageConfig;
    }

    Error DownloadContent(const String& digest, const String& path)
    {
        std::string content;

        {
            std::lock_guard lock {mMutex};

            content = mDownloadContents[digest.CStr()];
        }

        const auto existing = FileExists(path.CStr()) ? ReadFile(path.CStr()) : std::string();

        if (existing.size() > content.size()) {
            return ErrorEnum::eFailed;
        }

        WriteFile(path.CStr(), content.substr(existing.size()), true);

        return ErrorEnum::eNone;
    }

    UniquePtr<spaceallocator::SpaceItf> CreateDownloadSpace(size_t size)
    {
        auto space = MakeUnique<NiceMock<spaceallocator::SpaceMock>>(&mAllocator);

        ON_CALL(*space, Accept()).WillByDefault(Invoke([this, size]() {
            AddSpaceEvent({size, "accept", 0});

            return ErrorEnum::eNone;
        }));
        ON_CALL(*space, Release()).WillByDefault(Invoke([this, size]() {
            AddSpaceEvent({size, "release", 0});

            return ErrorEnum::eNone;
        }));
        ON_CALL(*space, Resize(_)).WillByDefault(Invoke([this, size](size_t newSize) {
            AddSpaceEvent({size, "resize", newSize});

            return ErrorEnum::eNone;
        }));

        return UniquePtr<spaceallocator::SpaceItf>(Move(space));
    }

    void AddSpaceEvent(const SpaceEvent& event)
    {
        std::lock_guard lock {mMutex};

        mSpaceEvents.push_back(event);
    }

    std::vector<SpaceEvent> GetSpaceEvents(size_t allocatedSize)
    {
        std::lock_guard lock {mMutex};

        std::vector<SpaceEvent> events;

        std::copy_if(mSpaceEvents.begin(), mSpaceEvents.end(), std::back_inserter(events),
            [allocatedSize](const SpaceEvent& event) { return event.mAllocatedSize == allocatedSize; });

        return events;
    }

    std::vector<ItemStateEnum> GetNotifiedStates()
    {
        std::lock_guard lock {mMutex};

        return mStates;
    }

    std::vector<const TestBlob*> GetBlobs() const { return {&mIndex, &mManifest, &mConfig, &mLayer}; }

    void ExpectDownload(const TestBlob& blob, std::optional<size_t> allocateSize = std::nullopt)
    {
        const auto& blobInfo = GetBlobInfo(blob);

        EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(allocateSize.value_or(blobInfo.mSize))).Times(1);
        EXPECT_CALL(mDownloader,
            Download(_, String(blob.mDigest.c_str()), Eq(blobInfo.mURLs), String(blob.mDownloadPath.c_str())))
            .Times(1);
    }

    void ExpectDownloads(const std::vector<const TestBlob*>& blobs)
    {
        for (const auto* blob : blobs) {
            ExpectDownload(*blob);
        }
    }

    void ExpectRegisterDownloads(const std::vector<const TestBlob*>& blobs)
    {
        for (const auto* blob : blobs) {
            EXPECT_CALL(mDownloadSpaceAllocator, AddOutdatedItem(String(blob->mDigest.c_str()), String(""), _))
                .Times(AtLeast(1));
        }
    }

    std::vector<OutdatedItem> GetOutdatedItems(const std::string& digest)
    {
        std::lock_guard lock {mMutex};

        std::vector<OutdatedItem> items;

        std::copy_if(mOutdatedItems.begin(), mOutdatedItems.end(), std::back_inserter(items),
            [&digest](const OutdatedItem& item) { return item.mDigest == digest; });

        return items;
    }

    std::optional<size_t> GetSpaceEventIndex(const SpaceEvent& event)
    {
        std::lock_guard lock {mMutex};

        auto it = std::find(mSpaceEvents.begin(), mSpaceEvents.end(), event);
        if (it == mSpaceEvents.end()) {
            return std::nullopt;
        }

        return static_cast<size_t>(std::distance(mSpaceEvents.begin(), it));
    }

    UpdateItemInfo MakeItemInfo(const char* itemID = cTestItemID, const TestBlob* index = nullptr) const
    {
        UpdateItemInfo itemInfo;

        itemInfo.mItemID      = itemID;
        itemInfo.mType        = UpdateItemTypeEnum::eService;
        itemInfo.mVersion     = cTestVersion;
        itemInfo.mIndexDigest = (index != nullptr ? *index : mIndex).mDigest.c_str();

        return itemInfo;
    }

    Error Install() { return mImageManager.InstallItem(MakeItemInfo()); }

    std::optional<common::imagemanager::DeployableItemInfo> GetStoredItem()
    {
        common::imagemanager::DeployableItemInfo item;

        if (!mStorage.GetDeployableItem(cTestItemID, cTestVersion, item).IsNone()) {
            return std::nullopt;
        }

        return item;
    }

    void ExpectItemState(ItemStateEnum state, std::optional<ErrorEnum> error = std::nullopt)
    {
        auto item = GetStoredItem();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(item->mState, state);

        if (error.has_value()) {
            EXPECT_TRUE(item->mError.Is(*error)) << tests::utils::ErrorToStr(item->mError);
        } else {
            EXPECT_TRUE(item->mError.IsNone()) << tests::utils::ErrorToStr(item->mError);
        }
    }

    static crypto::SignInfo MakeSignInfo()
    {
        crypto::SignInfo signInfo;

        signInfo.mChainName = "chain1";
        signInfo.mAlg       = "RSA/SHA256";

        for (uint8_t i = 0; i < 16; ++i) {
            EXPECT_TRUE(signInfo.mValue.PushBack(i).IsNone());
        }

        return signInfo;
    }

    static std::unique_ptr<crypto::CertificateInfoArray> MakeCerts(const std::vector<std::string>& fingerprints)
    {
        auto certs = std::make_unique<crypto::CertificateInfoArray>();

        for (const auto& fingerprint : fingerprints) {
            crypto::CertificateInfo cert;

            cert.mFingerprint = fingerprint.c_str();

            EXPECT_TRUE(cert.mCertificate.PushBack(static_cast<uint8_t>(fingerprint.size())).IsNone());
            EXPECT_TRUE(certs->PushBack(cert).IsNone());
        }

        return certs;
    }

    static std::unique_ptr<crypto::CertificateChainInfoArray> MakeChains(const std::vector<std::string>& names)
    {
        auto chains = std::make_unique<crypto::CertificateChainInfoArray>();

        for (const auto& name : names) {
            crypto::CertificateChainInfo chain;

            chain.mName = name.c_str();

            EXPECT_TRUE(chain.mFingerprints.EmplaceBack((name + "-fingerprint").c_str()).IsNone());
            EXPECT_TRUE(chains->PushBack(chain).IsNone());
        }

        return chains;
    }

    void ExpectValidateSigns(const crypto::SignInfo& signInfo, const crypto::CertificateChainInfoArray& chains,
        const crypto::CertificateInfoArray& certs, Error result = ErrorEnum::eNone)
    {
        EXPECT_CALL(mCryptoHelper,
            ValidateSigns(String(mLayer.mPath.c_str()), Eq(signInfo),
                Truly([&chains](const Array<crypto::CertificateChainInfo>& arg) { return arg == chains; }),
                Truly([&certs](const Array<crypto::CertificateInfo>& arg) { return arg == certs; })))
            .WillOnce(Return(result));
    }

    // mAllocator must be declared before any member that allocates from it, so it is destroyed after them, since
    // members are destroyed in reverse declaration order.
    HeapAllocator                                          mAllocator;
    crypto::DefaultCryptoProvider                          mCryptoProvider;
    fs::FileInfoProvider                                   mFileInfoProviderImpl;
    NiceMock<common::imagemanager::BlobInfoProviderMock>   mBlobInfoProvider;
    common::imagemanager::StorageStub                      mStorage;
    NiceMock<spaceallocator::SpaceAllocatorMock>           mSpaceAllocator;
    StrictMock<spaceallocator::SpaceAllocatorMock>         mDownloadSpaceAllocator;
    StrictMock<downloader::DownloaderMock>                 mDownloader;
    NiceMock<crypto::CryptoHelperMock>                     mCryptoHelper;
    NiceMock<fileserver::FileServerMock>                   mFileServer;
    NiceMock<fs::FileInfoProviderMock>                     mFileInfoProvider;
    NiceMock<oci::OCISpecMock>                             mOCISpec;
    NiceMock<FSPlatformMock>                               mFSPlatform;
    NiceMock<common::imagemanager::ItemStatusListenerMock> mListener;
    ImageManager                                           mImageManager;

    TestBlob mIndex;
    TestBlob mManifest;
    TestBlob mConfig;
    TestBlob mLayer;

    std::mutex                                       mMutex;
    std::map<std::string, std::shared_ptr<BlobInfo>> mBlobInfos;
    std::map<std::string, std::string>               mDownloadContents;
    std::vector<SpaceEvent>                          mSpaceEvents;
    std::vector<OutdatedItem>                        mOutdatedItems;
    std::vector<ItemStateEnum>                       mStates;

    std::map<std::string, std::shared_ptr<oci::ImageIndex>>    mImageIndexes;
    std::map<std::string, std::shared_ptr<oci::ImageManifest>> mImageManifests;
    std::map<std::string, std::shared_ptr<oci::ImageConfig>>   mImageConfigs;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CMImageManagerTest, SetCertsReplacesPrevious)
{
    const auto signInfo = MakeSignInfo();
    auto       certs    = MakeCerts({"cert1", "cert2"});
    auto       chains   = MakeChains({"chain1"});
    auto       newCerts = MakeCerts({"cert3"});
    auto       newChain = MakeChains({"chain2", "chain3"});

    GetBlobInfo(mLayer).mSignInfo.SetValue(signInfo);

    auto err = mImageManager.SetCerts(*certs, *chains);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.SetCerts(*newCerts, *newChain);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());
    ExpectValidateSigns(signInfo, *newChain, *newCerts);

    err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, SetCertsNoMemory)
{
    const auto signInfo = MakeSignInfo();
    auto       certs    = MakeCerts({"cert1"});
    auto       chains   = MakeChains({"chain1"});

    GetBlobInfo(mLayer).mSignInfo.SetValue(signInfo);

    auto err = mImageManager.SetCerts(*certs, *chains);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto tooManyCerts = std::make_unique<StaticArray<crypto::CertificateInfo, crypto::cMaxNumCertificates + 1>>();

    for (size_t i = 0; i < crypto::cMaxNumCertificates + 1; ++i) {
        ASSERT_TRUE(tooManyCerts->EmplaceBack().IsNone());
    }

    err = mImageManager.SetCerts(*tooManyCerts, *chains);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    auto tooManyChains = std::make_unique<StaticArray<crypto::CertificateChainInfo, crypto::cCertChainsCount + 1>>();

    for (size_t i = 0; i < crypto::cCertChainsCount + 1; ++i) {
        ASSERT_TRUE(tooManyChains->EmplaceBack().IsNone());
    }

    err = mImageManager.SetCerts(*certs, *tooManyChains);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());
    ExpectValidateSigns(signInfo, *chains, *certs);

    err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMImageManagerTest, SetCertsWaitsForSignVerification)
{
    const auto signInfo = MakeSignInfo();
    auto       certs    = MakeCerts({"cert1"});
    auto       chains   = MakeChains({"chain1"});
    auto       newCerts = MakeCerts({"cert2"});
    auto       newChain = MakeChains({"chain2"});

    std::promise<void> entered;
    std::promise<void> release;
    auto               releaseFuture = release.get_future().share();

    GetBlobInfo(mLayer).mSignInfo.SetValue(signInfo);

    ASSERT_TRUE(mImageManager.SetCerts(*certs, *chains).IsNone());

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mCryptoHelper, ValidateSigns(String(mLayer.mPath.c_str()), Eq(signInfo), _, _))
        .WillOnce(Invoke([&entered, releaseFuture](const String&, const crypto::SignInfo&,
                             const Array<crypto::CertificateChainInfo>&, const Array<crypto::CertificateInfo>&) {
            entered.set_value();
            releaseFuture.wait();

            return ErrorEnum::eNone;
        }));

    auto install = std::async(std::launch::async, [this]() { return Install(); });

    ASSERT_EQ(entered.get_future().wait_for(cWaitTimeout), std::future_status::ready);

    auto setCerts = std::async(
        std::launch::async, [this, &newCerts, &newChain]() { return mImageManager.SetCerts(*newCerts, *newChain); });

    EXPECT_EQ(setCerts.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);

    release.set_value();

    ASSERT_EQ(setCerts.wait_for(cWaitTimeout), std::future_status::ready);

    auto err = setCerts.get();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(install.wait_for(cWaitTimeout), std::future_status::ready);

    err = install.get();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallDownloadsBlobs)
{
    for (const auto* blob : GetBlobs()) {
        const auto hash = CalculateHash(blob->mContent, crypto::HashEnum::eSHA3_256);

        ASSERT_TRUE(GetBlobInfo(*blob).mSHA256.Assign(Array<uint8_t>(hash.data(), hash.size())).IsNone());
    }

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mCryptoHelper, Decrypt(_, _, _)).Times(0);
    EXPECT_CALL(mCryptoHelper, ValidateSigns(_, _, _, _)).Times(0);

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    for (const auto* blob : GetBlobs()) {
        EXPECT_EQ(ReadFile(blob->mPath), blob->mContent) << blob->mDigest;
        EXPECT_EQ(ReadFile(blob->mDownloadPath), blob->mContent) << blob->mDigest;
        EXPECT_EQ(
            GetSpaceEvents(blob->mContent.size()), std::vector<SpaceEvent>({{blob->mContent.size(), "accept", 0}}))
            << blob->mDigest;
    }

    EXPECT_EQ(GetNotifiedStates(),
        std::vector<ItemStateEnum>(
            {ItemStateEnum::eInstalling, ItemStateEnum::eDownloading, ItemStateEnum::eInstalled}));

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallDecryptsBlob)
{
    crypto::DecryptInfo decryptInfo;

    decryptInfo.mBlockAlg = "AES256/CBC/pkcs7padding";

    for (uint8_t i = 0; i < 8; ++i) {
        ASSERT_TRUE(decryptInfo.mBlockIV.PushBack(i).IsNone());
        ASSERT_TRUE(decryptInfo.mBlockKey.PushBack(i + 100).IsNone());
    }

    GetBlobInfo(mLayer).mDecryptInfo.SetValue(decryptInfo);

    const auto encrypted = std::string(mLayer.mContent.rbegin(), mLayer.mContent.rend());

    SetDownloadContent(mLayer, encrypted);
    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mCryptoHelper, Decrypt(_, _, _)).Times(0);
    EXPECT_CALL(
        mCryptoHelper, Decrypt(String(mLayer.mDownloadPath.c_str()), String(mLayer.mPath.c_str()), Eq(decryptInfo)))
        .WillOnce(Invoke([](const String& encryptedPath, const String& decryptedPath, const crypto::DecryptInfo&) {
            const auto content = ReadFile(encryptedPath.CStr());

            WriteFile(decryptedPath.CStr(), std::string(content.rbegin(), content.rend()));

            return ErrorEnum::eNone;
        }));

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(mLayer.mPath), mLayer.mContent);
    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), encrypted);

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallVerifiesSigns)
{
    const auto signInfo = MakeSignInfo();
    auto       certs    = MakeCerts({"cert1"});
    auto       chains   = MakeChains({"chain1"});

    GetBlobInfo(mLayer).mSignInfo.SetValue(signInfo);

    ASSERT_TRUE(mImageManager.SetCerts(*certs, *chains).IsNone());

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mCryptoHelper, ValidateSigns(_, _, _, _)).Times(0);
    ExpectValidateSigns(signInfo, *chains, *certs);

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(mLayer.mPath), mLayer.mContent);
    EXPECT_TRUE(FileExists(mLayer.mDownloadPath));

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallSignVerificationFails)
{
    const auto signInfo = MakeSignInfo();
    auto       certs    = MakeCerts({"cert1"});
    auto       chains   = MakeChains({"chain1"});

    GetBlobInfo(mLayer).mSignInfo.SetValue(signInfo);

    ASSERT_TRUE(mImageManager.SetCerts(*certs, *chains).IsNone());

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads({&mIndex, &mManifest, &mConfig});
    ExpectValidateSigns(signInfo, *chains, *certs, ErrorEnum::eFailed);

    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(mLayer.mContent.size())).Times(1);

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(mLayer.mDownloadPath));
    EXPECT_FALSE(FileExists(mLayer.mPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eFailed);
}

TEST_F(CMImageManagerTest, InstallChecksumMismatch)
{
    ASSERT_TRUE(GetBlobInfo(mLayer).mSHA256.Resize(crypto::cSHA256Size, 0xaa).IsNone());

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads({&mIndex, &mManifest, &mConfig});

    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(mLayer.mContent.size())).Times(1);

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(mLayer.mDownloadPath));
    EXPECT_FALSE(FileExists(mLayer.mPath));
    EXPECT_TRUE(FileExists(mConfig.mDownloadPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eInvalidChecksum);
}

TEST_F(CMImageManagerTest, InstallDecryptFails)
{
    crypto::DecryptInfo decryptInfo;

    decryptInfo.mBlockAlg = "AES256/CBC/pkcs7padding";

    GetBlobInfo(mLayer).mDecryptInfo.SetValue(decryptInfo);

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads({&mIndex, &mManifest, &mConfig});

    EXPECT_CALL(mCryptoHelper, Decrypt(String(mLayer.mDownloadPath.c_str()), String(mLayer.mPath.c_str()), _))
        .WillOnce(Return(Error(ErrorEnum::eFailed, "decrypt failed")));
    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(mLayer.mContent.size())).Times(1);

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(mLayer.mDownloadPath));
    EXPECT_FALSE(FileExists(mLayer.mPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eFailed);
}

TEST_F(CMImageManagerTest, InstallCopyFailsKeepsDownload)
{
    auto err = fs::MakeDirAll(mLayer.mPath.c_str());
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(_)).Times(0);

    err = Install();
    EXPECT_FALSE(err.IsNone());

    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), mLayer.mContent);

    auto item = GetStoredItem();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->mState, ItemStateEnum::eFailed);
}

TEST_F(CMImageManagerTest, InstallMalformedBlobDigest)
{
    GetBlobInfo(mIndex).mDigest = "sha256:../../x";

    EXPECT_CALL(mDownloader, Download(_, _, _, _)).Times(0);
    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(_)).Times(0);
    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(_)).Times(0);

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(std::string(cTestDir) + "/x"));
    EXPECT_FALSE(fs::DirExist(cTestDownloadPath).mValue);

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eInvalidArgument);
}

TEST_F(CMImageManagerTest, InstallDownloadSizeMismatch)
{
    constexpr size_t cPartSize = 7;

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads({&mIndex, &mManifest, &mConfig});

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(mLayer.mContent.size())).Times(1);
    EXPECT_CALL(mDownloader, Download(_, String(mLayer.mDigest.c_str()), _, String(mLayer.mDownloadPath.c_str())))
        .WillOnce(
            Invoke([this](CancelContext&, const String&, const Array<StaticString<cURLLen>>&, const String& path) {
                WriteFile(path.CStr(), mLayer.mContent.substr(0, cPartSize));

                return ErrorEnum::eNone;
            }));
    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(mLayer.mContent.size())).Times(1);

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetSpaceEvents(mLayer.mContent.size()), std::vector<SpaceEvent>({{mLayer.mContent.size(), "accept", 0}}));
    EXPECT_FALSE(FileExists(mLayer.mDownloadPath));
    EXPECT_FALSE(FileExists(mLayer.mPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eInvalidChecksum);
}

TEST_F(CMImageManagerTest, InstallResumesPartialDownload)
{
    constexpr size_t cPartSize = 5;

    WriteFile(mLayer.mDownloadPath, mLayer.mContent.substr(0, cPartSize));

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    {
        InSequence sequence;

        EXPECT_CALL(mDownloadSpaceAllocator, RestoreOutdatedItem(String(mLayer.mDigest.c_str()), String("")))
            .WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(mLayer.mContent.size() - cPartSize)).Times(1);
    }

    EXPECT_CALL(mDownloader, Download(_, String(mLayer.mDigest.c_str()), _, String(mLayer.mDownloadPath.c_str())))
        .Times(1);

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), mLayer.mContent);
    EXPECT_EQ(ReadFile(mLayer.mPath), mLayer.mContent);
    EXPECT_EQ(GetSpaceEvents(mLayer.mContent.size() - cPartSize),
        std::vector<SpaceEvent>({{mLayer.mContent.size() - cPartSize, "accept", 0}}));

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallReusesCompleteDownload)
{
    WriteFile(mLayer.mDownloadPath, mLayer.mContent);

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(mLayer.mPath), mLayer.mContent);
    EXPECT_TRUE(FileExists(mLayer.mDownloadPath));

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, FinishDownloadTouchesReusedFile)
{
    WriteFile(mLayer.mDownloadPath, mLayer.mContent);
    SetModifyTime(mLayer.mDownloadPath, cPastTime);

    const auto oldTime = GetModifyTime(mLayer.mDownloadPath);

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    const auto newTime = GetModifyTime(mLayer.mDownloadPath);

    EXPECT_TRUE(IsNow(newTime));

    const auto items = GetOutdatedItems(mLayer.mDigest);
    ASSERT_FALSE(items.empty());
    EXPECT_EQ(items.back().mTimestamp.UnixTime().tv_sec, newTime.UnixTime().tv_sec);
    EXPECT_NE(items.back().mTimestamp.UnixTime().tv_sec, oldTime.UnixTime().tv_sec);

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallRemovesOversizedDownload)
{
    const auto oversized = mLayer.mContent + " and some garbage";

    WriteFile(mLayer.mDownloadPath, oversized);

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    {
        InSequence sequence;

        EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(oversized.size())).Times(1);
        ExpectDownload(mLayer);
    }

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), mLayer.mContent);
    EXPECT_EQ(ReadFile(mLayer.mPath), mLayer.mContent);

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallDownloadFails)
{
    constexpr size_t cPartSize = 7;

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(mLayer.mContent.size())).Times(1);
    EXPECT_CALL(mDownloader, Download(_, String(mLayer.mDigest.c_str()), _, String(mLayer.mDownloadPath.c_str())))
        .WillOnce(
            Invoke([this](CancelContext&, const String&, const Array<StaticString<cURLLen>>&, const String& path) {
                WriteFile(path.CStr(), mLayer.mContent.substr(0, cPartSize));

                return Error(ErrorEnum::eRuntime, "download failed");
            }));

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eRuntime)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetSpaceEvents(mLayer.mContent.size()),
        std::vector<SpaceEvent>(
            {{mLayer.mContent.size(), "resize", cPartSize}, {mLayer.mContent.size(), "accept", 0}}));
    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), mLayer.mContent.substr(0, cPartSize));
    EXPECT_FALSE(FileExists(mLayer.mPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eRuntime);
}

TEST_F(CMImageManagerTest, InstallDownloadFailsResume)
{
    constexpr size_t cPartSize       = 5;
    constexpr size_t cDownloadedSize = 4;
    const auto       allocateSize    = mLayer.mContent.size() - cPartSize;

    WriteFile(mLayer.mDownloadPath, mLayer.mContent.substr(0, cPartSize));

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(allocateSize)).Times(1);
    EXPECT_CALL(mDownloader, Download(_, String(mLayer.mDigest.c_str()), _, String(mLayer.mDownloadPath.c_str())))
        .WillOnce(
            Invoke([this](CancelContext&, const String&, const Array<StaticString<cURLLen>>&, const String& path) {
                WriteFile(path.CStr(), mLayer.mContent.substr(cPartSize, cDownloadedSize), true);

                return Error(ErrorEnum::eRuntime, "download failed");
            }));

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eRuntime)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetSpaceEvents(allocateSize),
        std::vector<SpaceEvent>({{allocateSize, "resize", cDownloadedSize}, {allocateSize, "accept", 0}}));
    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), mLayer.mContent.substr(0, cPartSize + cDownloadedSize));
    EXPECT_FALSE(FileExists(mLayer.mPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eRuntime);
}

TEST_F(CMImageManagerTest, InstallDownloadFailsAfterRestart)
{
    constexpr size_t cPartSize       = 10;
    constexpr size_t cDownloadedSize = 3;
    const auto       allocateSize    = mLayer.mContent.size() - cPartSize;

    WriteFile(mLayer.mDownloadPath, mLayer.mContent.substr(0, cPartSize));

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(allocateSize)).Times(1);
    EXPECT_CALL(mDownloader, Download(_, String(mLayer.mDigest.c_str()), _, String(mLayer.mDownloadPath.c_str())))
        .WillOnce(
            Invoke([this](CancelContext&, const String&, const Array<StaticString<cURLLen>>&, const String& path) {
                WriteFile(path.CStr(), mLayer.mContent.substr(0, cDownloadedSize));

                return Error(ErrorEnum::eRuntime, "download failed");
            }));
    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(cPartSize - cDownloadedSize))
        .WillOnce(Invoke([this, allocateSize](size_t size) {
            AddSpaceEvent({allocateSize, "free", size});
        }));

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eRuntime)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetSpaceEvents(allocateSize),
        std::vector<SpaceEvent>({{allocateSize, "resize", 0}, {allocateSize, "accept", 0},
            {allocateSize, "free", cPartSize - cDownloadedSize}}));
    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), mLayer.mContent.substr(0, cDownloadedSize));
    EXPECT_FALSE(FileExists(mLayer.mPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eRuntime);
}

TEST_F(CMImageManagerTest, InstallAllocateSpaceFails)
{
    constexpr size_t cPartSize = 5;

    WriteFile(mLayer.mDownloadPath, mLayer.mContent.substr(0, cPartSize));

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(mLayer.mContent.size() - cPartSize))
        .WillOnce(Invoke([](size_t) -> RetWithError<UniquePtr<spaceallocator::SpaceItf>> {
            return {UniquePtr<spaceallocator::SpaceItf>(), ErrorEnum::eNoMemory};
        }));
    EXPECT_CALL(mDownloader, Download(_, String(mLayer.mDigest.c_str()), _, _)).Times(0);

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), mLayer.mContent.substr(0, cPartSize));
    EXPECT_FALSE(FileExists(mLayer.mPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eNoMemory);
}

TEST_F(CMImageManagerTest, CancelDuringDownload)
{
    std::promise<void> started;

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(mLayer.mContent.size())).Times(1);
    EXPECT_CALL(mDownloader, Download(_, String(mLayer.mDigest.c_str()), _, String(mLayer.mDownloadPath.c_str())))
        .WillOnce(Invoke(
            [&started](CancelContext& ctx, const String&, const Array<StaticString<cURLLen>>&, const String& path) {
                WriteFile(path.CStr(), "");
                started.set_value();

                return ctx.Wait(cDownloadTimeout);
            }));

    auto install = std::async(std::launch::async, [this]() { return Install(); });

    ASSERT_EQ(started.get_future().wait_for(cWaitTimeout), std::future_status::ready);

    auto err = mImageManager.Cancel();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(install.wait_for(cWaitTimeout), std::future_status::ready);

    err = install.get();
    EXPECT_TRUE(err.Is(ErrorEnum::eCanceled)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetSpaceEvents(mLayer.mContent.size()),
        std::vector<SpaceEvent>({{mLayer.mContent.size(), "resize", 0}, {mLayer.mContent.size(), "accept", 0}}));
    EXPECT_TRUE(FileExists(mLayer.mDownloadPath));
    EXPECT_FALSE(FileExists(mLayer.mPath));

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eCanceled);
}

TEST_F(CMImageManagerTest, CancelStopsNextBlobDownload)
{
    std::promise<void> started;

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(mIndex.mContent.size())).Times(1);
    EXPECT_CALL(mDownloader, Download(_, _, _, _))
        .WillOnce(
            Invoke([&started](CancelContext& ctx, const String&, const Array<StaticString<cURLLen>>&, const String&) {
                started.set_value();

                return ctx.Wait(cDownloadTimeout);
            }));

    auto install = std::async(std::launch::async, [this]() { return Install(); });

    ASSERT_EQ(started.get_future().wait_for(cWaitTimeout), std::future_status::ready);

    auto err = mImageManager.Cancel();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(install.wait_for(cWaitTimeout), std::future_status::ready);

    err = install.get();
    EXPECT_TRUE(err.Is(ErrorEnum::eCanceled)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(GetSpaceEvents(mIndex.mContent.size()),
        std::vector<SpaceEvent>({{mIndex.mContent.size(), "resize", 0}, {mIndex.mContent.size(), "accept", 0}}));

    for (const auto* blob : GetBlobs()) {
        EXPECT_FALSE(FileExists(blob->mPath)) << blob->mDigest;
    }

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eCanceled);
}

TEST_F(CMImageManagerTest, InstallRegistersDownloadFiles)
{
    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    for (const auto* blob : GetBlobs()) {
        const auto acceptIndex = GetSpaceEventIndex({blob->mContent.size(), "accept", 0});
        ASSERT_TRUE(acceptIndex.has_value()) << blob->mDigest;

        const auto items = GetOutdatedItems(blob->mDigest);
        ASSERT_FALSE(items.empty()) << blob->mDigest;

        for (const auto& item : items) {
            EXPECT_GT(item.mSpaceEventIndex, *acceptIndex) << blob->mDigest;
            EXPECT_TRUE(IsNow(item.mTimestamp)) << blob->mDigest;
        }
    }

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallRegistersLeftoverDownloads)
{
    const auto leftover1       = std::string(cTestDownloadPath) + "/sha256/" + cLeftoverHash1;
    const auto leftover2       = std::string(cTestDownloadPath) + "/sha256/" + cLeftoverHash2;
    const auto leftoverDigest1 = std::string("sha256:") + cLeftoverHash1;
    const auto leftoverDigest2 = std::string("sha256:") + cLeftoverHash2;
    const auto invalidName     = std::string(cTestDownloadPath) + "/sha256/NOT-A-HASH";
    const auto invalidAlgFile  = std::string(cTestDownloadPath) + "/junk";
    const auto invalidContent  = std::string("invalid");
    const auto junkContent     = std::string("junk content");

    WriteFile(leftover1, "leftover 1");
    WriteFile(leftover2, "leftover 2");
    WriteFile(invalidName, invalidContent);
    WriteFile(invalidAlgFile, junkContent);
    WriteFile(mIndex.mDownloadPath, mIndex.mContent);

    SetModifyTime(leftover1, cPastTime);
    SetModifyTime(leftover2, cPastTime + 1);
    SetModifyTime(mIndex.mDownloadPath, cPastTime + 2);

    const auto modifyTime1 = GetModifyTime(leftover1);
    const auto modifyTime2 = GetModifyTime(leftover2);
    const auto indexTime   = GetModifyTime(mIndex.mDownloadPath);

    ExpectDownloads({&mManifest, &mConfig, &mLayer});
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mDownloadSpaceAllocator, AddOutdatedItem(String(leftoverDigest1.c_str()), String(""), Eq(modifyTime1)))
        .Times(AtLeast(1));
    EXPECT_CALL(mDownloadSpaceAllocator, AddOutdatedItem(String(leftoverDigest2.c_str()), String(""), Eq(modifyTime2)))
        .Times(AtLeast(1));
    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(invalidContent.size())).Times(1);
    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(junkContent.size())).Times(1);

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(FileExists(leftover1));
    EXPECT_TRUE(FileExists(leftover2));
    EXPECT_FALSE(FileExists(invalidName));
    EXPECT_FALSE(FileExists(invalidAlgFile));
    EXPECT_EQ(ReadFile(mIndex.mPath), mIndex.mContent);

    const auto indexItems = GetOutdatedItems(mIndex.mDigest);
    ASSERT_FALSE(indexItems.empty());
    EXPECT_TRUE(IsNow(indexItems.front().mTimestamp));
    EXPECT_FALSE(indexItems.front().mTimestamp == indexTime);

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallRestoresBeforeScan)
{
    const auto leftover       = std::string(cTestDownloadPath) + "/sha256/" + cLeftoverHash1;
    const auto leftoverDigest = std::string("sha256:") + cLeftoverHash1;

    WriteFile(leftover, "leftover");

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    {
        InSequence sequence;

        EXPECT_CALL(mDownloadSpaceAllocator, RestoreOutdatedItem(String(mIndex.mDigest.c_str()), String("")))
            .WillOnce(Return(ErrorEnum::eNotFound));
        EXPECT_CALL(mDownloadSpaceAllocator, AddOutdatedItem(String(leftoverDigest.c_str()), String(""), _))
            .Times(AtLeast(1));
    }

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(FileExists(leftover));

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, InstallRestoresCurrentDownload)
{
    const auto index2         = AddBlob("second image index blob content: index of manifests");
    const auto manifest2      = AddBlob("second image manifest blob content");
    const auto config2        = AddBlob("second image config: config");
    const auto layer2         = AddBlob("second image layer blob");
    const auto leftover       = std::string(cTestDownloadPath) + "/sha256/" + cLeftoverHash1;
    const auto leftoverDigest = std::string("sha256:") + cLeftoverHash1;
    const auto blobs2         = std::vector<const TestBlob*>({&index2, &manifest2, &config2, &layer2});

    AddImage(index2, manifest2, config2, layer2, cTestDiffID2);

    std::promise<void> started;
    std::promise<void> release;
    auto               releaseFuture = release.get_future().share();

    ExpectDownloads({&mManifest, &mConfig, &mLayer});
    ExpectDownloads(blobs2);
    ExpectRegisterDownloads(GetBlobs());
    ExpectRegisterDownloads(blobs2);

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(mIndex.mContent.size())).Times(1);
    EXPECT_CALL(mDownloader, Download(_, String(mIndex.mDigest.c_str()), _, String(mIndex.mDownloadPath.c_str())))
        .WillOnce(Invoke([this, &started, releaseFuture](CancelContext&, const String& digest,
                             const Array<StaticString<cURLLen>>&, const String&        path) {
            started.set_value();
            releaseFuture.wait();

            return DownloadContent(digest, path);
        }));
    EXPECT_CALL(mDownloadSpaceAllocator, AddOutdatedItem(String(leftoverDigest.c_str()), String(""), _))
        .Times(AnyNumber());

    for (const auto* blob : blobs2) {
        EXPECT_CALL(mDownloadSpaceAllocator, RestoreOutdatedItem(String(blob->mDigest.c_str()), String("")))
            .WillOnce(Return(ErrorEnum::eNotFound));
    }

    auto install1 = std::async(std::launch::async, [this]() { return Install(); });

    ASSERT_EQ(started.get_future().wait_for(cWaitTimeout), std::future_status::ready);

    WriteFile(leftover, "leftover");

    auto install2 = std::async(std::launch::async,
        [this, &index2]() { return mImageManager.InstallItem(MakeItemInfo(cTestItemID2, &index2)); });

    const auto install2Status = install2.wait_for(cWaitTimeout);

    EXPECT_EQ(install1.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);
    EXPECT_TRUE(GetOutdatedItems(leftoverDigest).empty());

    release.set_value();

    ASSERT_EQ(install2Status, std::future_status::ready);

    auto err = install2.get();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(install1.wait_for(cWaitTimeout), std::future_status::ready);

    err = install1.get();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    for (const auto* blob : blobs2) {
        EXPECT_EQ(ReadFile(blob->mPath), blob->mContent) << blob->mDigest;
    }

    ExpectItemState(ItemStateEnum::eInstalled);
}

TEST_F(CMImageManagerTest, BrokenDownloadNotRegistered)
{
    ASSERT_TRUE(GetBlobInfo(mLayer).mSHA256.Resize(crypto::cSHA256Size, 0xaa).IsNone());

    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads({&mIndex, &mManifest, &mConfig});

    EXPECT_CALL(mDownloadSpaceAllocator, AddOutdatedItem(String(mLayer.mDigest.c_str()), _, _)).Times(0);
    EXPECT_CALL(mDownloadSpaceAllocator, FreeSpace(mLayer.mContent.size())).Times(1);

    auto err = Install();
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(FileExists(mLayer.mDownloadPath));
    EXPECT_TRUE(GetOutdatedItems(mLayer.mDigest).empty());

    ExpectItemState(ItemStateEnum::eFailed, ErrorEnum::eInvalidChecksum);
}

TEST_F(CMImageManagerTest, CanceledDownloadRegistered)
{
    std::promise<void> started;

    ExpectDownloads({&mIndex, &mManifest, &mConfig});
    ExpectRegisterDownloads(GetBlobs());

    EXPECT_CALL(mDownloadSpaceAllocator, AllocateSpace(mLayer.mContent.size())).Times(1);
    EXPECT_CALL(mDownloader, Download(_, String(mLayer.mDigest.c_str()), _, String(mLayer.mDownloadPath.c_str())))
        .WillOnce(Invoke(
            [&started](CancelContext& ctx, const String&, const Array<StaticString<cURLLen>>&, const String& path) {
                WriteFile(path.CStr(), "partial");
                started.set_value();

                return ctx.Wait(cDownloadTimeout);
            }));

    auto install = std::async(std::launch::async, [this]() { return Install(); });

    ASSERT_EQ(started.get_future().wait_for(cWaitTimeout), std::future_status::ready);

    EXPECT_TRUE(GetOutdatedItems(mLayer.mDigest).empty());

    auto err = mImageManager.Cancel();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(install.wait_for(cWaitTimeout), std::future_status::ready);

    err = install.get();
    EXPECT_TRUE(err.Is(ErrorEnum::eCanceled)) << tests::utils::ErrorToStr(err);

    const auto items = GetOutdatedItems(mLayer.mDigest);
    ASSERT_EQ(items.size(), 1);
    EXPECT_TRUE(IsNow(items.front().mTimestamp));
    EXPECT_EQ(ReadFile(mLayer.mDownloadPath), "partial");
}

TEST_F(CMImageManagerTest, DownloadItemRemoverRemovesFile)
{
    WriteFile(mLayer.mDownloadPath, mLayer.mContent);
    WriteFile(mLayer.mPath, mLayer.mContent);

    EXPECT_CALL(mDownloadSpaceAllocator, RestoreOutdatedItem(_, _)).Times(0);

    auto [size, err] = mImageManager.GetDownloadItemRemover().RemoveItem(mLayer.mDigest.c_str(), "");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(size, mLayer.mContent.size());
    EXPECT_FALSE(FileExists(mLayer.mDownloadPath));
    EXPECT_EQ(ReadFile(mLayer.mPath), mLayer.mContent);
}

TEST_F(CMImageManagerTest, DownloadItemRemoverMissingFile)
{
    EXPECT_CALL(mDownloadSpaceAllocator, RestoreOutdatedItem(_, _)).Times(0);

    auto [size, err] = mImageManager.GetDownloadItemRemover().RemoveItem(mLayer.mDigest.c_str(), "");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(size, 0);
}

TEST_F(CMImageManagerTest, DownloadItemRemoverMalformedDigest)
{
    EXPECT_CALL(mDownloadSpaceAllocator, RestoreOutdatedItem(_, _)).Times(0);

    auto [size, err] = mImageManager.GetDownloadItemRemover().RemoveItem("sha256:../../x", "");
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(size, 0);
}

TEST_F(CMImageManagerTest, GetBlobInfoReturnsStoredBlob)
{
    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    const auto url = "https://cm/blobs/" + mLayer.mDigest.substr(mLayer.mDigest.find(':') + 1);

    BlobInfo blobInfo;

    blobInfo.mDigest = "sha256:garbage";
    blobInfo.mSize   = 1;
    blobInfo.mDecryptInfo.SetValue(crypto::DecryptInfo {});
    blobInfo.mSignInfo.SetValue(MakeSignInfo());

    ASSERT_TRUE(blobInfo.mURLs.EmplaceBack("https://example.com/garbage").IsNone());
    ASSERT_TRUE(blobInfo.mSHA256.PushBack(0xff).IsNone());

    EXPECT_CALL(mFileServer, TranslateFilePathURL(String(mLayer.mPath.c_str()), _))
        .WillOnce(Invoke([&url](const String&, String& outURL) { return outURL.Assign(url.c_str()); }));

    err = mImageManager.GetBlobInfo(mLayer.mDigest.c_str(), blobInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(blobInfo.mDigest, String(mLayer.mDigest.c_str()));
    EXPECT_EQ(blobInfo.mSize, mLayer.mContent.size());
    EXPECT_TRUE(blobInfo.mSHA256.IsEmpty());
    ASSERT_EQ(blobInfo.mURLs.Size(), 1);
    EXPECT_EQ(blobInfo.mURLs[0], String(url.c_str()));
    EXPECT_FALSE(blobInfo.mDecryptInfo.HasValue());
    EXPECT_FALSE(blobInfo.mSignInfo.HasValue());
}

TEST_F(CMImageManagerTest, GetBlobInfoUnusedBlob)
{
    WriteFile(mLayer.mPath, mLayer.mContent);

    EXPECT_CALL(mFileServer, TranslateFilePathURL(_, _)).Times(0);

    BlobInfo blobInfo;

    auto err = mImageManager.GetBlobInfo(mLayer.mDigest.c_str(), blobInfo);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(CMImageManagerTest, GetBlobInfoRemovedItem)
{
    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mImageManager.RemoveItem(cTestItemID, cTestVersion);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectItemState(ItemStateEnum::eRemoved);

    EXPECT_CALL(mFileServer, TranslateFilePathURL(String(mLayer.mPath.c_str()), _)).WillOnce(Return(ErrorEnum::eNone));

    BlobInfo blobInfo;

    err = mImageManager.GetBlobInfo(mLayer.mDigest.c_str(), blobInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(blobInfo.mSize, mLayer.mContent.size());
}

TEST_F(CMImageManagerTest, GetBlobInfoMalformedDigest)
{
    EXPECT_CALL(mFileServer, TranslateFilePathURL(_, _)).Times(0);

    BlobInfo blobInfo;

    auto err = mImageManager.GetBlobInfo("not-a-digest", blobInfo);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);
}

TEST_F(CMImageManagerTest, GetBlobInfoMissingFile)
{
    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = fs::RemoveAll(mLayer.mPath.c_str());
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFileServer, TranslateFilePathURL(_, _)).Times(0);

    BlobInfo blobInfo;

    err = mImageManager.GetBlobInfo(mLayer.mDigest.c_str(), blobInfo);
    EXPECT_FALSE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_FALSE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(CMImageManagerTest, GetBlobInfoFileServerFails)
{
    ExpectDownloads(GetBlobs());
    ExpectRegisterDownloads(GetBlobs());

    auto err = Install();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFileServer, TranslateFilePathURL(String(mLayer.mPath.c_str()), _))
        .WillOnce(Return(ErrorEnum::eFailed));

    BlobInfo blobInfo;

    blobInfo.mDigest = "sha256:unchanged";
    blobInfo.mSize   = 42;

    err = mImageManager.GetBlobInfo(mLayer.mDigest.c_str(), blobInfo);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(blobInfo.mDigest, "sha256:unchanged");
    EXPECT_EQ(blobInfo.mSize, 42U);
    EXPECT_TRUE(blobInfo.mURLs.IsEmpty());
}

} // namespace aos::cm::imagemanager
