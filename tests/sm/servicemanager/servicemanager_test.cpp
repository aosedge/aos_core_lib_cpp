/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>

#include <gtest/gtest.h>

#include "aos/sm/servicemanager.hpp"

#include "tests/utils/utils.hpp"

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
class MockStorage1 : public StorageItf {
public:
    Error AddService(const ServiceData& service) override
    {
        std::lock_guard<std::mutex> lock(mMutex);

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
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = std::find_if(mServices.begin(), mServices.end(),
            [&serviceID](const ServiceData& data) { return serviceID == data.mServiceID; });

        if (it == mServices.end()) {
            return {*mServices.begin(), ErrorEnum::eNotFound};
        }

        return *it;
    }

    Error UpdateService(const ServiceData& service) override
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = std::find_if(mServices.begin(), mServices.end(),
            [&service](const ServiceData& data) { return service.mServiceID == data.mServiceID; });

        if (it == mServices.end()) {
            return ErrorEnum::eNotFound;
        }

        *it = service;

        return ErrorEnum::eNone;
    }

    Error RemoveService(const String& serviceID, uint64_t aosVersion) override
    {
        std::lock_guard<std::mutex> lock(mMutex);

        (void)aosVersion;

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
        std::lock_guard<std::mutex> lock(mMutex);

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
 * Tests
 **********************************************************************************************************************/

TEST(ServiceManagerTest, InstallServices)
{
    MockDownloader downloader;
    MockStorage1   storage;

    ServiceManager serviceManager;

    Log::SetCallback([](LogModule module, LogLevel level, const String& message) {
        std::lock_guard<std::mutex> lock(sLogMutex);

        std::cout << level.ToString().CStr() << " | " << module.ToString().CStr() << " | " << message.CStr()
                  << std::endl;
    });

    EXPECT_TRUE(serviceManager.Init(downloader, storage).IsNone());

    std::vector<TestData> testData = {
        {
            std::vector<ServiceInfo> {
                {{1, "", ""}, "service1", "provider1", 0, "url", {}, {}, 0},
                {{1, "", ""}, "service2", "provider2", 0, "url", {}, {}, 0},
                {{1, "", ""}, "service3", "provider3", 0, "url", {}, {}, 0},
                {{1, "", ""}, "service4", "provider4", 0, "url", {}, {}, 0},
            },
            std::vector<ServiceData> {
                {{1, "", ""}, "service1", "provider1", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service1"},
                {{1, "", ""}, "service2", "provider2", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service2"},
                {{1, "", ""}, "service3", "provider3", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service3"},
                {{1, "", ""}, "service4", "provider4", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service4"},
            },
        },
        {
            std::vector<ServiceInfo> {
                {{1, "", ""}, "service3", "provider3", 0, "url", {}, {}, 0},
                {{1, "", ""}, "service4", "provider4", 0, "url", {}, {}, 0},
                {{1, "", ""}, "service5", "provider5", 0, "url", {}, {}, 0},
                {{1, "", ""}, "service6", "provider5", 0, "url", {}, {}, 0},
            },
            std::vector<ServiceData> {
                {{1, "", ""}, "service3", "provider3", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service3"},
                {{1, "", ""}, "service4", "provider4", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service4"},
                {{1, "", ""}, "service5", "provider5", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service5"},
                {{1, "", ""}, "service6", "provider6", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service6"},
            },
        },
        {
            std::vector<ServiceInfo> {
                {{1, "", ""}, "service3", "provider3", 0, "url", {}, {}, 0},
                {{2, "", ""}, "service4", "provider4", 0, "url", {}, {}, 0},
                {{3, "", ""}, "service5", "provider5", 0, "url", {}, {}, 0},
                {{4, "", ""}, "service6", "provider5", 0, "url", {}, {}, 0},
            },
            std::vector<ServiceData> {
                {{1, "", ""}, "service3", "provider3", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service3"},
                {{2, "", ""}, "service4", "provider4", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service4"},
                {{3, "", ""}, "service5", "provider5", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service5"},
                {{4, "", ""}, "service6", "provider6", AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR "/service6"},
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
} // namespace servicemanager
} // namespace sm
} // namespace aos
