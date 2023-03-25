/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <future>

#include <gtest/gtest.h>

#include "aos/common/tools/error.hpp"
#include "aos/common/tools/log.hpp"
#include "aos/sm/launcher.hpp"

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
 * Mocks service manager.
 */
class MockServiceManager : public ServiceManagerItf {
public:
    Error InstallServices(const Array<ServiceInfo>& services) override
    {
        (void)services;

        return ErrorEnum::eNone;
    }

    RetWithError<ServiceData> GetService(const String serviceID) override
    {
        (void)serviceID;

        return ServiceData {};
    }

    RetWithError<ImageParts> GetImageParts(const ServiceData& service) override
    {
        (void)service;

        return ImageParts {};
    }
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
    Error AddInstance(const InstanceInfo& instance) override
    {
        (void)instance;

        return ErrorEnum::eNone;
    }

    Error UpdateInstance(const InstanceInfo& instance) override
    {
        (void)instance;

        return ErrorEnum::eNone;
    }

    Error RemoveInstance(const InstanceIdent& instanceIdent) override
    {
        (void)instanceIdent;

        return ErrorEnum::eNone;
    }

    Error GetAllInstances(Array<InstanceInfo>& instances) override
    {
        (void)instances;

        return ErrorEnum::eNone;
    }
};

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

bool CompareInstanceStatuses(const Array<InstanceStatus> status1, const Array<InstanceStatus> status2)
{
    if (status1.Size() != status2.Size()) {
        return false;
    }

    for (const auto& instance : status1) {
        if (!status2.Find(instance).mError.IsNone()) {
            return false;
        }
    }

    for (const auto& instance : status2) {
        if (!status1.Find(instance).mError.IsNone()) {
            return false;
        }
    }

    return true;
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST(launcher, RunInstances)
{
    MockServiceManager serviceManager;
    MockRunner         runner;
    MockStatusReceiver statusReceiver;
    MockStorage        storage;

    Launcher launcher;

    Log::SetCallback([](LogModule module, LogLevel level, const String& message) {
        std::lock_guard<std::mutex> lock(sLogMutex);

        std::cout << level.ToString().CStr() << " | " << module.ToString().CStr() << " | " << message.CStr()
                  << std::endl;
    });

    EXPECT_TRUE(launcher.Init(serviceManager, runner, statusReceiver, storage).IsNone());

    std::vector<TestData> testData = {
        // Run instances first time
        {
            std::vector<InstanceInfo> {
                {{"service1", "subject1", 0}, 0, 0, "", ""},
                {{"service1", "subject1", 1}, 0, 0, "", ""},
                {{"service1", "subject1", 2}, 0, 0, "", ""},
            },
            {},
            {},
            std::vector<InstanceStatus> {
                {{"service1", "subject1", 0}, 0, InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject1", 1}, 0, InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject1", 2}, 0, InstanceRunStateEnum::eActive, ErrorEnum::eNone},
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
                {{"service1", "subject1", 4}, 0, 0, "", ""},
                {{"service1", "subject1", 5}, 0, 0, "", ""},
                {{"service1", "subject1", 6}, 0, 0, "", ""},
            },
            {},
            {},
            std::vector<InstanceStatus> {
                {{"service1", "subject1", 4}, 0, InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject1", 5}, 0, InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject1", 6}, 0, InstanceRunStateEnum::eActive, ErrorEnum::eNone},
            },
        },
    };

    // Run instances

    for (auto& testItem : testData) {
        auto feature = statusReceiver.GetFeature();

        EXPECT_TRUE(launcher
                        .RunInstances(Array<ServiceInfo>(testItem.mServices.data(), testItem.mServices.size()),
                            Array<LayerInfo>(testItem.mLayers.data(), testItem.mLayers.size()),
                            Array<InstanceInfo>(testItem.mInstances.data(), testItem.mInstances.size()))
                        .IsNone());

        EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
        EXPECT_TRUE(CompareInstanceStatuses(
            feature.get(), Array<InstanceStatus>(testItem.mStatus.data(), testItem.mStatus.size())));
    }
}

} // namespace launcher
} // namespace sm
} // namespace aos
