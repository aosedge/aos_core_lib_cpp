/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <chrono>
#include <future>

#include <gtest/gtest.h>

#include "aos/common/monitoring/monitoring.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/log.hpp"
#include "aos/sm/launcher.hpp"

#include "aos/test/log.hpp"
#include "aos/test/utils.hpp"

using namespace aos::sm::runner;
using namespace aos::sm::servicemanager;

namespace aos {
namespace sm {
namespace launcher {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

static constexpr auto cWaitStatusTimeout = std::chrono::seconds(5);

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct TestData {
    std::vector<InstanceInfo>   mInstances;
    std::vector<ServiceInfo>    mServices;
    std::vector<LayerInfo>      mLayers;
    std::vector<InstanceStatus> mStatus;
};

/***********************************************************************************************************************
 * Vars
 **********************************************************************************************************************/

static std::mutex sLogMutex;

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

/**
 * Mocks resource monitor.
 */
class MockResourceMonitor : public monitoring::ResourceMonitorItf {
public:
    Error StartInstanceMonitoring(
        const String& instanceID, const monitoring::InstanceMonitorParams& monitoringConfig) override
    {
        (void)instanceID;
        (void)monitoringConfig;

        return ErrorEnum::eNone;
    }

    Error StopInstanceMonitoring(const String& instanceID) override
    {
        (void)instanceID;

        return ErrorEnum::eNone;
    }

    Error GetAverageMonitoringData(monitoring::NodeMonitoringData& monitoringData) override
    {
        (void)monitoringData;

        return ErrorEnum::eNone;
    }
};

/**
 * Mocks service manager.
 */
class MockServiceManager : public ServiceManagerItf {
public:
    Error InstallServices(const Array<ServiceInfo>& services) override
    {
        std::lock_guard lock {mMutex};

        LOG_DBG() << "Install services has been called";

        mServicesData.clear();

        std::transform(
            services.begin(), services.end(), std::back_inserter(mServicesData), [](const ServiceInfo& service) {
                return ServiceData {service.mServiceID, service.mProviderID, service.mVersion,
                    FS::JoinPath("/aos/storages", service.mServiceID), "", Time::Now(), ServiceStateEnum::eActive, 0,
                    0};
            });

        return ErrorEnum::eNone;
    }

    RetWithError<ServiceData> GetService(const String& serviceID) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mServicesData.begin(), mServicesData.end(),
            [&serviceID](const ServiceData& service) { return service.mServiceID == serviceID; });
        if (it == mServicesData.end()) {
            return {ServiceData(), ErrorEnum::eNotFound};
        }

        return *it;
    }

    Error GetAllServices(Array<ServiceData>& services) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& service : mServicesData) {
            services.PushBack(service);
        }

        return ErrorEnum::eNone;
    }

    RetWithError<ImageParts> GetImageParts(const ServiceData& service) override
    {
        return ImageParts {FS::JoinPath(service.mImagePath, "image.json"),
            FS::JoinPath(service.mImagePath, "service.json"), service.mImagePath};
    }

private:
    std::mutex               mMutex;
    std::vector<ServiceData> mServicesData;
};

/**
 * Mocks runner.
 */
class MockRunner : public RunnerItf {
public:
    RunStatus StartInstance(const String& instanceID, const String& runtimeDir) override
    {
        (void)instanceID;
        (void)runtimeDir;

        return RunStatus {};
    }

    Error StopInstance(const String& instanceID) override
    {
        (void)instanceID;

        return ErrorEnum::eNone;
    }
};

/**
 * Mock OCI manager.
 */

class MockOCIManager : public OCISpecItf {
public:
    Error LoadImageManifest(const String& path, oci::ImageManifest& manifest) override
    {
        (void)path;
        (void)manifest;

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

        imageSpec.mConfig.mEntryPoint.EmplaceBack("unikernel");

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
 * Mocks status receiver.
 */
class MockStatusReceiver : public InstanceStatusReceiverItf {
public:
    auto GetFeature()
    {
        mPromise = std::promise<const Array<InstanceStatus>>();

        return mPromise.get_future();
    }

    Error InstancesRunStatus(const Array<InstanceStatus>& status) override
    {
        mPromise.set_value(status);

        return ErrorEnum::eNone;
    }

    Error InstancesUpdateStatus(const Array<InstanceStatus>& status) override
    {
        mPromise.set_value(status);

        return ErrorEnum::eNone;
    }

private:
    std::promise<const Array<InstanceStatus>> mPromise;
};

/**
 * Mocks storage.
 */
class MockStorage : public sm::launcher::StorageItf {
public:
    Error AddInstance(const InstanceData& instance) override
    {
        std::lock_guard lock {mMutex};

        if (std::find_if(mInstances.begin(), mInstances.end(),
                [&instance](const InstanceData& info) { return instance == info; })
            != mInstances.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mInstances.push_back(instance);

        return ErrorEnum::eNone;
    }

    Error UpdateInstance(const InstanceData& instance) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mInstances.begin(), mInstances.end(),
            [&instance](const auto& info) { return instance.mInstanceID == info.mInstanceID; });
        if (it == mInstances.end()) {
            return ErrorEnum::eNotFound;
        }

        *it = instance;

        return ErrorEnum::eNone;
    }

    Error RemoveInstance(const String& instanceID) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mInstances.begin(), mInstances.end(),
            [&instanceID](const auto& instance) { return instance.mInstanceID == instanceID; });
        if (it == mInstances.end()) {
            return ErrorEnum::eNotFound;
        }

        mInstances.erase(it);

        return ErrorEnum::eNone;
    }

    Error GetAllInstances(Array<InstanceData>& instances) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& instance : mInstances) {
            auto err = instances.PushBack(instance);
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    RetWithError<uint64_t> GetOperationVersion() const override
    {
        std::lock_guard lock {mMutex};

        return {mOperationVersion, ErrorEnum::eNone};
    }

    Error SetOperationVersion(uint64_t version) override
    {
        std::lock_guard lock {mMutex};

        mOperationVersion = version;

        return ErrorEnum::eNone;
    }

    Error GetOverrideEnvVars(cloudprotocol::EnvVarsInstanceInfoArray& envVarsInstanceInfos) const override
    {
        std::lock_guard lock {mMutex};

        envVarsInstanceInfos = mEnvVarsInstanceInfos;

        return ErrorEnum::eNone;
    }

    Error SetOverrideEnvVars(const cloudprotocol::EnvVarsInstanceInfoArray& envVarsInstanceInfos) override
    {
        std::lock_guard lock {mMutex};

        mEnvVarsInstanceInfos = envVarsInstanceInfos;

        return ErrorEnum::eNone;
    }

    RetWithError<Time> GetOnlineTime() const override
    {
        std::lock_guard lock {mMutex};

        return {mOnlineTime, ErrorEnum::eNone};
    }

    Error SetOnlineTime(const Time& time) override
    {
        std::lock_guard lock {mMutex};

        mOnlineTime = time;

        return ErrorEnum::eNone;
    }

private:
    std::vector<InstanceData> mInstances;
    mutable std::mutex        mMutex;

    uint64_t                                mOperationVersion = launcher::Launcher::cOperationVersion;
    cloudprotocol::EnvVarsInstanceInfoArray mEnvVarsInstanceInfos;
    Time                                    mOnlineTime = Time::Now();
};

/**
 * Mocks connection publisher.
 */

class MockConnectionPublisher : public ConnectionPublisherItf {
public:
    aos::Error Subscribe(ConnectionSubscriberItf& subscriber) override
    {
        mSubscriber = &subscriber;
        return ErrorEnum::eNone;
    }

    void Unsubscribe(ConnectionSubscriberItf& subscriber) override
    {
        (void)subscriber;
        mSubscriber = nullptr;
    }

    ConnectionSubscriberItf* GetSubscriber() const { return mSubscriber; }

    void Connect()
    {
        if (mSubscriber) {
            mSubscriber->OnConnect();
        }
    }

    void Disconnect()
    {
        if (mSubscriber) {
            mSubscriber->OnDisconnect();
        }
    }

private:
    ConnectionSubscriberItf* mSubscriber = nullptr;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST(LauncherTest, RunInstances)
{
    MockServiceManager      serviceManager;
    MockRunner              runner;
    MockOCIManager          ociManager;
    MockStatusReceiver      statusReceiver;
    MockStorage             storage;
    MockResourceMonitor     resourceMonitor;
    MockConnectionPublisher connectionPublisher;

    Launcher launcher;

    InitLog();

    auto feature = statusReceiver.GetFeature();

    EXPECT_TRUE(
        launcher.Init(serviceManager, runner, ociManager, statusReceiver, storage, resourceMonitor, connectionPublisher)
            .IsNone());

    ASSERT_TRUE(launcher.Start().IsNone());

    connectionPublisher.Connect();

    // Wait for initial instance status

    EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
    EXPECT_TRUE(TestUtils::CompareArrays(feature.get(), Array<InstanceStatus>()));

    // Test different scenarios

    struct TestData {
        std::vector<InstanceInfo>   mInstances;
        std::vector<ServiceInfo>    mServices;
        std::vector<LayerInfo>      mLayers;
        std::vector<InstanceStatus> mStatus;
    };

    std::vector<TestData> testData = {
        // Run instances first time
        {
            std::vector<InstanceInfo> {
                {{"service1", "subject1", 0}, {}, 0, 0, "", ""},
                {{"service1", "subject1", 1}, {}, 0, 0, "", ""},
                {{"service1", "subject1", 2}, {}, 0, 0, "", ""},
            },
            std::vector<ServiceInfo> {
                {"service1", "provider1", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service1", "subject1", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject1", 1}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject1", 2}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
            },
        },
        // Empty instances
        {
            {},
            {},
            {},
            {},
        },
        // Another instances round
        {
            std::vector<InstanceInfo> {
                {{"service1", "subject1", 4}, {}, 0, 0, "", ""},
                {{"service1", "subject1", 5}, {}, 0, 0, "", ""},
                {{"service1", "subject1", 6}, {}, 0, 0, "", ""},
            },
            std::vector<ServiceInfo> {
                {"service1", "provider1", "2.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service1", "subject1", 4}, "2.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject1", 5}, "2.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject1", 6}, "2.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
            },
        },
    };

    // Run instances

    for (auto& testItem : testData) {
        LOG_INF() << "Test run instances: iteration=" << &testItem - &testData.front();

        feature = statusReceiver.GetFeature();

        EXPECT_TRUE(launcher
                        .RunInstances(Array<ServiceInfo>(testItem.mServices.data(), testItem.mServices.size()),
                            Array<LayerInfo>(testItem.mLayers.data(), testItem.mLayers.size()),
                            Array<InstanceInfo>(testItem.mInstances.data(), testItem.mInstances.size()))
                        .IsNone());

        EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
        EXPECT_TRUE(TestUtils::CompareArrays(
            feature.get(), Array<InstanceStatus>(testItem.mStatus.data(), testItem.mStatus.size())));
    }

    // Reset

    feature = statusReceiver.GetFeature();

    connectionPublisher.Connect();

    // Wait for initial instance status

    EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
    EXPECT_TRUE(TestUtils::CompareArrays(
        feature.get(), Array<InstanceStatus>(testData.back().mStatus.data(), testData.back().mStatus.size())));

    EXPECT_TRUE(launcher.Stop().IsNone());
}

} // namespace launcher
} // namespace sm
} // namespace aos
