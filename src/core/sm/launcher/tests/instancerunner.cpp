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
#include <core/sm/launcher/instancerunner.hpp>
#include <core/sm/tests/mocks/networkmanagermock.hpp>
#include <core/sm/tests/mocks/resourcemanagermock.hpp>

#include "mocks/runtimemock.hpp"
#include "stubs/storagestub.hpp"

using namespace testing;

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class InstanceRunnerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        RuntimeInfo runtimeInfo;

        runtimeInfo.mRuntimeID = "runtime0";

        ON_CALL(mRuntime, GetRuntimeInfo).WillByDefault(DoAll(SetArgReferee<0>(runtimeInfo), Return(ErrorEnum::eNone)));
        ON_CALL(mRuntime, StartInstance).WillByDefault(Return(ErrorEnum::eNone));

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

        ASSERT_TRUE(mInstances.PushBack(mInstance.get()).IsNone());

        mRunner = std::make_unique<InstanceRunner>();

        mRunner->Init(mAllocator, mRuntimes, mStorage, mItemInfoProvider, mOCISpec, mNetworkManager,
            mResourceInfoProvider, mStateMutex);
    }

    HeapAllocator                                        mAllocator;
    NiceMock<RuntimeMock>                                mRuntime;
    RuntimeManager                                       mRuntimes;
    StorageStub                                          mStorage;
    NiceMock<common::imagemanager::ItemInfoProviderMock> mItemInfoProvider;
    NiceMock<oci::OCISpecMock>                           mOCISpec;
    NiceMock<networkmanager::NetworkManagerMock>         mNetworkManager;
    NiceMock<resourcemanager::ResourceInfoProviderMock>  mResourceInfoProvider;
    Mutex                                                mStateMutex;
    std::unique_ptr<oci::ImageManifest>                  mManifest;
    std::unique_ptr<oci::ItemConfig>                     mItemConfig;
    std::unique_ptr<InstanceData>                        mInstance;
    StaticArray<InstanceData*, 1>                        mInstances;
    std::unique_ptr<InstanceRunner>                      mRunner;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(InstanceRunnerTest, StartPreparesAndStartsService)
{
    {
        InSequence sequence;

        EXPECT_CALL(mNetworkManager, CreateInstanceNetwork(String("instanceID0"), _, _))
            .WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNetworkManager, StartInstanceNetwork(String("instanceID0"), _)).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mRuntime, StartInstance).WillOnce(Return(ErrorEnum::eNone));
    }

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eActive);
    EXPECT_TRUE(mInstance->mPrepared);
    EXPECT_TRUE(mInstance->mConfigsLoaded);
    EXPECT_EQ(mInstance->mOfflineTTL, 10 * Time::cSeconds);

    StaticArray<InstanceInfo, 1> stored;

    ASSERT_TRUE(mStorage.GetAllInstancesInfos(stored).IsNone());
    ASSERT_EQ(stored.Size(), 1);
    EXPECT_EQ(stored[0], mInstance->mInfo);
}

TEST_F(InstanceRunnerTest, StartDoesNotPrepareInstanceAgain)
{
    mInstance->mPrepared      = true;
    mInstance->mConfigsLoaded = true;

    EXPECT_CALL(mOCISpec, LoadItemConfig).Times(0);
    EXPECT_CALL(mNetworkManager, CreateInstanceNetwork).Times(0);
    EXPECT_CALL(mNetworkManager, StartInstanceNetwork).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime, StartInstance).WillOnce(Return(ErrorEnum::eAlreadyExist));

    mRunner->Start(mInstances);

    // An already started instance isn't an error.
    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eActive);
}

TEST_F(InstanceRunnerTest, StartOnlyStoresAndStartsComponent)
{
    mInstance->mInfo.mType = UpdateItemTypeEnum::eComponent;

    EXPECT_CALL(mOCISpec, LoadItemConfig).Times(0);
    EXPECT_CALL(mNetworkManager, CreateInstanceNetwork).Times(0);
    EXPECT_CALL(mNetworkManager, StartInstanceNetwork).Times(0);
    EXPECT_CALL(mRuntime, StartInstance).WillOnce(Return(ErrorEnum::eNone));

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eActive);
    EXPECT_TRUE(mInstance->mPrepared);
}

TEST_F(InstanceRunnerTest, StartFailsInstanceWithBrokenConfigs)
{
    EXPECT_CALL(mOCISpec, LoadItemConfig).WillOnce(Return(ErrorEnum::eIntegrity));
    EXPECT_CALL(mNetworkManager, StartInstanceNetwork).Times(0);
    EXPECT_CALL(mRuntime, StartInstance).Times(0);

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(mInstance->mStatus.mError.Is(ErrorEnum::eIntegrity));
    EXPECT_FALSE(mInstance->mPrepared);
}

TEST_F(InstanceRunnerTest, StartSkipsFailedInstance)
{
    mInstance->mStatus.mState = InstanceStateEnum::eFailed;

    EXPECT_CALL(mOCISpec, LoadItemConfig).Times(0);
    EXPECT_CALL(mNetworkManager, StartInstanceNetwork).Times(0);
    EXPECT_CALL(mRuntime, StartInstance).Times(0);

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eFailed);
}

TEST_F(InstanceRunnerTest, StartFailsIfRuntimeNotFound)
{
    mInstance->mInfo.mRuntimeID = "unknown";

    EXPECT_CALL(mRuntime, StartInstance).Times(0);

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(mInstance->mStatus.mError.Is(ErrorEnum::eNotFound));
}

TEST_F(InstanceRunnerTest, StartFailsInstanceWhoseNetworkFailedToApply)
{
    EXPECT_CALL(mNetworkManager, FlushBatch).WillOnce(Invoke([](Array<StaticString<cIDLen>>& failedIDs) {
        failedIDs.PushBack("instanceID0");

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mNetworkManager, StopInstanceNetwork(String("instanceID0"), _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime, StartInstance).Times(0);

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eFailed);
}

TEST_F(InstanceRunnerTest, StartLoadsConfigsNotLoadedOnModuleStart)
{
    mInstance->mPrepared = true;

    // The configs are loaded before the start.
    EXPECT_CALL(mRuntime, StartInstance).WillOnce(Invoke([this](const InstanceInfo&, InstanceStatus&) {
        EXPECT_TRUE(mInstance->mConfigsLoaded);

        return ErrorEnum::eNone;
    }));

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eActive);
}

TEST_F(InstanceRunnerTest, StartFailsOnlyOnBrokenConfigs)
{
    mInstance->mPrepared = true;

    // The broken content fails the start.
    EXPECT_CALL(mOCISpec, LoadItemConfig)
        .WillOnce(Return(ErrorEnum::eIntegrity))
        .WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mRuntime, StartInstance).WillOnce(Return(ErrorEnum::eNone));

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(mInstance->mStatus.mError.Is(ErrorEnum::eIntegrity));

    // Another error is left to the runtime.
    mInstance->mStatus.mState = InstanceStateEnum::eInactive;

    mRunner->Start(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eActive);
}

TEST_F(InstanceRunnerTest, StopStopsInstanceAndItsNetwork)
{
    mInstance->mStatus.mState = InstanceStateEnum::eActive;

    // A not found instance isn't an error.
    EXPECT_CALL(mRuntime, StopInstance).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mNetworkManager, StopInstanceNetwork(String("instanceID0"), _)).WillOnce(Return(ErrorEnum::eNone));

    mRunner->Stop(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eInactive);
}

TEST_F(InstanceRunnerTest, StopFailsInstanceOnError)
{
    mInstance->mStatus.mState = InstanceStateEnum::eActive;

    EXPECT_CALL(mRuntime, StopInstance).WillOnce(Return(ErrorEnum::eFailed));

    mRunner->Stop(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(mInstance->mStatus.mError.Is(ErrorEnum::eFailed));
}

TEST_F(InstanceRunnerTest, StopUsesRuntimeOfStatus)
{
    mInstance->mInfo.mRuntimeID   = "unknown";
    mInstance->mStatus.mRuntimeID = "runtime0";

    EXPECT_CALL(mRuntime, StopInstance).WillOnce(Return(ErrorEnum::eNone));

    mRunner->Stop(mInstances);

    EXPECT_EQ(mInstance->mStatus.mState, InstanceStateEnum::eInactive);
}

TEST_F(InstanceRunnerTest, ReleaseRemovesStoredInstanceAndReleasesNetwork)
{
    ASSERT_TRUE(mStorage.UpdateInstanceInfo(mInstance->mInfo).IsNone());

    EXPECT_CALL(mNetworkManager, ReleaseInstanceNetwork(String("instanceID0"), _))
        .Times(2)
        .WillRepeatedly(Return(ErrorEnum::eNone));

    mRunner->Release(mInstances);

    // A not stored instance isn't an error.
    mRunner->Release(mInstances);

    EXPECT_NE(mInstance->mStatus.mState, InstanceStateEnum::eFailed);

    StaticArray<InstanceInfo, 1> stored;

    ASSERT_TRUE(mStorage.GetAllInstancesInfos(stored).IsNone());
    EXPECT_TRUE(stored.IsEmpty());
}

TEST_F(InstanceRunnerTest, LoadConfigsLoadsServiceConfigs)
{
    mRunner->LoadConfigs(mInstances);

    EXPECT_TRUE(mInstance->mConfigsLoaded);
    EXPECT_EQ(mInstance->mOfflineTTL, 10 * Time::cSeconds);

    // The failure is left to the start.
    mInstance->mConfigsLoaded = false;

    EXPECT_CALL(mOCISpec, LoadItemConfig).WillOnce(Return(ErrorEnum::eIntegrity));

    mRunner->LoadConfigs(mInstances);

    EXPECT_FALSE(mInstance->mConfigsLoaded);
    EXPECT_NE(mInstance->mStatus.mState, InstanceStateEnum::eFailed);
}

} // namespace aos::sm::launcher
