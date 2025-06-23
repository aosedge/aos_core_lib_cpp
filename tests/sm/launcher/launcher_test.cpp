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

/***********************************************************************************************************************
 * std namespace
 **********************************************************************************************************************/

namespace std {
template <>
struct hash<aos::InstanceIdent> {
    std::size_t operator()(const aos::InstanceIdent& instanceIdent) const
    {
        // Use std::string's hash function directly
        return std::hash<std::string> {}(std::string(instanceIdent.mServiceID.CStr()) + "-"
            + instanceIdent.mSubjectID.CStr() + "-" + std::to_string(instanceIdent.mInstance));
    }
};
} // namespace std

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
    std::vector<InstanceInfo>                mInstances;
    std::vector<ServiceInfo>                 mServices;
    std::vector<LayerInfo>                   mLayers;
    std::vector<InstanceStatus>              mStatus;
    std::unordered_map<InstanceIdent, Error> mErrors;
};

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class LauncherTest : public Test {
protected:
    static void SetUpTestSuite()
    {
        test::InitLog();

        LOG_INF() << "Launcher size: size=" << sizeof(Launcher);
    }

    void SetUp() override
    {
        LOG_INF() << "Set up";

        mLayerManager   = std::make_unique<LayerManagerStub>();
        mServiceManager = std::make_unique<ServiceManagerStub>();
        mOCIManager     = std::make_unique<OCISpecStub>();
        mStatusReceiver = std::make_unique<StatusReceiverStub>();
        mStorage        = std::make_unique<StorageStub>();

        mLauncher = std::make_unique<Launcher>();

        EXPECT_CALL(mNetworkManager, GetNetnsPath).WillRepeatedly(Invoke([](const String& instanceID) {
            return RetWithError<StaticString<cFilePathLen>>(fs::JoinPath("/var/run/netns", instanceID));
        }));

        EXPECT_CALL(mRunner, StartInstance)
            .WillRepeatedly(Return(RunStatus {"", InstanceRunStateEnum::eActive, ErrorEnum::eNone}));

        ASSERT_TRUE(mLauncher
                        ->Init(Config {}, mNodeInfoProvider, *mServiceManager, *mLayerManager, mResourceManager,
                            mNetworkManager, mPermHandler, mRunner, mRuntime, mResourceMonitor, *mOCIManager,
                            *mStatusReceiver, mConnectionPublisher, *mStorage)
                        .IsNone());

        ASSERT_TRUE(mLauncher->Start().IsNone());

        auto runStatus = std::make_unique<InstanceStatusStaticArray>();

        ASSERT_TRUE(mLauncher->GetCurrentRunStatus(*runStatus).IsNone());
        EXPECT_TRUE(test::CompareArrays(*runStatus, Array<InstanceStatus>()));
    }

    void TearDown() override
    {
        LOG_INF() << "Tear down";

        ASSERT_TRUE(mLauncher->Stop().IsNone());

        mLauncher.reset();
    }

    Error InstallService(const ServiceInfo& service)
    {
        auto imageSpec = std::make_unique<oci::ImageSpec>();

        imageSpec->mOS = "linux";

        auto serviceConfig = std::make_unique<ServiceConfig>();

        serviceConfig->mRunners.PushBack("runc");

        if (auto err
            = mOCIManager->SaveImageSpec(fs::JoinPath("/aos/services", service.mServiceID, "image.json"), *imageSpec);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = mOCIManager->SaveServiceConfig(
                fs::JoinPath("/aos/services", service.mServiceID, "service.json"), *serviceConfig);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    std::unique_ptr<Launcher>           mLauncher;
    NiceMock<ConnectionPublisherMock>   mConnectionPublisher;
    std::unique_ptr<LayerManagerStub>   mLayerManager;
    NiceMock<NetworkManagerMock>        mNetworkManager;
    NiceMock<NodeInfoProviderMock>      mNodeInfoProvider;
    std::unique_ptr<OCISpecStub>        mOCIManager;
    NiceMock<PermHandlerMock>           mPermHandler;
    NiceMock<ResourceManagerMock>       mResourceManager;
    NiceMock<ResourceMonitorMock>       mResourceMonitor;
    NiceMock<RunnerMock>                mRunner;
    NiceMock<RuntimeMock>               mRuntime;
    std::unique_ptr<ServiceManagerStub> mServiceManager;
    std::unique_ptr<StatusReceiverStub> mStatusReceiver;
    std::unique_ptr<StorageStub>        mStorage;
};

} // namespace

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(LauncherTest, RunInstances)
{
    std::vector<TestData> testData = {
        // start from scratch
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, 0, 0, "", "", {}},
                {{"service1", "subject0", 0}, 0, 0, "", "", {}},
                {{"service2", "subject0", 0}, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "1.0.0", 0, "", {}, 0},
                {"service1", "provider0", "1.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service2", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
            },
            {},
        },
        // start the same instances
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, 0, 0, "", "", {}},
                {{"service1", "subject0", 0}, 0, 0, "", "", {}},
                {{"service2", "subject0", 0}, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "1.0.0", 0, "", {}, 0},
                {"service1", "provider0", "1.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service1", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service2", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
            },
            {},
        },
        // stop and start some instances
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, 0, 0, "", "", {}},
                {{"service2", "subject0", 1}, 0, 0, "", "", {}},
                {{"service3", "subject0", 2}, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "1.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
                {"service3", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service2", "subject0", 1}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service3", "subject0", 2}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
            },
            {},
        },
        // new service version
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, 0, 0, "", "", {}},
                {{"service2", "subject0", 1}, 0, 0, "", "", {}},
                {{"service3", "subject0", 2}, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "2.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
                {"service3", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "2.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service2", "subject0", 1}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
                {{"service3", "subject0", 2}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
            },
            {},
        },
        // run error
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, 0, 0, "", "", {}},
                {{"service1", "subject0", 0}, 0, 0, "", "", {}},
                {{"service2", "subject0", 0}, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "1.0.0", 0, "", {}, 0},
                {"service1", "provider0", "1.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eFailed, ErrorEnum::eNotFound},
                {{"service1", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eFailed, ErrorEnum::eNotFound},
                {{"service2", "subject0", 0}, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone},
            },
            {
                {InstanceIdent {"service0", "subject0", 0}, ErrorEnum::eNotFound},
                {InstanceIdent {"service1", "subject0", 0}, ErrorEnum::eNotFound},
            },
        },
        // stop all instances
        {},
    };

    // Run instances

    auto i = 0;

    for (auto& testItem : testData) {
        LOG_INF() << "Running test case #" << i++;

        for (const auto& service : testItem.mServices) {
            ASSERT_TRUE(InstallService(service).IsNone());
        }

        auto feature = mStatusReceiver->GetFeature();

        EXPECT_CALL(mRunner, StartInstance)
            .WillRepeatedly(Invoke([this, &testItem](const String& instanceID, const String&, const RunParameters&) {
                InstanceData instanceData;

                if (auto err = mStorage->GetInstance(instanceID, instanceData); !err.IsNone()) {
                    return RunStatus {"", InstanceRunStateEnum::eFailed, AOS_ERROR_WRAP(err)};
                }

                auto runError = testItem.mErrors[instanceData.mInstanceInfo.mInstanceIdent];

                if (runError != ErrorEnum::eNone) {
                    return RunStatus {"", InstanceRunStateEnum::eFailed, runError};
                }

                return RunStatus {"", InstanceRunStateEnum::eActive, ErrorEnum::eNone};
            }));

        EXPECT_TRUE(mLauncher
                        ->RunInstances(Array<ServiceInfo>(testItem.mServices.data(), testItem.mServices.size()),
                            Array<LayerInfo>(testItem.mLayers.data(), testItem.mLayers.size()),
                            Array<InstanceInfo>(testItem.mInstances.data(), testItem.mInstances.size()))
                        .IsNone());

        EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
        EXPECT_TRUE(test::CompareArrays(
            feature.get(), Array<InstanceStatus>(testItem.mStatus.data(), testItem.mStatus.size())));
    }

    EXPECT_TRUE(mLauncher->Stop().IsNone());
}

TEST_F(LauncherTest, RunMaxInstances)
{
    TestData testItem;

    for (size_t i = 0; i < cMaxNumInstances; i++) {
        auto          serviceID = "service" + std::to_string(i % cMaxNumServices);
        InstanceIdent ident     = {serviceID.c_str(), "subject0", i / cMaxNumServices};

        testItem.mInstances.push_back({ident, 0, 0, "", "", {}});
        testItem.mStatus.push_back({ident, "1.0.0", InstanceRunStateEnum::eActive, ErrorEnum::eNone});
    }

    for (size_t i = 0; i < static_cast<size_t>(std::min(cMaxNumServices, cMaxNumInstances)); i++) {
        auto serviceID = "service" + std::to_string(i);

        testItem.mServices.push_back({serviceID.c_str(), "provider0", "1.0.0", 0, "", {}, 0});
    }

    for (const auto& service : testItem.mServices) {
        ASSERT_TRUE(InstallService(service).IsNone());
    }

    auto feature = mStatusReceiver->GetFeature();

    EXPECT_CALL(mRunner, StartInstance)
        .WillRepeatedly(Invoke([this, &testItem](const String& instanceID, const String&, const RunParameters&) {
            return RunStatus {instanceID, InstanceRunStateEnum::eActive, ErrorEnum::eNone};
        }));

    EXPECT_TRUE(mLauncher
                    ->RunInstances(Array<ServiceInfo>(testItem.mServices.data(), testItem.mServices.size()),
                        Array<LayerInfo>(testItem.mLayers.data(), testItem.mLayers.size()),
                        Array<InstanceInfo>(testItem.mInstances.data(), testItem.mInstances.size()))
                    .IsNone());

    EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
    EXPECT_TRUE(
        test::CompareArrays(feature.get(), Array<InstanceStatus>(testItem.mStatus.data(), testItem.mStatus.size())));

    EXPECT_TRUE(mLauncher->Stop().IsNone());
}

} // namespace aos::sm::launcher
