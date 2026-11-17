/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/mocks/iteminfoprovidermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/sm/launcher/instancelauncher.hpp>
#include <core/sm/tests/mocks/networkmanagermock.hpp>
#include <core/sm/tests/mocks/resourcemanagermock.hpp>

#include "mocks/runtimemock.hpp"
#include "stubs/storagestub.hpp"

using namespace testing;

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class InstanceLauncherTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        RuntimeInfo runtimeInfo;

        runtimeInfo.mRuntimeID = "runtime0";

        ON_CALL(mRuntime, GetRuntimeInfo).WillByDefault(DoAll(SetArgReferee<0>(runtimeInfo), Return(ErrorEnum::eNone)));

        StaticArray<RuntimeItf*, cMaxNumNodeRuntimes> runtimes;

        runtimes.PushBack(&mRuntime);

        ASSERT_TRUE(mRuntimes.Init(mAllocator, runtimes).IsNone());
        ASSERT_TRUE(mRuntimes.Start().IsNone());

        mManifest = std::make_unique<oci::ImageManifest>();
        mManifest->mItemConfig.EmplaceValue();

        mItemConfig              = std::make_unique<oci::ItemConfig>();
        mItemConfig->mOfflineTTL = 10 * Time::cSeconds;

        ON_CALL(mItemInfoProvider, GetBlobPath).WillByDefault(Return(ErrorEnum::eNone));
        ON_CALL(mOCISpec, LoadImageManifest)
            .WillByDefault(DoAll(SetArgReferee<1>(*mManifest), Return(ErrorEnum::eNone)));
        ON_CALL(mOCISpec, LoadItemConfig)
            .WillByDefault(DoAll(SetArgReferee<1>(*mItemConfig), Return(ErrorEnum::eNone)));
        ON_CALL(mOCISpec, LoadImageConfig).WillByDefault(Return(ErrorEnum::eNone));

        mInstance = std::make_unique<InstanceData>();

        mInstance->mInfo.mItemID      = "item0";
        mInstance->mInfo.mType        = UpdateItemTypeEnum::eService;
        mInstance->mInfo.mRuntimeID   = "runtime0";
        mInstance->mStatus.mRuntimeID = "runtime0";
        mInstance->mInstanceID        = "instanceID0";

        mLauncher.Init(
            mAllocator, mRuntimes, mStorage, mItemInfoProvider, mOCISpec, mNetworkManager, mResourceInfoProvider);
    }

    HeapAllocator                                        mAllocator;
    NiceMock<RuntimeMock>                                mRuntime;
    RuntimeManager                                       mRuntimes;
    StorageStub                                          mStorage;
    NiceMock<common::imagemanager::ItemInfoProviderMock> mItemInfoProvider;
    NiceMock<oci::OCISpecMock>                           mOCISpec;
    StrictMock<networkmanager::NetworkManagerMock>       mNetworkManager;
    NiceMock<resourcemanager::ResourceInfoProviderMock>  mResourceInfoProvider;
    std::unique_ptr<oci::ImageManifest>                  mManifest;
    std::unique_ptr<oci::ItemConfig>                     mItemConfig;
    std::unique_ptr<InstanceData>                        mInstance;
    InstanceLauncher                                     mLauncher;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(InstanceLauncherTest, PrepareStoresServiceAndCreatesItsNetwork)
{
    EXPECT_CALL(mNetworkManager, CreateInstanceNetwork(String("instanceID0"), _, _)).WillOnce(Return(ErrorEnum::eNone));

    auto err = mLauncher.Prepare(*mInstance);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mInstance->mConfigsLoaded);
    EXPECT_EQ(mInstance->mOfflineTTL, 10 * Time::cSeconds);

    StaticArray<InstanceInfo, 1> stored;

    ASSERT_TRUE(mStorage.GetAllInstancesInfos(stored).IsNone());
    ASSERT_EQ(stored.Size(), 1);
    EXPECT_EQ(stored[0], mInstance->mInfo);
}

TEST_F(InstanceLauncherTest, PrepareOnlyStoresComponent)
{
    mInstance->mInfo.mType = UpdateItemTypeEnum::eComponent;

    EXPECT_CALL(mOCISpec, LoadItemConfig).Times(0);

    EXPECT_TRUE(mLauncher.Prepare(*mInstance).IsNone());
    EXPECT_FALSE(mInstance->mConfigsLoaded);
}

TEST_F(InstanceLauncherTest, PrepareFailsOnBrokenConfigs)
{
    EXPECT_CALL(mOCISpec, LoadItemConfig).WillOnce(Return(ErrorEnum::eIntegrity));

    EXPECT_TRUE(mLauncher.Prepare(*mInstance).Is(ErrorEnum::eIntegrity));
    EXPECT_FALSE(mInstance->mConfigsLoaded);
}

TEST_F(InstanceLauncherTest, StartIgnoresAlreadyStartedInstance)
{
    mInstance->mConfigsLoaded = true;

    EXPECT_CALL(mRuntime, StartInstance)
        .WillOnce(Return(ErrorEnum::eAlreadyExist))
        .WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(mLauncher.Start(*mInstance).IsNone());
    EXPECT_TRUE(mLauncher.Start(*mInstance).Is(ErrorEnum::eFailed));
}

TEST_F(InstanceLauncherTest, StartFailsIfRuntimeNotFound)
{
    mInstance->mInfo.mRuntimeID = "unknown";

    EXPECT_CALL(mRuntime, StartInstance).Times(0);

    EXPECT_TRUE(mLauncher.Start(*mInstance).Is(ErrorEnum::eNotFound));
}

TEST_F(InstanceLauncherTest, StartLoadsConfigsNotLoadedOnModuleStart)
{
    // The configs are loaded before the start.
    EXPECT_CALL(mRuntime, StartInstance).WillOnce(Invoke([this](const InstanceInfo&, InstanceStatus&) {
        EXPECT_TRUE(mInstance->mConfigsLoaded);

        return ErrorEnum::eNone;
    }));

    EXPECT_TRUE(mLauncher.Start(*mInstance).IsNone());
}

TEST_F(InstanceLauncherTest, StartFailsOnlyOnBrokenConfigs)
{
    // The broken content fails the start.
    EXPECT_CALL(mOCISpec, LoadItemConfig)
        .WillOnce(Return(ErrorEnum::eIntegrity))
        .WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mRuntime, StartInstance).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_TRUE(mLauncher.Start(*mInstance).Is(ErrorEnum::eIntegrity));

    // Another error is left to the runtime.
    EXPECT_TRUE(mLauncher.Start(*mInstance).IsNone());
}

TEST_F(InstanceLauncherTest, StopIgnoresNotFoundInstance)
{
    EXPECT_CALL(mRuntime, StopInstance).WillOnce(Return(ErrorEnum::eNotFound)).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(mLauncher.Stop(*mInstance).IsNone());
    EXPECT_TRUE(mLauncher.Stop(*mInstance).Is(ErrorEnum::eFailed));
}

TEST_F(InstanceLauncherTest, StopUsesRuntimeOfStatus)
{
    mInstance->mInfo.mRuntimeID   = "unknown";
    mInstance->mStatus.mRuntimeID = "runtime0";

    EXPECT_CALL(mRuntime, StopInstance).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_TRUE(mLauncher.Stop(*mInstance).IsNone());
}

TEST_F(InstanceLauncherTest, ReleaseRemovesStoredInstanceAndReleasesNetwork)
{
    ASSERT_TRUE(mStorage.UpdateInstanceInfo(mInstance->mInfo).IsNone());

    EXPECT_CALL(mNetworkManager, ReleaseInstanceNetwork(String("instanceID0"), _))
        .Times(2)
        .WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_TRUE(mLauncher.Release(*mInstance).IsNone());

    // A not stored instance isn't an error.
    EXPECT_TRUE(mLauncher.Release(*mInstance).IsNone());

    StaticArray<InstanceInfo, 1> stored;

    ASSERT_TRUE(mStorage.GetAllInstancesInfos(stored).IsNone());
    EXPECT_TRUE(stored.IsEmpty());
}

} // namespace aos::sm::launcher
