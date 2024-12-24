/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/fs.hpp"
#include "aos/sm/layermanager.hpp"

#include "aos/test/log.hpp"
#include "aos/test/utils.hpp"

#include "mocks/downloadermock.hpp"

#include "stubs/imagehandlerstub.hpp"
#include "stubs/layermanagerstub.hpp"
#include "stubs/ocispecstub.hpp"
#include "stubs/spaceallocatorstub.hpp"

using namespace testing;

namespace aos::sm::layermanager {

namespace {

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

constexpr auto cTestRootDir = "layermanager_test";
const auto     cLayersDir   = FS::JoinPath(cTestRootDir, "layers");
const auto     cDownloadDir = FS::JoinPath(cTestRootDir, "download");
constexpr auto cTTL         = Time::cSeconds * 30;

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

Config CreateConfig(const String& layersDir = cLayersDir, const String& downloadDir = cDownloadDir, Duration ttl = cTTL)
{
    Config config;

    config.mLayersDir   = layersDir;
    config.mDownloadDir = downloadDir;
    config.mTTL         = ttl;

    return config;
}

StaticString<cFilePathLen> CreateExtractedLayerPath(const String& digest, const String& algorithm = "sha256")
{
    auto path = FS::JoinPath(cLayersDir, algorithm, digest);

    FS::MakeDirAll(path);

    return path;
}

LayerData CreateLayerData(const String& layerID, size_t size, LayerStateEnum state = LayerStateEnum::eActive,
    const Time& timestamp = Time::Now())
{
    LayerData layer;

    layer.mLayerID = layerID;
    layer.mLayerDigest.Append("sha256:").Append(layerID);
    layer.mSize      = size;
    layer.mState     = state;
    layer.mTimestamp = timestamp;

    return layer;
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class LayerManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test::InitLog();

        FS::ClearDir(FS::JoinPath(cLayersDir, "sha256"));
        FS::ClearDir(cDownloadDir);
    }

    void InitTest(const Config& config = CreateConfig())
    {
        EXPECT_CALL(mDownloader, Download).WillRepeatedly(Return(ErrorEnum::eNone));
        ASSERT_TRUE(mImageHandler.Init(mLayerSpaceAllocator).IsNone());

        mConfig = config;

        auto err = mManager.Init(mConfig, mLayerSpaceAllocator, mDownloadSpaceAllocator, mStorage, mDownloader,
            mImageHandler, mOCISpecManager);

        ASSERT_TRUE(err.IsNone()) << err.Message();
    }

    void StoreImageManifest(const aos::LayerInfo& layer)
    {
        oci::ImageManifest content = {};

        content.mConfig.mDigest = layer.mLayerDigest;
        content.mConfig.mSize   = layer.mSize;

        auto manifestPath = FS::JoinPath(cLayersDir, "sha256", layer.mLayerID, "layer.json");

        LOG_DBG() << "Store image manifest: path=" << manifestPath;

        if (auto err = mOCISpecManager.SaveImageManifest(manifestPath, content); !err.IsNone()) {
            LOG_ERR() << "Failed to save image manifest: path=" << manifestPath << ", error=" << err;
        }
    }

    aos::LayerInfo CreateAosLayer(const String& layerID, const std::string& uriPrefix)
    {
        aos::LayerInfo layer = {};

        layer.mLayerID = layerID;
        layer.mLayerDigest.Append("sha256:").Append(layerID);
        layer.mURL.Append(uriPrefix.c_str()).Append(layerID);

        const auto layerPath = FS::JoinPath(cLayersDir, "sha256", layerID);

        StoreImageManifest(layer);

        const StaticString<cFilePathLen> archivePath
            = (uriPrefix == "file://") ? layerID : FS::JoinPath(cDownloadDir, layer.mLayerDigest);

        EXPECT_TRUE(mImageHandler.SetInstallResult(archivePath, layerPath).IsNone());

        return layer;
    }

    LayerInfoStaticArray CreateAosLayers(const std::vector<std::string>& ids, const std::string& uriPrefix = "http://")
    {
        LayerInfoStaticArray layers;

        for (const auto& id : ids) {
            layers.PushBack(CreateAosLayer(id.c_str(), uriPrefix));
        }

        return layers;
    }

    void SetProcessDesiredLayerExpectedCalls() { }

    Config                             mConfig;
    LayerManager                       mManager;
    spaceallocator::SpaceAllocatorStub mLayerSpaceAllocator;
    spaceallocator::SpaceAllocatorStub mDownloadSpaceAllocator;
    StorageStub                        mStorage;
    downloader::DownloaderMock         mDownloader;
    image::ImageHandlerStub            mImageHandler;
    oci::OCISpecStub                   mOCISpecManager;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(LayerManagerTest, UnknownLayersAreClearedOnInit)
{
    auto unknownLayer  = CreateLayerData("unknown", 1024);
    unknownLayer.mPath = CreateExtractedLayerPath(unknownLayer.mLayerID);

    Error err;
    bool  exists;

    Tie(exists, err) = FS::DirExist(unknownLayer.mPath);
    ASSERT_TRUE(err.IsNone()) << err.Message();
    ASSERT_TRUE(exists);

    InitTest();

    Tie(exists, err) = FS::DirExist(unknownLayer.mPath);
    ASSERT_TRUE(err.IsNone()) << err.Message();
    ASSERT_FALSE(exists);
}

TEST_F(LayerManagerTest, GetLayerInfoByDigest)
{
    InitTest();

    LayerData dbLayer = CreateLayerData("layer1", 1024);

    ASSERT_TRUE(mStorage.AddLayer(dbLayer).IsNone());

    LayerData result;

    ASSERT_TRUE(mManager.GetLayer(dbLayer.mLayerDigest, result).IsNone());

    EXPECT_EQ(result.mLayerID, dbLayer.mLayerID);
    EXPECT_EQ(result.mLayerDigest, dbLayer.mLayerDigest);
}

TEST_F(LayerManagerTest, RemoveLayer)
{
    InitTest();

    LayerData dbLayer = CreateLayerData("layer1", 1024);

    ASSERT_TRUE(mStorage.AddLayer(dbLayer).IsNone());

    ASSERT_TRUE(mManager.RemoveItem(dbLayer.mLayerDigest).IsNone());

    StaticArray<LayerData, 1> dbLayers;

    ASSERT_TRUE(mStorage.GetAllLayers(dbLayers).IsNone());

    ASSERT_EQ(dbLayers.Size(), 0);
}

TEST_F(LayerManagerTest, ExpiredLayersAreClearedOnInit)
{
    auto config = CreateConfig();
    config.mTTL = Time::cSeconds;

    const auto expiredTTL = Time::Now().Add(-config.mTTL * 2);

    auto cachedAndExpiredLayer  = CreateLayerData("layer1", 1024, LayerStateEnum::eCached, expiredTTL);
    cachedAndExpiredLayer.mPath = CreateExtractedLayerPath("layer1");

    auto damagedFolderLayer  = CreateLayerData("layer2", 1024, LayerStateEnum::eCached, expiredTTL);
    damagedFolderLayer.mPath = "damaged_layer_folder_";

    auto notCachedAndExpiredLayer  = CreateLayerData("layer3", 1024, LayerStateEnum::eActive, expiredTTL);
    notCachedAndExpiredLayer.mPath = CreateExtractedLayerPath("layer3");

    ASSERT_TRUE(mStorage.AddLayer(cachedAndExpiredLayer).IsNone());
    ASSERT_TRUE(mStorage.AddLayer(damagedFolderLayer).IsNone());
    ASSERT_TRUE(mStorage.AddLayer(notCachedAndExpiredLayer).IsNone());

    InitTest(config);

    auto err = mManager.Start();
    ASSERT_TRUE(err.IsNone()) << err.Message();

    StaticArray<LayerData, 2> dbLayers;
    EXPECT_TRUE(mStorage.GetAllLayers(dbLayers).IsNone());

    ASSERT_EQ(dbLayers.Size(), 1);
    EXPECT_EQ(dbLayers[0].mLayerID, notCachedAndExpiredLayer.mLayerID);
}

TEST_F(LayerManagerTest, ValidateOutdateLayersByTimer)
{
    auto config = CreateConfig();
    config.mTTL = Time::cSeconds;

    InitTest(config);

    const auto                   expiredTTL     = Time::Now().Add(-config.mTTL * 2);
    const std::vector<LayerData> expectedLayers = {
        CreateLayerData("layer2", 1024, LayerStateEnum::eCached, Time::Now().Add(config.mTTL * 10)),
        CreateLayerData("layer3", 1024, LayerStateEnum::eActive, expiredTTL),
    };
    const auto cachedAndExpiredLayer = CreateLayerData("layer1", 1024, LayerStateEnum::eCached, expiredTTL);

    for (const auto& layer : expectedLayers) {
        ASSERT_TRUE(mStorage.AddLayer(layer).IsNone());
    }
    ASSERT_TRUE(mStorage.AddLayer(cachedAndExpiredLayer).IsNone());

    auto err = mManager.Start();
    ASSERT_TRUE(err.IsNone()) << err.Message();

    StaticArray<LayerData, 3> dbLayers;
    for (size_t i = 1; i < 4; ++i) {
        sleep(i);

        dbLayers.Clear();

        EXPECT_TRUE(mStorage.GetAllLayers(dbLayers).IsNone());

        if (dbLayers.Size() == 2) {
            break;
        }
    }

    ASSERT_TRUE(mManager.Stop().IsNone());

    sleep(1);

    ASSERT_TRUE(test::CompareArrays(dbLayers, Array<LayerData>(expectedLayers.data(), expectedLayers.size())));
}

TEST_F(LayerManagerTest, ProcessDesiredLayers)
{
    InitTest();

    struct {
        std::vector<std::string> mDesiredLayers;
        std::vector<std::string> mInstalledDigests;
        std::vector<std::string> mRemovedDigests;
        std::vector<std::string> mRestoredDigests;
    } testCases[] = {
        // Desired layers, installed layers, removed layers, restored layers
        {{"layer1", "layer2", "layer3"}, {"sha256:layer1", "sha256:layer2", "sha256:layer3"}, {}, {}},
        {{"layer1", "layer3"}, {"sha256:layer1", "sha256:layer3"}, {"sha256:layer2"}, {}},
        {{"layer1", "layer2"}, {"sha256:layer1", "sha256:layer2"}, {"sha256:layer3"}, {"sha256:layer2"}},
    };

    SetProcessDesiredLayerExpectedCalls();

    for (const auto& testCase : testCases) {

        auto desiredLayers = CreateAosLayers(testCase.mDesiredLayers);

        auto err = mManager.ProcessDesiredLayers(desiredLayers);
        ASSERT_TRUE(err.IsNone()) << err.Message();

        StaticArray<LayerData, 4> dbLayers;

        err = mStorage.GetAllLayers(dbLayers);
        ASSERT_TRUE(err.IsNone()) << err.Message();

        for (const auto& expectedLayer : testCase.mInstalledDigests) {
            EXPECT_TRUE(dbLayers
                            .FindIf([&expectedLayer](const auto& layer) {
                                return layer.mLayerDigest == expectedLayer.c_str()
                                    && layer.mState == LayerStateEnum::eActive;
                            })
                            .mError.IsNone())
                << "Layer " << expectedLayer << " is not installed";
        }
    }
}

TEST_F(LayerManagerTest, ProcessDesiredLayersFromFileURI)
{
    InitTest();

    struct {
        std::vector<std::string> mDesiredLayers;
        std::vector<std::string> mInstalledDigests;
        std::vector<std::string> mRemovedDigests;
        std::vector<std::string> mRestoredDigests;
    } testCases[] = {
        // Desired layers, installed layers, removed layers, restored layers
        {{"layer1", "layer2", "layer3"}, {"sha256:layer1", "sha256:layer2", "sha256:layer3"}, {}, {}},
        {{"layer1", "layer3"}, {"sha256:layer1", "sha256:layer3"}, {"sha256:layer2"}, {}},
        {{"layer1", "layer2"}, {"sha256:layer1", "sha256:layer2"}, {"sha256:layer3"}, {"sha256:layer2"}},
    };

    SetProcessDesiredLayerExpectedCalls();

    for (const auto& testCase : testCases) {

        auto desiredLayers = CreateAosLayers(testCase.mDesiredLayers, "file://");

        auto err = mManager.ProcessDesiredLayers(desiredLayers);
        ASSERT_TRUE(err.IsNone()) << err.Message();

        StaticArray<LayerData, 4> dbLayers;

        err = mStorage.GetAllLayers(dbLayers);
        ASSERT_TRUE(err.IsNone()) << err.Message();

        for (const auto& expectedLayer : testCase.mInstalledDigests) {
            EXPECT_TRUE(dbLayers
                            .FindIf([&expectedLayer](const auto& layer) {
                                return layer.mLayerDigest == expectedLayer.c_str()
                                    && layer.mState == LayerStateEnum::eActive;
                            })
                            .mError.IsNone())
                << "Layer " << expectedLayer << " is not installed";
        }
    }
}

} // namespace aos::sm::layermanager
