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
#include "mocks/launchermock.hpp"
#include "mocks/monitoringmock.hpp"
#include "mocks/networkmanagermock.hpp"
#include "mocks/nodeinfoprovidermock.hpp"
#include "mocks/permhandlermock.hpp"
#include "mocks/resourcemanagermock.hpp"
#include "mocks/runnermock.hpp"

#include "stubs/launcherstub.hpp"
#include "stubs/layermanagerstub.hpp"
#include "stubs/ocispecstub.hpp"
#include "stubs/servicemanagerstub.hpp"

using namespace aos::monitoring;
using namespace aos::oci;
using namespace aos::iam::nodeinfoprovider;
using namespace aos::iam::permhandler;
using namespace aos::sm::layermanager;
using namespace aos::sm::networkmanager;
using namespace aos::sm::resourcemanager;
using namespace aos::sm::runner;
using namespace aos::sm::servicemanager;
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
    std::vector<InstanceInfo>   mInstances;
    std::vector<ServiceInfo>    mServices;
    std::vector<LayerInfo>      mLayers;
    std::vector<InstanceStatus> mStatus;
};

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class LauncherTest : public Test {
protected:
    void SetUp() override
    {
        test::InitLog();

        EXPECT_CALL(mRunner, StartInstance)
            .WillRepeatedly(Return(RunStatus {"", InstanceRunStateEnum::eActive, ErrorEnum::eNone}));

        EXPECT_CALL(mNetworkManager, GetNetnsPath)
            .WillRepeatedly(Return(RetWithError<StaticString<cFilePathLen>>("/var/run/netns")));
    }

    ConnectionPublisherMock mConnectionPublisher;
    LayerManagerStub        mLayerManager;
    NetworkManagerMock      mNetworkManager;
    NodeInfoProviderMock    mNodeInfoProvider;
    OCISpecStub             mOCIManager;
    PermHandlerMock         mPermHandler;
    ResourceManagerMock     mResourceManager;
    ResourceMonitorMock     mResourceMonitor;
    RunnerMock              mRunner;
    RuntimeMock             mRuntime;
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

    LOG_INF() << "Launcher size: size=" << sizeof(Launcher);

    ASSERT_TRUE(launcher
                    ->Init(Config {}, mNodeInfoProvider, mServiceManager, mLayerManager, mResourceManager,
                        mNetworkManager, mPermHandler, mRunner, mRuntime, mResourceMonitor, mOCIManager,
                        mStatusReceiver, mConnectionPublisher, mStorage)
                    .IsNone());

    ASSERT_TRUE(launcher->Start().IsNone());

    launcher->OnConnect();

    // Get initial instance status

    auto runStatus = std::make_unique<InstanceStatusStaticArray>();

    EXPECT_TRUE(launcher->GetCurrentRunStatus(*runStatus).IsNone());
    EXPECT_TRUE(test::CompareArrays(*runStatus, Array<InstanceStatus>()));

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

        auto feature = mStatusReceiver.GetFeature();

        auto imageSpec = std::make_unique<oci::ImageSpec>();

        imageSpec->mOS = "linux";

        auto serviceConfig = std::make_unique<ServiceConfig>();

        serviceConfig->mRunners.PushBack("runc");

        for (const auto& service : testItem.mServices) {
            ASSERT_TRUE(
                mOCIManager.SaveImageSpec(FS::JoinPath("/aos/services", service.mServiceID, "image.json"), *imageSpec)
                    .IsNone());
            ASSERT_TRUE(mOCIManager
                            .SaveServiceConfig(
                                FS::JoinPath("/aos/services", service.mServiceID, "service.json"), *serviceConfig)
                            .IsNone());
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

    EXPECT_TRUE(launcher->Stop().IsNone());
}

} // namespace aos::sm::launcher
