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

#include "mocks/connectionsubscmock.hpp"
#include "mocks/networkmanagermock.hpp"

#include "stubs/launcherstub.hpp"
#include "stubs/layermanagerstub.hpp"
#include "stubs/monitoringstub.hpp"
#include "stubs/ocispecstub.hpp"
#include "stubs/runnerstub.hpp"
#include "stubs/servicemanagerstub.hpp"

using namespace aos::monitoring;
using namespace aos::oci;
using namespace aos::sm::layermanager;
using namespace aos::sm::runner;
using namespace aos::sm::servicemanager;
using namespace aos::sm::networkmanager;

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

} // namespace

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST(LauncherTest, RunInstances)
{
    auto connectionPublisher = std::make_unique<ConnectionPublisherMock>();
    auto layerManager        = std::make_unique<LayerManagerStub>();
    auto networkManager      = std::make_unique<NetworkManagerMock>();
    auto ociManager          = std::make_unique<OCISpecStub>();
    auto resourceMonitor     = std::make_unique<ResourceMonitorStub>();
    auto runner              = std::make_unique<RunnerStub>();
    auto serviceManager      = std::make_unique<ServiceManagerStub>();
    auto statusReceiver      = std::make_unique<StatusReceiverStub>();
    auto storage             = std::make_unique<StorageStub>();

    auto launcher = std::make_unique<Launcher>();

    test::InitLog();

    auto feature = statusReceiver->GetFeature();

    EXPECT_TRUE(launcher
                    ->Init(Config {}, *serviceManager, *layerManager, *networkManager, *runner, *resourceMonitor,
                        *ociManager, *statusReceiver, *connectionPublisher, *storage)
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

    auto i = 0;

    for (auto& testItem : testData) {
        LOG_INF() << "Running test case #" << i++;

        feature = statusReceiver->GetFeature();

        auto imageSpec = std::make_unique<oci::ImageSpec>();

        imageSpec->mConfig.mEntryPoint.PushBack("unikernel");

        for (const auto& service : testItem.mServices) {
            ociManager->SaveImageSpec(FS::JoinPath("/aos/services", service.mServiceID, "image.json"), *imageSpec);
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

    feature = statusReceiver->GetFeature();

    launcher->OnConnect();

    // Wait for initial instance status

    EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
    EXPECT_TRUE(test::CompareArrays(
        feature.get(), Array<InstanceStatus>(testData.back().mStatus.data(), testData.back().mStatus.size())));

    EXPECT_TRUE(launcher->Stop().IsNone());
}

} // namespace aos::sm::launcher
