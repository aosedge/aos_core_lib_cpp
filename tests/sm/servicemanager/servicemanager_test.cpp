/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>

#include <gtest/gtest.h>

#include "aos/sm/servicemanager.hpp"

#include "log.hpp"
#include "utils.hpp"

namespace aos {
namespace sm {
namespace servicemanager {

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

class MockOCIManager : public OCISpecItf {
public:
    Error LoadImageManifest(const String& path, oci::ImageManifest& manifest) override
    {
        (void)path;

        manifest.mSchemaVersion       = 1;
        manifest.mConfig.mDigest      = "sha256:11111111";
        manifest.mAosService->mDigest = "sha256:22222222";
        manifest.mLayers.PushBack({"", "sha256:33333333", 1234});

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

class MockDownloader : public DownloaderItf {
public:
    Error Download(const String& url, const String& path, DownloadContent contentType) override
    {
        (void)url;
        (void)path;

        EXPECT_EQ(contentType, DownloadContentEnum::eService);

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
                [&service](const ServiceData& data) { return service.mServiceID == data.mServiceID; })
            != mServices.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mServices.push_back(service);

        return ErrorEnum::eNone;
    }

    RetWithError<ServiceData> GetService(const String& serviceID) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mServices.begin(), mServices.end(),
            [&serviceID](const ServiceData& data) { return serviceID == data.mServiceID; });

        if (it == mServices.end()) {
            return {*mServices.begin(), ErrorEnum::eNotFound};
        }

        return *it;
    }

    Error UpdateService(const ServiceData& service) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mServices.begin(), mServices.end(),
            [&service](const ServiceData& data) { return service.mServiceID == data.mServiceID; });

        if (it == mServices.end()) {
            return ErrorEnum::eNotFound;
        }

        *it = service;

        return ErrorEnum::eNone;
    }

    Error RemoveService(const String& serviceID, const String& version) override
    {
        std::lock_guard lock {mMutex};

        (void)version;

        auto it = std::find_if(mServices.begin(), mServices.end(),
            [&serviceID](const ServiceData& data) { return serviceID == data.mServiceID; });
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

private:
    std::vector<ServiceData> mServices;
    std::mutex               mMutex;
};

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ServiceManagerTest : public ::testing::Test {
protected:
    virtual ~ServiceManagerTest() { }

    virtual void SetUp() override { InitLog(); }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST(ServiceManagerTest, InstallServices)
{
    MockOCIManager ociManager;
    MockDownloader downloader;
    MockStorage    storage;

    ServiceManager serviceManager;

    EXPECT_TRUE(serviceManager.Init(ociManager, downloader, storage).IsNone());

    std::vector<TestData> testData = {
        {
            std::vector<ServiceInfo> {
                {"service1", "provider1", "1.0.0", 0, "url", {}, 0},
                {"service2", "provider2", "1.0.0", 0, "url", {}, 0},
                {"service3", "provider3", "1.0.0", 0, "url", {}, 0},
                {"service4", "provider4", "1.0.0", 0, "url", {}, 0},
            },
            std::vector<ServiceData> {
                {"service1", "provider1", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service1"},
                {"service2", "provider2", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service2"},
                {"service3", "provider3", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service3"},
                {"service4", "provider4", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service4"},
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
                {"service3", "provider3", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service3"},
                {"service4", "provider4", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service4"},
                {"service5", "provider5", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service5"},
                {"service6", "provider6", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service6"},
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
                {"service3", "provider3", "1.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service3"},
                {"service4", "provider4", "2.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service4"},
                {"service5", "provider5", "3.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service5"},
                {"service6", "provider6", "4.0.0", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service6"},
            },
        },
        {
            std::vector<ServiceInfo> {},
            std::vector<ServiceData> {},
        },
    };

    for (auto& testItem : testData) {
        EXPECT_TRUE(
            serviceManager.InstallServices(Array<ServiceInfo>(testItem.mInfo.data(), testItem.mInfo.size())).IsNone());

        ServiceDataStaticArray installedServices;

        EXPECT_TRUE(storage.GetAllServices(installedServices).IsNone());
        EXPECT_TRUE(TestUtils::CompareArrays(
            installedServices, Array<ServiceData>(testItem.mData.data(), testItem.mData.size())));
    }
}

TEST(ServiceManagerTest, GetImageParts)
{
    MockOCIManager ociManager;
    MockDownloader downloader;
    MockStorage    storage;

    ServiceManager serviceManager;

    EXPECT_TRUE(serviceManager.Init(ociManager, downloader, storage).IsNone());

    ServiceData serviceData = {"service0", "provider0", "2.1.0", "/aos/services/service1"};

    auto imageParts = serviceManager.GetImageParts(serviceData);
    EXPECT_TRUE(imageParts.mError.IsNone());

    EXPECT_TRUE(imageParts.mValue.mImageConfigPath == "/aos/services/service1/blobs/sha256/11111111");
    EXPECT_TRUE(imageParts.mValue.mServiceConfigPath == "/aos/services/service1/blobs/sha256/22222222");
    EXPECT_TRUE(imageParts.mValue.mServiceFSPath == "/aos/services/service1/blobs/sha256/33333333");
}

} // namespace servicemanager
} // namespace sm
} // namespace aos
