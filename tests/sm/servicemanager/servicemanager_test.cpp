/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "aos/sm/servicemanager.hpp"

#include "aos/test/log.hpp"
#include "aos/test/utils.hpp"

#include "mocks/downloadermock.hpp"
#include "mocks/ocispecmock.hpp"

#include "stubs/imagehandlerstub.hpp"
#include "stubs/servicemanagerstub.hpp"
#include "stubs/spaceallocatorstub.hpp"

using namespace testing;

namespace aos::sm::servicemanager {

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

static constexpr auto cTestRootDir = "servicemanager_test";
static const auto     cServicesDir = FS::JoinPath(cTestRootDir, "services");
static const auto     cDownloadDir = FS::JoinPath(cTestRootDir, "download");

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

Config CreateConfig()
{
    Config config;

    config.mServicesDir = cServicesDir;
    config.mDownloadDir = cDownloadDir;
    config.mTTL         = aos::Time::cSeconds * 30;

    return config;
}

bool CompareServiceData(const ServiceData& data1, const ServiceData& data2)
{
    return data1.mServiceID == data2.mServiceID && data1.mVersion == data2.mVersion && data1.mState == data2.mState;
}

} // namespace

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct TestData {
    std::vector<ServiceInfo> mInfo;
    std::vector<ServiceData> mData;
};

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ServiceManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test::InitLog();

        FS::ClearDir(cTestRootDir);

        mImageHandler.Init(mServiceSpaceAllocator);
    }

    Config                             mConfig = CreateConfig();
    oci::OCISpecMock                   mOCIManager;
    downloader::DownloaderMock         mDownloader;
    StorageStub                        mStorage;
    spaceallocator::SpaceAllocatorStub mServiceSpaceAllocator;
    spaceallocator::SpaceAllocatorStub mDownloadSpaceAllocator;
    imagehandler::ImageHandlerStub     mImageHandler;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ServiceManagerTest, RemoveOutdatedServicesByTimer)
{
    mConfig.mTTL = aos::Time::cSeconds;

    const auto        expiredTime    = aos::Time::Now().Add(-mConfig.mTTL);
    const ServiceData expiredService = {
        "service1", "provider1", "1.0.0", "/aos/services/service1", "", expiredTime, ServiceStateEnum::eCached, 0, 0};
    const std::vector<ServiceData> expected = {
        {"service0", "provider0", "1.0.0", "/aos/services/service0", "", expiredTime, ServiceStateEnum::eActive, 0, 0},
        {"service1", "provider1", "2.0.0", "/aos/services/service1", "", Time::Now().Add(aos::Time::cHours),
            ServiceStateEnum::eCached, 0, 0},
    };

    ServiceManager serviceManager;

    ASSERT_TRUE(serviceManager
                    .Init(mConfig, mOCIManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHandler)
                    .IsNone());

    for (const auto& service : expected) {
        ASSERT_TRUE(mStorage.AddService(service).IsNone());
    }
    ASSERT_TRUE(mStorage.AddService(expiredService).IsNone());

    ASSERT_TRUE(serviceManager.Start().IsNone());

    for (size_t i = 1; i < 4; ++i) {
        if (mStorage.Size() == expected.size()) {
            break;
        }

        sleep(i);
    }

    ASSERT_TRUE(serviceManager.Stop().IsNone());

    ServiceDataStaticArray services;

    ASSERT_TRUE(mStorage.GetAllServices(services).IsNone());

    EXPECT_EQ(services.Size(), expected.size());
    EXPECT_TRUE(test::CompareArrays(services, Array<ServiceData>(expected.data(), expected.size())));
}

TEST_F(ServiceManagerTest, ProcessDesiredServices)
{
    ServiceManager serviceManager;

    ASSERT_TRUE(serviceManager
                    .Init(mConfig, mOCIManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHandler)
                    .IsNone());

    std::vector<TestData> testData = {
        {
            std::vector<ServiceInfo> {
                {"service1", "provider1", "1.0.0", 0, "url", {}, 0},
                {"service2", "provider2", "1.0.0", 0, "url", {}, 0},
                {"service3", "provider3", "1.0.0", 0, "url", {}, 0},
                {"service4", "provider4", "1.0.0", 0, "url", {}, 0},
            },
            std::vector<ServiceData> {
                {"service1", "provider1", "1.0.0", FS::JoinPath(cServicesDir, "service1"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service2", "provider2", "1.0.0", FS::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service3", "provider3", "1.0.0", FS::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service4", "provider4", "1.0.0", FS::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
            },
        },
        {
            std::vector<ServiceInfo> {
                {"service3", "provider3", "1.0.0", 0, "url", {}, 0},
                {"service4", "provider4", "1.0.0", 0, "url", {}, 0},
                {"service5", "provider5", "1.0.0", 0, "url", {}, 0},
                {"service6", "provider6", "1.0.0", 0, "url", {}, 0},
            },
            std::vector<ServiceData> {
                {"service1", "provider1", "1.0.0", FS::JoinPath(cServicesDir, "service1"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service2", "provider2", "1.0.0", FS::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service3", "provider3", "1.0.0", FS::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service4", "provider4", "1.0.0", FS::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service5", "provider5", "1.0.0", FS::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service6", "provider6", "1.0.0", FS::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
            },
        },
        {
            std::vector<ServiceInfo> {
                {"service3", "provider3", "1.0.0", 0, "url", {}, 0},
                {"service4", "provider4", "2.0.0", 0, "url", {}, 0},
                {"service5", "provider5", "3.0.0", 0, "url", {}, 0},
                {"service6", "provider6", "4.0.0", 0, "url", {}, 0},
            },
            std::vector<ServiceData> {
                {"service1", "provider1", "1.0.0", FS::JoinPath(cServicesDir, "service1"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service2", "provider2", "1.0.0", FS::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service3", "provider3", "1.0.0", FS::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service4", "provider4", "1.0.0", FS::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service4", "provider4", "2.0.0", FS::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service5", "provider5", "1.0.0", FS::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service5", "provider5", "3.0.0", FS::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service6", "provider6", "1.0.0", FS::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service6", "provider6", "4.0.0", FS::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
            },
        },
        {
            std::vector<ServiceInfo> {},
            std::vector<ServiceData> {
                {"service1", "provider1", "1.0.0", FS::JoinPath(cServicesDir, "service1"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service2", "provider2", "1.0.0", FS::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service3", "provider3", "1.0.0", FS::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service4", "provider4", "1.0.0", FS::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service4", "provider4", "2.0.0", FS::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service5", "provider5", "1.0.0", FS::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service5", "provider5", "3.0.0", FS::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service6", "provider6", "1.0.0", FS::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service6", "provider6", "4.0.0", FS::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
            },
        },
    };

    size_t i = 0;

    for (auto& testItem : testData) {
        LOG_DBG() << "Running test case #" << i++;

        for (const auto& service : testItem.mData) {
            mImageHandler.SetInstallResult(FS::JoinPath(cDownloadDir, service.mServiceID), service.mImagePath);
        }

        EXPECT_TRUE(
            serviceManager.ProcessDesiredServices(Array<ServiceInfo>(testItem.mInfo.data(), testItem.mInfo.size()))
                .IsNone());

        ServiceDataStaticArray installedServices;

        EXPECT_TRUE(mStorage.GetAllServices(installedServices).IsNone());

        EXPECT_THAT(std::vector<ServiceData>(installedServices.begin(), installedServices.end()),
            testing::UnorderedPointwise(testing::Truly([](const auto& tuple) {
                return CompareServiceData(std::get<0>(tuple), std::get<1>(tuple));
            }),
                testItem.mData));
    }
}

TEST_F(ServiceManagerTest, GetImageParts)
{
    ServiceManager serviceManager;

    ASSERT_TRUE(serviceManager
                    .Init(mConfig, mOCIManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHandler)
                    .IsNone());

    ServiceData serviceData
        = {"service0", "provider0", "2.1.0", "/aos/services/service1", "", {}, ServiceStateEnum::eActive, 0, 0};

    EXPECT_CALL(mOCIManager, LoadImageManifest).WillOnce(Invoke([&](const String& path, oci::ImageManifest& manifest) {
        (void)path;

        manifest.mSchemaVersion  = 1;
        manifest.mConfig.mDigest = "sha256:11111111";
        manifest.mAosService.EmplaceValue("", "sha256:22222222", 0);
        manifest.mLayers.EmplaceBack("", "sha256:33333333", 0);

        return ErrorEnum::eNone;
    }));

    auto imageParts = serviceManager.GetImageParts(serviceData);
    EXPECT_TRUE(imageParts.mError.IsNone());

    EXPECT_TRUE(imageParts.mValue.mImageConfigPath == "/aos/services/service1/blobs/sha256/11111111");
    EXPECT_TRUE(imageParts.mValue.mServiceConfigPath == "/aos/services/service1/blobs/sha256/22222222");
    EXPECT_TRUE(imageParts.mValue.mServiceFSPath == "/aos/services/service1/blobs/sha256/33333333");
}

} // namespace aos::sm::servicemanager
