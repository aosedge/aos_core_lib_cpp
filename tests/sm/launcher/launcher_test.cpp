/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/sm/launcher.hpp"

#include "aos/test/log.hpp"
#include "aos/test/utils.hpp"

#include "mocks/connectionsubscmock.hpp"
#include "mocks/networkmanagermock.hpp"
#include "mocks/runnermock.hpp"

#include "stubs/launcherstub.hpp"
#include "stubs/layermanagerstub.hpp"
#include "stubs/monitoringstub.hpp"
#include "stubs/ocispecstub.hpp"
#include "stubs/servicemanagerstub.hpp"

using namespace aos::monitoring;
using namespace aos::oci;
using namespace aos::sm::layermanager;
using namespace aos::sm::runner;
using namespace aos::sm::servicemanager;
using namespace aos::sm::networkmanager;
using namespace testing;

namespace aos::sm::launcher {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cWaitStatusTimeout = std::chrono::seconds(5);

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct TestData {
    std::vector<aos::InstanceInfo>   mInstances;
    std::vector<aos::ServiceInfo>    mServices;
    std::vector<aos::LayerInfo>      mLayers;
    std::vector<aos::InstanceStatus> mStatus;
};

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class LauncherTest : public Test {
protected:
    void SetUp() override { aos::test::InitLog(); }

    ConnectionPublisherMock mConnectionPublisher;
    LayerManagerStub        mLayerManager;
    NetworkManagerMock      mNetworkManager;
    OCISpecStub             mOCIManager;
    ResourceMonitorStub     mResourceMonitor;
    RunnerMock              mRunner;
    ServiceManagerStub      mServiceManager;
    StatusReceiverStub      mStatusReceiver;
    StorageStub             mStorage;
};

} // namespace

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(LauncherTest, RunInstances)
{
    auto launcher = std::make_unique<Launcher>();

    test::InitLog();

    auto feature = mStatusReceiver.GetFeature();

    EXPECT_TRUE(launcher
                    ->Init(Config {}, mServiceManager, mLayerManager, mNetworkManager, mRunner, mResourceMonitor,
                        mOCIManager, mStatusReceiver, mConnectionPublisher, mStorage)
                    .IsNone());

    ASSERT_TRUE(launcher->Start().IsNone());

    launcher->OnConnect();

    // Wait for initial instance status

    EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
    EXPECT_TRUE(test::CompareArrays(feature.get(), Array<InstanceStatus>()));

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
                {{"service1", "subject1", 0}, 0, 0, "", "", {}},
                {{"service1", "subject1", 1}, 0, 0, "", "", {}},
                {{"service1", "subject1", 2}, 0, 0, "", "", {}},
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
                {{"service1", "subject1", 4}, 0, 0, "", "", {}},
                {{"service1", "subject1", 5}, 0, 0, "", "", {}},
                {{"service1", "subject1", 6}, 0, 0, "", "", {}},
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

    EXPECT_CALL(mRunner, StartInstance)
        .WillRepeatedly(Return(RunStatus {"", InstanceRunStateEnum::eActive, ErrorEnum::eNone}));

    auto i = 0;

    for (auto& testItem : testData) {
        LOG_INF() << "Running test case #" << i++;

        feature = mStatusReceiver.GetFeature();

        auto imageSpec = std::make_unique<oci::ImageSpec>();

        imageSpec->mConfig.mEntryPoint.PushBack("unikernel");

        for (const auto& service : testItem.mServices) {
            mOCIManager.SaveImageSpec(FS::JoinPath("/aos/services", service.mServiceID, "image.json"), *imageSpec);
        }

        EXPECT_TRUE(launcher
                        ->RunInstances(Array<ServiceInfo>(testItem.mServices.data(), testItem.mServices.size()),
                            Array<LayerInfo>(testItem.mLayers.data(), testItem.mLayers.size()),
                            Array<InstanceInfo>(testItem.mInstances.data(), testItem.mInstances.size()))
                        .IsNone());

        EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
        EXPECT_TRUE(test::CompareArrays(
            feature.get(), Array<InstanceStatus>(testItem.mStatus.data(), testItem.mStatus.size())));
    }

    // Reset

    feature = mStatusReceiver.GetFeature();

    launcher->OnConnect();

    // Wait for initial instance status

    EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
    EXPECT_TRUE(test::CompareArrays(
        feature.get(), Array<InstanceStatus>(testData.back().mStatus.data(), testData.back().mStatus.size())));

    EXPECT_TRUE(launcher->Stop().IsNone());
}

} // namespace aos::sm::launcher
