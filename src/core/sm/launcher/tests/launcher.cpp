/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/crypto/cryptoprovider.hpp>
#include <core/common/tests/mocks/connectionprovidermock.hpp>
#include <core/common/tests/mocks/monitoringmock.hpp>
#include <core/common/tests/mocks/permhandlermock.hpp>
#include <core/common/tests/stubs/ocispecstub.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/iam/tests/mocks/nodeinfoprovidermock.hpp>
#include <core/sm/launcher/launcher.hpp>
#include <core/sm/tests/mocks/launchermock.hpp>
#include <core/sm/tests/mocks/networkmanagermock.hpp>
#include <core/sm/tests/mocks/resourcemanagermock.hpp>
#include <core/sm/tests/mocks/runnermock.hpp>
#include <core/sm/tests/stubs/launcherstub.hpp>
#include <core/sm/tests/stubs/layermanagerstub.hpp>
#include <core/sm/tests/stubs/servicemanagerstub.hpp>

using namespace aos::iam::nodeinfoprovider;
using namespace aos::iamclient;
using namespace aos::monitoring;
using namespace aos::oci;
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
        return std::hash<std::string> {}(std::string(instanceIdent.mItemID.CStr()) + "-"
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
        tests::utils::InitLog();

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
        mCryptoProvider = std::make_unique<crypto::DefaultCryptoProvider>();

        mLauncher = std::make_unique<Launcher>();

        ASSERT_TRUE(mCryptoProvider->Init().IsNone()) << "crypto provider initialization failed";

        EXPECT_CALL(mNetworkManager, GetNetnsPath).WillRepeatedly(Invoke([](const String& instanceID) {
            return RetWithError<StaticString<cFilePathLen>>(fs::JoinPath("/var/run/netns", instanceID));
        }));

        EXPECT_CALL(mRunner, StartInstance)
            .WillRepeatedly(Return(RunStatus {"", InstanceStateEnum::eActive, ErrorEnum::eNone}));

        ASSERT_TRUE(mLauncher
                        ->Init(Config {}, mNodeInfoProvider, *mServiceManager, *mLayerManager, mResourceManager,
                            mNetworkManager, mPermHandler, mRunner, mRuntime, mResourceMonitor, *mOCIManager,
                            *mStatusReceiver, mConnectionPublisher, *mStorage, *mCryptoProvider)
                        .IsNone());

        ASSERT_TRUE(mLauncher->Start().IsNone());

        auto runStatus = std::make_unique<InstanceStatusArray>();

        ASSERT_TRUE(mLauncher->GetCurrentRunStatus(*runStatus).IsNone());
        EXPECT_TRUE(tests::utils::CompareArrays(*runStatus, Array<InstanceStatus>()));
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

    std::unique_ptr<Launcher>                      mLauncher;
    NiceMock<ConnectionPublisherMock>              mConnectionPublisher;
    std::unique_ptr<LayerManagerStub>              mLayerManager;
    NiceMock<NetworkManagerMock>                   mNetworkManager;
    NiceMock<NodeInfoProviderMock>                 mNodeInfoProvider;
    std::unique_ptr<OCISpecStub>                   mOCIManager;
    NiceMock<PermHandlerMock>                      mPermHandler;
    NiceMock<ResourceManagerMock>                  mResourceManager;
    NiceMock<ResourceMonitorMock>                  mResourceMonitor;
    NiceMock<RunnerMock>                           mRunner;
    NiceMock<RuntimeMock>                          mRuntime;
    std::unique_ptr<ServiceManagerStub>            mServiceManager;
    std::unique_ptr<StatusReceiverStub>            mStatusReceiver;
    std::unique_ptr<StorageStub>                   mStorage;
    std::unique_ptr<crypto::DefaultCryptoProvider> mCryptoProvider;
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
                {{"service0", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service1", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service2", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "1.0.0", 0, "", {}, 0},
                {"service1", "provider0", "1.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
                {{"service1", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
                {{"service2", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
            },
            {},
        },
        // start the same instances
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service1", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service2", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "1.0.0", 0, "", {}, 0},
                {"service1", "provider0", "1.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
                {{"service1", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
                {{"service2", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
            },
            {},
        },
        // stop and start some instances
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service2", "subject0", 1}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service3", "subject0", 2}, "image1", "", 0, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "1.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
                {"service3", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
                {{"service2", "subject0", 1}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
                {{"service3", "subject0", 2}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
            },
            {},
        },
        // new service version
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service2", "subject0", 1}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service3", "subject0", 2}, "image1", "", 0, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "2.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
                {"service3", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "2.0.0"},
                {{"service2", "subject0", 1}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
                {{"service3", "subject0", 2}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
            },
            {},
        },
        // run error
        {
            std::vector<InstanceInfo> {
                {{"service0", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service1", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
                {{"service2", "subject0", 0}, "image1", "", 0, 0, 0, "", "", {}},
            },
            std::vector<ServiceInfo> {
                {"service0", "provider0", "1.0.0", 0, "", {}, 0},
                {"service1", "provider0", "1.0.0", 0, "", {}, 0},
                {"service2", "provider0", "1.0.0", 0, "", {}, 0},
            },
            {},
            std::vector<InstanceStatus> {
                {{"service0", "subject0", 0}, "", "", {}, InstanceStateEnum::eFailed, ErrorEnum::eNotFound, "1.0.0"},
                {{"service1", "subject0", 0}, "", "", {}, InstanceStateEnum::eFailed, ErrorEnum::eNotFound, "1.0.0"},
                {{"service2", "subject0", 0}, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"},
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
                    return RunStatus {"", InstanceStateEnum::eFailed, AOS_ERROR_WRAP(err)};
                }

                auto runError = testItem.mErrors[instanceData.mInstanceInfo];

                if (runError != ErrorEnum::eNone) {
                    return RunStatus {"", InstanceStateEnum::eFailed, runError};
                }

                return RunStatus {"", InstanceStateEnum::eActive, ErrorEnum::eNone};
            }));

        EXPECT_TRUE(mLauncher
                        ->RunInstances(Array<ServiceInfo>(testItem.mServices.data(), testItem.mServices.size()),
                            Array<LayerInfo>(testItem.mLayers.data(), testItem.mLayers.size()),
                            Array<InstanceInfo>(testItem.mInstances.data(), testItem.mInstances.size()))
                        .IsNone());

        EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
        EXPECT_TRUE(tests::utils::CompareArrays(
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

        testItem.mInstances.push_back({ident, "", "image1", 0, 0, 0, "", "", {}});
        testItem.mStatus.push_back({ident, "", "", {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0"});
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
            return RunStatus {instanceID, InstanceStateEnum::eActive, ErrorEnum::eNone};
        }));

    EXPECT_TRUE(mLauncher
                    ->RunInstances(Array<ServiceInfo>(testItem.mServices.data(), testItem.mServices.size()),
                        Array<LayerInfo>(testItem.mLayers.data(), testItem.mLayers.size()),
                        Array<InstanceInfo>(testItem.mInstances.data(), testItem.mInstances.size()))
                    .IsNone());

    EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
    EXPECT_TRUE(tests::utils::CompareArrays(
        feature.get(), Array<InstanceStatus>(testItem.mStatus.data(), testItem.mStatus.size())));

    EXPECT_TRUE(mLauncher->Stop().IsNone());
}

} // namespace aos::sm::launcher
