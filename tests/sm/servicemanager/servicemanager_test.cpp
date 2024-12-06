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

#include "imagehandlerstub.hpp"
#include "spaceallocatorstub.hpp"

using testing::Return;

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
 * Vars
 **********************************************************************************************************************/

static std::mutex sLogMutex;

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

/**
 * Mock OCI manager.
 */

class MockOCIManager : public oci::OCISpecItf {
public:
    Error LoadImageManifest(const String& path, oci::ImageManifest& manifest) override
    {
        (void)path;

        manifest.mSchemaVersion  = 1;
        manifest.mConfig.mDigest = "sha256:11111111";
        manifest.mLayers.PushBack({"", "sha256:33333333", 1234});

        if (manifest.mAosService) {
            manifest.mAosService->mDigest = "sha256:22222222";
        }

        return ErrorEnum::eNone;
    }

    Error SaveImageManifest(const String& path, const oci::ImageManifest& manifest) override
    {
        (void)path;
        (void)manifest;

        return ErrorEnum::eNone;
    }

    Error LoadImageSpec(const String& path, oci::ImageSpec& imageSpec) override
    {
        (void)path;

        imageSpec.mConfig.mCmd.EmplaceBack("unikernel");

        return ErrorEnum::eNone;
    }

    Error SaveImageSpec(const String& path, const oci::ImageSpec& imageSpec) override
    {
        (void)path;
        (void)imageSpec;

        return ErrorEnum::eNone;
    }

    Error LoadRuntimeSpec(const String& path, oci::RuntimeSpec& runtimeSpec) override
    {
        (void)path;
        (void)runtimeSpec;

        return ErrorEnum::eNone;
    }

    Error SaveRuntimeSpec(const String& path, const oci::RuntimeSpec& runtimeSpec) override
    {
        (void)path;
        (void)runtimeSpec;

        return ErrorEnum::eNone;
    }
};

/**
 * Mock downloader.
 */

class MockDownloader : public downloader::DownloaderItf {
public:
    Error Download(const String& url, const String& path, downloader::DownloadContent contentType) override
    {
        (void)url;

        EXPECT_EQ(contentType, downloader::DownloadContentEnum::eService);

        if (auto err = FS::MakeDirAll(path); !err.IsNone()) {
            return err;
        };

        return ErrorEnum::eNone;
    }
};

/**
 * Mock storage.
 */
class MockStorage : public StorageItf {
public:
    Error AddService(const ServiceData& service) override
    {
        std::lock_guard lock {mMutex};

        if (std::find_if(mServices.begin(), mServices.end(),
                [&service](const ServiceData& data) {
                    return service.mServiceID == data.mServiceID && service.mVersion == data.mVersion;
                })
            != mServices.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mServices.push_back(service);

        return ErrorEnum::eNone;
    }

    Error GetServiceVersions(const String& serviceID, Array<sm::servicemanager::ServiceData>& services) override
    {
        std::lock_guard lock {mMutex};

        Error err = ErrorEnum::eNotFound;

        for (const auto& service : mServices) {
            if (service.mServiceID == serviceID) {
                if (auto errPushBack = services.PushBack(service); !err.IsNone()) {
                    err = AOS_ERROR_WRAP(errPushBack);

                    break;
                }

                err = ErrorEnum::eNone;
            }
        }

        return ErrorEnum::eNone;
    }

    Error UpdateService(const ServiceData& service) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mServices.begin(), mServices.end(), [&service](const ServiceData& data) {
            return service.mServiceID == data.mServiceID && service.mVersion == data.mVersion;
        });

        if (it == mServices.end()) {
            return ErrorEnum::eNotFound;
        }

        *it = service;

        return ErrorEnum::eNone;
    }

    Error RemoveService(const String& serviceID, const String& version) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mServices.begin(), mServices.end(), [&serviceID, &version](const ServiceData& data) {
            return serviceID == data.mServiceID && version == data.mVersion;
        });
        if (it == mServices.end()) {
            return ErrorEnum::eNotFound;
        }

        mServices.erase(it);

        return ErrorEnum::eNone;
    }

    Error GetAllServices(Array<ServiceData>& services) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& service : mServices) {
            auto err = services.PushBack(service);
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    size_t Size() const
    {
        std::lock_guard lock {mMutex};

        return mServices.size();
    }

private:
    std::vector<ServiceData> mServices;
    mutable std::mutex       mMutex;
};

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ServiceManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        InitLog();

        FS::ClearDir(cTestRootDir);

        mImageHanlder.Init(mServiceSpaceAllocator);
    }

    Config                             mConfig = CreateConfig();
    MockOCIManager                     mOciManager;
    MockDownloader                     mDownloader;
    MockStorage                        mStorage;
    spaceallocator::SpaceAllocatorStub mServiceSpaceAllocator;
    spaceallocator::SpaceAllocatorStub mDownloadSpaceAllocator;
    imagehandler::ImageHandlerStub     mImageHanlder;
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
                    .Init(mConfig, mOciManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHanlder)
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
    EXPECT_TRUE(TestUtils::CompareArrays(services, Array<ServiceData>(expected.data(), expected.size())));
}

TEST_F(ServiceManagerTest, ProcessDesiredServices)
{
    ServiceManager serviceManager;

    ASSERT_TRUE(serviceManager
                    .Init(mConfig, mOciManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHanlder)
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
            mImageHanlder.SetInstallResult(FS::JoinPath(cDownloadDir, service.mServiceID), service.mImagePath);
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
                    .Init(mConfig, mOciManager, mDownloader, mStorage, mServiceSpaceAllocator, mDownloadSpaceAllocator,
                        mImageHanlder)
                    .IsNone());
    ServiceData serviceData
        = {"service0", "provider0", "2.1.0", "/aos/services/service1", "", {}, ServiceStateEnum::eActive, 0, 0};

    auto imageParts = serviceManager.GetImageParts(serviceData);
    EXPECT_TRUE(imageParts.mError.IsNone());

    EXPECT_TRUE(imageParts.mValue.mImageConfigPath == "/aos/services/service1/blobs/sha256/11111111");
    EXPECT_TRUE(imageParts.mValue.mServiceConfigPath == "/aos/services/service1/blobs/sha256/22222222");
    EXPECT_TRUE(imageParts.mValue.mServiceFSPath == "/aos/services/service1/blobs/sha256/33333333");
}

} // namespace aos::sm::servicemanager
