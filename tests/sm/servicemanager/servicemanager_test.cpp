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
static const auto     cServicesDir = fs::JoinPath(cTestRootDir, "services");
static const auto     cDownloadDir = fs::JoinPath(cTestRootDir, "download");

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

        fs::ClearDir(cTestRootDir);

        mImageHandler.Init(mServiceSpaceAllocator);

        EXPECT_CALL(mDownloader, Download).WillRepeatedly(Return(ErrorEnum::eNone));
    }

    Config                             mConfig = CreateConfig();
    oci::OCISpecMock                   mOCIManager;
    downloader::DownloaderMock         mDownloader;
    StorageStub                        mStorage;
    spaceallocator::SpaceAllocatorStub mServiceSpaceAllocator;
    spaceallocator::SpaceAllocatorStub mDownloadSpaceAllocator;
    image::ImageHandlerStub            mImageHandler;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ServiceManagerTest, DamagedServicesAreRemovedOnStart)
{
    const std::vector<ServiceData> validServices = {
        {"service0", "provider0", "1.0.0", fs::JoinPath(cServicesDir, "s0"), "", Time::Now(), ServiceStateEnum::eActive,
            0, 0},
        {"service1", "provider1", "1.0.0", fs::JoinPath(cServicesDir, "s1"), "", Time::Now(), ServiceStateEnum::eActive,
            0, 0},
    };
    const ServiceData damagedService = {"service2", "provider2", "1.0.0", fs::JoinPath(cServicesDir, "damaged"), "",
        Time::Now(), ServiceStateEnum::eActive, 0, 0};

    for (const auto& service : validServices) {
        fs::MakeDirAll(service.mImagePath);
        ASSERT_TRUE(mStorage.AddService(service).IsNone());
    }

    ASSERT_TRUE(mStorage.AddService(damagedService).IsNone());

    ServiceManager serviceManager;

    ASSERT_TRUE(serviceManager
                    .Init(mConfig, mOCIManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHandler)
                    .IsNone());

    auto services = std::make_unique<ServiceDataStaticArray>();

    ASSERT_TRUE(mStorage.GetAllServices(*services).IsNone());

    EXPECT_EQ(services->Size(), validServices.size());
    EXPECT_TRUE(test::CompareArrays(*services, Array<ServiceData>(validServices.data(), validServices.size())));
}

TEST_F(ServiceManagerTest, RemoveOutdatedServicesByTimer)
{
    mConfig.mTTL                  = aos::Time::cSeconds.Nanoseconds() / 2;
    mConfig.mRemoveOutdatedPeriod = aos::Time::cSeconds;

    const auto        expiredTime    = aos::Time::Now().Add(-mConfig.mTTL.Nanoseconds());
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

    auto services = std::make_unique<ServiceDataStaticArray>();

    ASSERT_TRUE(mStorage.GetAllServices(*services).IsNone());

    EXPECT_TRUE(test::CompareArrays(*services, Array<ServiceData>(expected.data(), expected.size())));
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
                {"service1", "provider1", "1.0.0", fs::JoinPath(cServicesDir, "service1"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service2", "provider2", "1.0.0", fs::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service3", "provider3", "1.0.0", fs::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service4", "provider4", "1.0.0", fs::JoinPath(cServicesDir, "service4"), "", {},
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
                {"service1", "provider1", "1.0.0", fs::JoinPath(cServicesDir, "service1"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service2", "provider2", "1.0.0", fs::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service3", "provider3", "1.0.0", fs::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service4", "provider4", "1.0.0", fs::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service5", "provider5", "1.0.0", fs::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service6", "provider6", "1.0.0", fs::JoinPath(cServicesDir, "service6"), "", {},
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
                {"service2", "provider2", "1.0.0", fs::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service3", "provider3", "1.0.0", fs::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service4", "provider4", "1.0.0", fs::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service4", "provider4", "2.0.0", fs::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service5", "provider5", "1.0.0", fs::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service5", "provider5", "3.0.0", fs::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service6", "provider6", "1.0.0", fs::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service6", "provider6", "4.0.0", fs::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
            },
        },
        {
            std::vector<ServiceInfo> {
                {"service1", "provider1", "3.0.0", 0, "url", {}, 0},
                {"service2", "provider2", "3.0.0", 0, "url", {}, 0},
                {"service3", "provider3", "3.0.0", 0, "url", {}, 0},
                {"service4", "provider4", "3.0.0", 0, "url", {}, 0},
                {"service5", "provider5", "3.0.0", 0, "url", {}, 0},
                {"service6", "provider6", "3.0.0", 0, "url", {}, 0},
                {"service7", "provider7", "3.0.0", 0, "url", {}, 0},
                {"service8", "provider8", "3.0.0", 0, "url", {}, 0},
            },
            std::vector<ServiceData> {
                {"service1", "provider1", "3.0.0", fs::JoinPath(cServicesDir, "service1"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service2", "provider2", "3.0.0", fs::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service3", "provider3", "3.0.0", fs::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service4", "provider4", "3.0.0", fs::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service5", "provider5", "3.0.0", fs::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service6", "provider6", "3.0.0", fs::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service7", "provider7", "3.0.0", fs::JoinPath(cServicesDir, "service7"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
                {"service8", "provider8", "3.0.0", fs::JoinPath(cServicesDir, "service8"), "", {},
                    ServiceStateEnum::eActive, 0, 0},
            },
        },
        {
            std::vector<ServiceInfo> {},
            std::vector<ServiceData> {
                {"service1", "provider1", "3.0.0", fs::JoinPath(cServicesDir, "service1"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service2", "provider2", "3.0.0", fs::JoinPath(cServicesDir, "service2"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service3", "provider3", "3.0.0", fs::JoinPath(cServicesDir, "service3"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service4", "provider4", "3.0.0", fs::JoinPath(cServicesDir, "service4"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service5", "provider5", "3.0.0", fs::JoinPath(cServicesDir, "service5"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service6", "provider6", "3.0.0", fs::JoinPath(cServicesDir, "service6"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service7", "provider7", "3.0.0", fs::JoinPath(cServicesDir, "service7"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
                {"service8", "provider8", "3.0.0", fs::JoinPath(cServicesDir, "service8"), "", {},
                    ServiceStateEnum::eCached, 0, 0},
            },
        },
    };

    size_t i = 0;

    for (auto& testItem : testData) {
        LOG_DBG() << "Running test case #" << i++;

        for (const auto& service : testItem.mData) {
            mImageHandler.SetInstallResult(fs::JoinPath(cDownloadDir, service.mServiceID), service.mImagePath);
            mImageHandler.SetCalculateDigestResult(fs::JoinPath(service.mImagePath, "manifest.json"), "sha256:123");
        }

        auto serviceStatuses = std::make_unique<ServiceStatusStaticArray>();

        EXPECT_TRUE(serviceManager
                        .ProcessDesiredServices(
                            Array<ServiceInfo>(testItem.mInfo.data(), testItem.mInfo.size()), *serviceStatuses)
                        .IsNone());

        ASSERT_EQ(serviceStatuses->Size(), testItem.mInfo.size()) << "Invalid service status size";

        if (auto it
            = serviceStatuses->FindIf([](const auto& status) { return status.mStatus != ItemStatusEnum::eInstalled; });
            it != serviceStatuses->end()) {
            LOG_DBG() << "Found unexpected service status: id" << it->mServiceID << ", version=" << it->mVersion
                      << ", status=" << it->mStatus << ", err=" << it->mError;

            FAIL() << "Invalid service status";
        }

        auto services = std::make_unique<ServiceDataStaticArray>();

        EXPECT_TRUE(mStorage.GetAllServices(*services).IsNone());

        EXPECT_THAT(std::vector<ServiceData>(services->begin(), services->end()),
            testing::UnorderedPointwise(testing::Truly([](const auto& tuple) {
                return CompareServiceData(std::get<0>(tuple), std::get<1>(tuple));
            }),
                testItem.mData));
    }
}

TEST_F(ServiceManagerTest, ProcessDesiredServicesOnInvalidService)
{
    ServiceManager serviceManager;

    ASSERT_TRUE(serviceManager
                    .Init(mConfig, mOCIManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHandler)
                    .IsNone());

    const auto testData = std::vector<ServiceInfo> {
        {"service1", "provider1", "1.0.0", 0, "url", {}, 0},
        {"service2", "provider2", "1.0.0", 0, "url", {}, 0},
        {"service3", "provider3", "1.0.0", 0, "url", {}, 0},
        {"service4", "provider4", "1.0.0", 0, "url", {}, 0},
    };

    for (const auto& service : testData) {
        const auto imagePath = fs::JoinPath(cServicesDir, service.mServiceID);

        mImageHandler.SetInstallResult(fs::JoinPath(cDownloadDir, service.mServiceID), imagePath);
        mImageHandler.SetCalculateDigestResult(fs::JoinPath(imagePath, "manifest.json"), "sha256:123");
    }

    const auto desiredServices = Array<ServiceInfo>(testData.data(), testData.size());
    auto       serviceStatuses = std::make_unique<ServiceStatusStaticArray>();

    ASSERT_TRUE(serviceManager.ProcessDesiredServices(desiredServices, *serviceStatuses).IsNone());

    ASSERT_EQ(serviceStatuses->Size(), testData.size()) << "Invalid service status size";

    if (auto it
        = serviceStatuses->FindIf([](const auto& status) { return status.mStatus != ItemStatusEnum::eInstalled; });
        it != serviceStatuses->end()) {
        LOG_DBG() << "Found unexpected service status: id=" << it->mServiceID << ", version=" << it->mVersion
                  << ", status=" << it->mStatus << ", err=" << it->mError;

        FAIL() << "Invalid service status";
    }

    // Change hash of an installed service
    mImageHandler.SetCalculateDigestResult(fs::JoinPath(cServicesDir, "service2", "manifest.json"), "hash-mismatch");

    serviceStatuses->Clear();
    ASSERT_TRUE(serviceManager.ProcessDesiredServices(desiredServices, *serviceStatuses).IsNone());

    ASSERT_EQ(serviceStatuses->Size(), testData.size()) << "Invalid service status size";

    if (auto it = serviceStatuses->FindIf([](const ServiceStatus& status) {
            return status.mServiceID == "service2" && status.mVersion == "1.0.0"
                && status.mStatus == ItemStatusEnum::eError && status.mError.Is(ErrorEnum::eInvalidChecksum);
        });
        it == serviceStatuses->end()) {

        FAIL() << "Invalid service status";
    }
}

TEST_F(ServiceManagerTest, RemoveService)
{
    ServiceManager serviceManager;

    ASSERT_TRUE(serviceManager
                    .Init(mConfig, mOCIManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHandler)
                    .IsNone());

    const auto testData = std::vector<ServiceInfo> {
        {"service1", "provider1", "1.0.0", 0, "url", {}, 0},
        {"service2", "provider2", "1.0.0", 0, "url", {}, 0},
        {"service3", "provider3", "1.0.0", 0, "url", {}, 0},
        {"service4", "provider4", "1.0.0", 0, "url", {}, 0},
    };

    for (const auto& service : testData) {
        const auto imagePath = fs::JoinPath(cServicesDir, service.mServiceID);

        mImageHandler.SetInstallResult(fs::JoinPath(cDownloadDir, service.mServiceID), imagePath);
        mImageHandler.SetCalculateDigestResult(fs::JoinPath(imagePath, "manifest.json"), "sha256:123");
    }

    const auto desiredServices = Array<ServiceInfo>(testData.data(), testData.size());
    auto       serviceStatuses = std::make_unique<ServiceStatusStaticArray>();

    ASSERT_TRUE(serviceManager.ProcessDesiredServices(desiredServices, *serviceStatuses).IsNone());

    auto services = std::make_unique<ServiceDataStaticArray>();
    ASSERT_TRUE(mStorage.GetAllServices(*services).IsNone());
    ASSERT_EQ(services->Size(), testData.size());

    for (const auto& service : *services) {
        ASSERT_TRUE(serviceManager.RemoveService(service).IsNone());
    }

    services->Clear();
    ASSERT_TRUE(mStorage.GetAllServices(*services).IsNone());
    ASSERT_TRUE(services->IsEmpty());
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
        manifest.mLayers.EmplaceBack("", "sha256:44444444", 0);

        return ErrorEnum::eNone;
    }));

    auto imageParts = std::make_unique<image::ImageParts>();

    ASSERT_TRUE(serviceManager.GetImageParts(serviceData, *imageParts).IsNone());

    EXPECT_TRUE(imageParts->mImageConfigPath == "/aos/services/service1/blobs/sha256/11111111");
    EXPECT_TRUE(imageParts->mServiceConfigPath == "/aos/services/service1/blobs/sha256/22222222");
    EXPECT_TRUE(imageParts->mServiceFSPath == "/aos/services/service1/blobs/sha256/33333333");

    ASSERT_TRUE(imageParts->mLayerDigests.Size() == 1);
    EXPECT_EQ(imageParts->mLayerDigests[0], "sha256:44444444");
}

} // namespace aos::sm::servicemanager
