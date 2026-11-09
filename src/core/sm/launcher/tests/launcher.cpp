/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <atomic>
#include <future>
#include <list>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/monitoring/monitoring.hpp>
#include <core/common/tests/mocks/cloudconnectionmock.hpp>
#include <core/common/tests/mocks/instancestatusprovidermock.hpp>
#include <core/common/tests/mocks/iteminfoprovidermock.hpp>
#include <core/common/tests/mocks/itemstatusprovidermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/sm/launcher/itf/rebooter.hpp>
#include <core/sm/launcher/itf/updatechecker.hpp>
#include <core/sm/launcher/launcher.hpp>
#include <core/sm/tests/mocks/instanceidprovidermock.hpp>
#include <core/sm/tests/mocks/networkmanagermock.hpp>
#include <core/sm/tests/mocks/resourcemanagermock.hpp>

#include "mocks/imagemanagermock.hpp"
#include "mocks/runtimemock.hpp"
#include "stubs/senderstub.hpp"
#include "stubs/storagestub.hpp"

using namespace testing;

namespace aos {

/***********************************************************************************************************************
 * Operators
 **********************************************************************************************************************/

std::ostream& operator<<(std::ostream& os, const InstanceIdent& ident)
{
    return os << "{" << ident.mItemID.CStr() << ":" << ident.mSubjectID.CStr() << ":" << ident.mInstance << "}";
}

std::ostream& operator<<(std::ostream& os, const InstanceStatus& status)
{
    return os << "{" << static_cast<const InstanceIdent&>(status) << ":" << status.mVersion.CStr() << ":"
              << status.mRuntimeID.CStr() << ":" << status.mState.ToString().CStr() << ":"
              << tests::utils::ErrorToStr(status.mError) << "}";
}

namespace sm::launcher {

std::ostream& operator<<(std::ostream& os, const InstanceStatus& status)
{
    return os << "{" << static_cast<const InstanceIdent&>(status) << ":" << status.mRuntimeID.CStr() << ":"
              << status.mState.ToString().CStr() << ":" << tests::utils::ErrorToStr(status.mError) << "}";
}

} // namespace sm::launcher

} // namespace aos

namespace aos::sm::launcher {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

InstanceInfo CreateInstanceInfo(const String& itemID, uint64_t instance, const String& version, const String& runtimeID,
    const UpdateItemType& type = UpdateItemTypeEnum::eService)
{
    InstanceInfo info;

    info.mItemID    = itemID;
    info.mSubjectID = "";
    info.mInstance  = instance;
    info.mVersion   = version;
    info.mRuntimeID = runtimeID;
    info.mType      = type;

    return info;
}

RuntimeInfo CreateRuntimeInfo(const String& runtimeID)
{
    RuntimeInfo info;

    info.mRuntimeID = runtimeID;

    return info;
}

void SetInstanceStatus(const InstanceIdent& instance, InstanceStateEnum state, InstanceStatus& status)
{
    static_cast<InstanceIdent&>(status) = instance;
    status.mState                       = state;
}

void SetInstanceStatus(const InstanceInfo& instance, InstanceStateEnum state, InstanceStatus& status)
{
    SetInstanceStatus(static_cast<const InstanceIdent&>(instance), state, status);
    status.mRuntimeID = instance.mRuntimeID;
}

InstanceStatus CreateInstanceStatus(
    const InstanceIdent& instance, const String& version, const String& runtimeID, InstanceStateEnum state)
{
    InstanceStatus status;

    SetInstanceStatus(instance, state, status);
    status.mVersion   = version;
    status.mRuntimeID = runtimeID;

    return status;
}

InstanceStatus CreateInstanceStatus(const InstanceInfo& instance, InstanceStateEnum state,
    const UpdateItemTypeEnum type = UpdateItemTypeEnum::eService, bool preinstalled = false)
{
    InstanceStatus status = CreateInstanceStatus(
        static_cast<const InstanceIdent&>(instance), instance.mVersion, instance.mRuntimeID, state);

    status.mType         = type;
    status.mPreinstalled = preinstalled;

    return status;
}

monitoring::InstanceMonitoringData CreateMonitoringData(const InstanceInfo& instance)
{
    monitoring::InstanceMonitoringData data;

    data.mInstanceIdent = static_cast<const InstanceIdent&>(instance);
    data.mRuntimeID     = instance.mRuntimeID;

    return data;
}

auto ItemsInstalled(std::vector<std::string> itemIDs)
{
    return [itemIDs = std::move(itemIDs)](Array<UpdateItemStatus>& statuses) {
        for (const auto& itemID : itemIDs) {
            UpdateItemStatus installed;

            installed.mItemID  = itemID.c_str();
            installed.mVersion = "1.0.0";
            installed.mState   = ItemStateEnum::eInstalled;

            if (auto err = statuses.PushBack(installed); !err.IsNone()) {
                return err;
            }
        }

        return Error(ErrorEnum::eNone);
    };
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class LauncherTest : public Test {
protected:
    static constexpr auto cWaitTimeout = Time::cSeconds;

    static void SetUpTestSuite()
    {
        tests::utils::InitLog();

        LOG_INF() << "Launcher size" << Log::Field("size", sizeof(Launcher));
    }

    void SetUp() override
    {
        EXPECT_CALL(mRuntime0, Start).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mRuntime0, Stop).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mRuntime0, GetRuntimeInfo)
            .WillRepeatedly(DoAll(SetArgReferee<0>(CreateRuntimeInfo("runtime0")), Return(ErrorEnum::eNone)));
        EXPECT_CALL(mRuntime0, InitInstances).WillRepeatedly(Return(ErrorEnum::eNone));

        EXPECT_CALL(mRuntime1, Start).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mRuntime1, Stop).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mRuntime1, GetRuntimeInfo)
            .WillRepeatedly(DoAll(SetArgReferee<0>(CreateRuntimeInfo("runtime1")), Return(ErrorEnum::eNone)));
        EXPECT_CALL(mRuntime1, InitInstances).WillRepeatedly(Return(ErrorEnum::eNone));

        mImageManifest.mItemConfig.EmplaceValue();

        EXPECT_CALL(mItemInfoProvider, GetBlobPath).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mOCISpec, LoadImageManifest)
            .WillRepeatedly(DoAll(SetArgReferee<1>(mImageManifest), Return(ErrorEnum::eNone)));
        EXPECT_CALL(mOCISpec, LoadImageConfig)
            .WillRepeatedly(DoAll(SetArgReferee<1>(mImageConfig), Return(ErrorEnum::eNone)));
        EXPECT_CALL(mOCISpec, LoadItemConfig)
            .WillRepeatedly(DoAll(SetArgReferee<1>(mItemConfig), Return(ErrorEnum::eNone)));

        EXPECT_CALL(mCloudConnection, SubscribeListener)
            .WillRepeatedly(Invoke([&](cloudconnection::ConnectionListenerItf& listener) {
                mCloudConnectionListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mCloudConnection, IsConnected).WillRepeatedly(Return(true));

        EXPECT_CALL(mInstanceIDProvider, GetInstanceID).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNetworkManager, CreateInstanceNetwork).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNetworkManager, StartInstanceNetwork).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNetworkManager, StopInstanceNetwork).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNetworkManager, ReleaseInstanceNetwork).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNetworkManager, BeginBatch()).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNetworkManager, FlushBatch(_)).WillRepeatedly(Return(ErrorEnum::eNone));
    }

    StaticArray<RuntimeItf*, cMaxNumNodeRuntimes> GetRuntimesArray()
    {
        StaticArray<RuntimeItf*, cMaxNumNodeRuntimes> runtimes;

        runtimes.PushBack(&mRuntime0);
        runtimes.PushBack(&mRuntime1);

        return runtimes;
    }

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

    Launcher mLauncher;

    StrictMock<RuntimeMock>                                mRuntime0;
    StrictMock<RuntimeMock>                                mRuntime1;
    NiceMock<imagemanager::ImageManagerMock>               mImageManager;
    NiceMock<common::imagemanager::ItemStatusProviderMock> mItemStatusProvider;
    StorageStub                                            mStorage;
    SenderStub                                             mSender;
    StrictMock<oci::OCISpecMock>                           mOCISpec;
    StrictMock<common::imagemanager::ItemInfoProviderMock> mItemInfoProvider;
    StrictMock<cloudconnection::CloudConnectionMock>       mCloudConnection;
    StrictMock<networkmanager::NetworkManagerMock>         mNetworkManager;
    instancestatusprovider::ListenerMock                   mStatusListener;
    InstanceStatusArray                                    mReceivedStatuses;
    cloudconnection::ConnectionListenerItf*                mCloudConnectionListener {};
    StrictMock<InstanceIDProviderMock>                     mInstanceIDProvider;
    StrictMock<resourcemanager::ResourceInfoProviderMock>  mResourceInfoProvider;

    oci::ImageManifest mImageManifest;
    oci::ImageConfig   mImageConfig;
    oci::ItemConfig    mItemConfig;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(LauncherTest, NoStoredInstancesOnModuleStart)
{
    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, InitInstances)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item1", 1, "1.0.0", "runtime1"),
        CreateInstanceInfo("item2", 2, "1.0.0", "runtime0"),
    };

    const std::vector cRuntime0Infos = {cStoredInfos[0], cStoredInfos[2]};
    const std::vector cRuntime1Infos = {cStoredInfos[1]};

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, InitInstances(Array<InstanceInfo>(&cRuntime0Infos.front(), cRuntime0Infos.size())))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, InitInstances(Array<InstanceInfo>(&cRuntime1Infos.front(), cRuntime1Infos.size())))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mRuntime0, StartInstance)
        .WillRepeatedly(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mRuntime1, StartInstance)
        .WillRepeatedly(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StopInstance)
        .WillRepeatedly(Invoke([](const InstanceIdent& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eInactive, status);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mRuntime1, StopInstance)
        .WillRepeatedly(Invoke([](const InstanceIdent& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eInactive, status);

            return ErrorEnum::eNone;
        }));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, SendActiveComponentNodeInstancesStatusOnModuleStart)
{
    const std::vector cRuntime0Components = {
        CreateInstanceStatus(CreateInstanceInfo("item0", 0, "1.0.1", "runtime0"), InstanceStateEnum::eActive,
            UpdateItemTypeEnum::eComponent),
        CreateInstanceStatus(CreateInstanceInfo("item1", 0, "1.0.0", "runtime0"), InstanceStateEnum::eInactive,
            UpdateItemTypeEnum::eComponent, true),
        CreateInstanceStatus(CreateInstanceInfo("item2", 2, "1.0.0", "runtime0"), InstanceStateEnum::eActive,
            UpdateItemTypeEnum::eService),
    };

    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.1", "runtime0", UpdateItemTypeEnum::eComponent),
    };

    mStorage.Init(cStoredInfos);

    const auto cActiveComponent = static_cast<const InstanceIdent&>(cRuntime0Components[0]);

    EXPECT_CALL(mRuntime0, Start).WillOnce(Invoke([&]() {
        mLauncher.OnInstancesStatusesReceived(
            Array<InstanceStatus>(cRuntime0Components.data(), cRuntime0Components.size()));

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([&](const InstanceInfo& info, InstanceStatus& status) {
        EXPECT_EQ(static_cast<const InstanceIdent&>(info), cActiveComponent);

        status = cRuntime0Components[0];

        return ErrorEnum::eNone;
    }));

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0], cRuntime0Components[0]);

    EXPECT_CALL(mRuntime0, StopInstance(cActiveComponent, _)).Times(0);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, DoNotSendUpdateInstancesStatusesBeforeModuleStart)
{
    const std::vector cRuntime0Components = {
        CreateInstanceStatus(CreateInstanceInfo("item1", 1, "1.0.0", "runtime0"), InstanceStateEnum::eActive,
            UpdateItemTypeEnum::eComponent, true),
    };

    const auto cPreinstalledComponent = static_cast<const InstanceIdent&>(cRuntime0Components[0]);

    EXPECT_CALL(mRuntime0, Start).WillOnce(Invoke([&]() {
        mLauncher.OnInstancesStatusesReceived(
            Array<InstanceStatus>(cRuntime0Components.data(), cRuntime0Components.size()));

        return ErrorEnum::eNone;
    }));

    std::promise<void> startInstancePromise;

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([&](const InstanceInfo& info, InstanceStatus& status) {
        EXPECT_EQ(static_cast<const InstanceIdent&>(info), cPreinstalledComponent);

        status = cRuntime0Components[0];

        if (startInstancePromise.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            return ErrorEnum::eTimeout;
        }

        return ErrorEnum::eNone;
    }));

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, Time::cSeconds);
    EXPECT_EQ(err, ErrorEnum::eTimeout) << tests::utils::ErrorToStr(err);

    startInstancePromise.set_value();

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0], cRuntime0Components[0]);

    EXPECT_CALL(mRuntime0, StopInstance(cPreinstalledComponent, _)).Times(0);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, LauncherStartsStoredInstancesOnModuleStart)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item1", 1, "1.0.0", "runtime1"),
    };
    const std::vector cExpectedStatuses = {
        CreateInstanceStatus(cStoredInfos[0], InstanceStateEnum::eActive),
        CreateInstanceStatus(cStoredInfos[1], InstanceStateEnum::eActive),
    };

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mRuntime1, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStoredInfos.size());

    for (size_t i = 0; i < mReceivedStatuses.Size(); ++i) {
        EXPECT_EQ(mReceivedStatuses[i], cExpectedStatuses[i]);
    }

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cStoredInfos[0]), _))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, StopInstance(static_cast<const InstanceIdent&>(cStoredInfos[1]), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, StartNetworks_FlushFailure_FailsInstance)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item1", 1, "1.0.0", "runtime1"),
    };

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mInstanceIDProvider, GetInstanceID)
        .WillRepeatedly(Invoke([](const InstanceIdent& instance, String& instanceID) {
            instanceID = instance.mItemID;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mNetworkManager, FlushBatch(_))
        .WillRepeatedly(DoAll(WithArg<0>([](auto& failedInstanceIDs) { failedInstanceIDs.PushBack("item0"); }),
            Return(ErrorEnum::eNone)));

    EXPECT_CALL(mNetworkManager, StopInstanceNetwork(String("item0"), _))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mRuntime1, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStoredInfos.size());

    for (const auto& status : mReceivedStatuses) {
        if (status.mItemID == "item0") {
            EXPECT_EQ(status.mState, InstanceStateEnum::eFailed);
        } else {
            EXPECT_EQ(status.mState, InstanceStateEnum::eActive);
        }
    }

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cStoredInfos[0]), _))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, StopInstance(static_cast<const InstanceIdent&>(cStoredInfos[1]), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, StopInstancesWithExpiredOfflineTTL)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item1", 1, "1.0.0", "runtime1"),
    };
    const std::vector cExpectedStatuses = {
        CreateInstanceStatus(cStoredInfos[0], InstanceStateEnum::eActive),
        CreateInstanceStatus(cStoredInfos[1], InstanceStateEnum::eActive),
    };

    mItemConfig.mOfflineTTL = Time::cMilliseconds;
    EXPECT_CALL(mOCISpec, LoadItemConfig)
        .WillRepeatedly(DoAll(SetArgReferee<1>(mItemConfig), Return(ErrorEnum::eNone)));

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mRuntime1, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStoredInfos.size());

    for (size_t i = 0; i < mReceivedStatuses.Size(); ++i) {
        EXPECT_EQ(mReceivedStatuses[i], cExpectedStatuses[i]);
    }

    std::promise<void> stop0promise;
    std::promise<void> stop1promise;

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cStoredInfos[0]), _))
        .WillOnce(Invoke([&](const InstanceIdent& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eInactive, status);

            stop0promise.set_value();

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mRuntime1, StopInstance(static_cast<const InstanceIdent&>(cStoredInfos[1]), _))
        .WillOnce(Invoke([&](const InstanceIdent& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eInactive, status);

            stop1promise.set_value();

            return ErrorEnum::eNone;
        }));

    mCloudConnectionListener->OnDisconnect();

    ASSERT_TRUE(stop0promise.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready)
        << "Runtime0 StopInstance was not called";
    ASSERT_TRUE(stop1promise.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready)
        << "Runtime1 StopInstance was not called";

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cStoredInfos[0]), _))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, StopInstance(static_cast<const InstanceIdent&>(cStoredInfos[1]), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, InstallUpdateItemFailed)
{
    const std::vector cInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 1, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 2, "2.0.0", "runtime1"),
    };
    const Array<InstanceInfo> cInstances(cInstanceInfos.data(), cInstanceInfos.size());
    const Error               cInstallError(ErrorEnum::eFailed, "update item install failed");

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mImageManager, InstallItem).Times(2).WillRepeatedly(Invoke([&](const auto& item) {
        return item.mVersion == "1.0.0" ? cInstallError : Error(ErrorEnum::eNone);
    }));
    EXPECT_CALL(mRuntime0, StartInstance).Times(0);
    EXPECT_CALL(mRuntime1, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mNetworkManager, CreateInstanceNetwork).Times(1).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mNetworkManager, StartInstanceNetwork).Times(1).WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.UpdateInstances({}, cInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_EQ(mReceivedStatuses.Size(), cInstances.Size());

    for (const auto& status : mReceivedStatuses) {
        if (status.mVersion == "1.0.0") {
            EXPECT_EQ(status.mState, InstanceStateEnum::eFailed);
            EXPECT_EQ(status.mError, cInstallError);
        } else {
            EXPECT_EQ(status.mState, InstanceStateEnum::eActive);
        }
    }

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mImageManager));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mRuntime0));

    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime0, StartInstance)
        .Times(2)
        .WillRepeatedly(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mNetworkManager, CreateInstanceNetwork).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mNetworkManager, StartInstanceNetwork).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.UpdateInstances({}, Array<InstanceInfo>(cInstanceInfos.data(), 2));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_EQ(mReceivedStatuses.Size(), cInstances.Size());
    for (const auto& status : mReceivedStatuses) {
        EXPECT_EQ(status.mState, InstanceStateEnum::eActive);
        EXPECT_TRUE(status.mError.IsNone());
    }

    EXPECT_CALL(mRuntime0, StopInstance).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, StopInstance).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime0, Stop).WillOnce(Return(ErrorEnum::eNone));
    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, InstalledItemsReadFailedAndFailedInstanceRemoved)
{
    const std::vector cInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item1", 1, "1.0.0", "runtime1"),
    };
    const Array<InstanceInfo> cInstances(cInstanceInfos.data(), cInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke([](Array<UpdateItemStatus>& statuses) {
        UpdateItemStatus installed;

        installed.mItemID  = "item0";
        installed.mVersion = "1.0.0";
        installed.mState   = ItemStateEnum::eInstalled;

        (void)statuses.PushBack(installed);

        return ErrorEnum::eFailed;
    }));
    EXPECT_CALL(mImageManager, InstallItem).Times(2).WillRepeatedly(Invoke([](const auto& item) {
        return item.mID == "item0" ? ErrorEnum::eFailed : ErrorEnum::eNone;
    }));
    EXPECT_CALL(mRuntime0, StartInstance).Times(0);
    EXPECT_CALL(mRuntime1, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.UpdateInstances({}, cInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_EQ(mReceivedStatuses.Size(), 2);
    for (const auto& status : mReceivedStatuses) {
        EXPECT_EQ(status.mState, status.mItemID == "item0" ? InstanceStateEnum::eFailed : InstanceStateEnum::eActive);
    }

    auto storedInstances = std::make_unique<InstanceInfoArray>();
    err                  = mStorage.GetAllInstancesInfos(*storedInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_EQ(storedInstances->Size(), 1);
    EXPECT_EQ((*storedInstances)[0].mItemID, "item1");

    EXPECT_CALL(mRuntime0, StopInstance).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mNetworkManager, ReleaseInstanceNetwork).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Return(ErrorEnum::eNone));
    const InstanceIdent cStopInstance = cInstanceInfos[0];
    err                               = mLauncher.UpdateInstances(Array<InstanceIdent>(&cStopInstance, 1), {});
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0].mItemID, "item1");
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eActive);

    EXPECT_CALL(mRuntime1, StopInstance).WillOnce(Return(ErrorEnum::eNone));
    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, UpdateInstances)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item1", 1, "1.0.0", "runtime0"),
        CreateInstanceInfo("item2", 2, "1.0.0", "runtime1"),
    };
    const Array<InstanceInfo>  cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());
    const Array<InstanceIdent> cStopInstances(&static_cast<const InstanceIdent&>(cStoredInfos.front()), 1);

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStoredInfos.size());

    for (size_t i = 0; i < mReceivedStatuses.Size(); ++i) {
        EXPECT_EQ(mReceivedStatuses[i], CreateInstanceStatus(cStoredInfos[i], InstanceStateEnum::eActive));
    }

    EXPECT_CALL(mRuntime0, StopInstance).WillOnce(Invoke([](const InstanceIdent& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eInactive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mRuntime1, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mImageManager, RemoveItem(cStoredInfos[0].mItemID, cStoredInfos[0].mVersion))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mImageManager, InstallItem).WillRepeatedly(Invoke([&](const imagemanager::UpdateItemInfo& itemInfo) {
        auto it = std::find_if(
            cStartInstanceInfos.begin(), cStartInstanceInfos.end(), [&itemInfo](const InstanceInfo& info) {
                return info.mItemID == itemInfo.mID && info.mVersion == itemInfo.mVersion;
            });

        EXPECT_NE(it, cStartInstanceInfos.end());

        return ErrorEnum::eNone;
    }));

    err = mLauncher.UpdateInstances(cStopInstances, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStartInstances.Size());

    for (size_t i = 0; i < cStartInstances.Size(); ++i) {
        EXPECT_EQ(mReceivedStatuses[i], CreateInstanceStatus(cStartInstances[i], InstanceStateEnum::eActive));
    }

    auto storedData = std::make_unique<InstanceInfoArray>();

    err = mStorage.GetAllInstancesInfos(*storedData);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    storedData->Sort([](const InstanceInfo& a, const InstanceInfo& b) {
        return static_cast<const InstanceIdent&>(a) < static_cast<const InstanceIdent&>(b);
    });

    EXPECT_EQ(*storedData, Array<InstanceInfo>(&cStartInstanceInfos.front(), cStartInstanceInfos.size()));

    for (const auto& expectedStopInstance : cStartInstances) {
        if (expectedStopInstance.mRuntimeID == "runtime0") {
            EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(expectedStopInstance), _))
                .WillOnce(Return(ErrorEnum::eNone));
        } else if (expectedStopInstance.mRuntimeID == "runtime1") {
            EXPECT_CALL(mRuntime1, StopInstance(static_cast<const InstanceIdent&>(expectedStopInstance), _))
                .WillOnce(Return(ErrorEnum::eNone));
        }
    }

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, UpdateInstancesRestartsInstancesWithModifiedParams)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    std::vector               startInstanceInfos = {cStoredInfos[0]};
    const Array<InstanceInfo> cStartInstances(&startInstanceInfos.front(), startInstanceInfos.size());

    // Modify first instance parameters to force restart
    startInstanceInfos[0].mMonitoringParams.EmplaceValue();
    startInstanceInfos[0].mMonitoringParams->mAlertRules.EmplaceValue();

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStoredInfos.size());

    for (size_t i = 0; i < mReceivedStatuses.Size(); ++i) {
        EXPECT_EQ(mReceivedStatuses[i], CreateInstanceStatus(cStoredInfos[i], InstanceStateEnum::eActive));
    }

    EXPECT_CALL(mRuntime0, StopInstance).WillOnce(Invoke([](const InstanceIdent& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eInactive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);

    EXPECT_EQ(mReceivedStatuses[0],
        CreateInstanceStatus(
            cStoredInfos[0], cStoredInfos[0].mVersion, cStoredInfos[0].mRuntimeID, InstanceStateEnum::eActive));

    auto storedData = std::make_unique<InstanceInfoArray>();

    err = mStorage.GetAllInstancesInfos(*storedData);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    storedData->Sort([](const InstanceInfo& a, const InstanceInfo& b) {
        return static_cast<const InstanceIdent&>(a) < static_cast<const InstanceIdent&>(b);
    });

    EXPECT_EQ(*storedData, cStartInstances);

    EXPECT_CALL(mRuntime0, StopInstance(mReceivedStatuses[0], _)).WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, UpdateInstancesInstallsOnlyNotInstalledItems)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item1", 1, "1.0.0", "runtime0"),
        CreateInstanceInfo("item2", 2, "1.0.0", "runtime1"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 0);

    const auto startInstance = [](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    };

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke(startInstance));
    EXPECT_CALL(mRuntime1, StartInstance).WillOnce(Invoke(startInstance));

    // The reported versions which are not installed, e.g. removed and kept in the cache, are installed.
    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke([](Array<UpdateItemStatus>& statuses) {
        UpdateItemStatus removed;

        removed.mItemID  = "item1";
        removed.mVersion = "1.0.0";
        removed.mState   = ItemStateEnum::eRemoved;

        UpdateItemStatus installed;

        installed.mItemID  = "item2";
        installed.mVersion = "1.0.0";
        installed.mState   = ItemStateEnum::eInstalled;

        EXPECT_TRUE(statuses.PushBack(removed).IsNone());
        EXPECT_TRUE(statuses.PushBack(installed).IsNone());

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Invoke([](const imagemanager::UpdateItemInfo& itemInfo) {
        EXPECT_EQ(itemInfo.mID, "item1");

        return ErrorEnum::eNone;
    }));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cStartInstanceInfos[0]), _))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, StopInstance(static_cast<const InstanceIdent&>(cStartInstanceInfos[1]), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, StopCancelsItemsBeingInstalled)
{
    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mImageManager, Cancel).WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, StopCancelsItemsRequestedAfterCancel)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    std::promise<void> launchStarted;
    std::promise<void> cancelCalled;
    auto               cancelCalledFuture = cancelCalled.get_future();

    // The launch reaches the install after Stop has canceled the image manager.
    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke([&](Array<UpdateItemStatus>&) {
        launchStarted.set_value();
        cancelCalledFuture.wait();

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManager, Cancel).WillOnce(Invoke([&cancelCalled]() {
        cancelCalled.set_value();

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManager, InstallItem).Times(0);
    EXPECT_CALL(mRuntime0, StartInstance).Times(0);
    EXPECT_CALL(mRuntime0, StopInstance).WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    launchStarted.get_future().wait();

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(mReceivedStatuses[0].mError.Is(ErrorEnum::eCanceled))
        << tests::utils::ErrorToStr(mReceivedStatuses[0].mError);
}

TEST_F(LauncherTest, RepairsItemsOfInstancesFailedWithIntegrityError)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 1, "1.0.0", "runtime0"),
        CreateInstanceInfo("item1", 2, "1.0.0", "runtime1"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke(ItemsInstalled({"item0", "item1"})));

    std::atomic_int brokenStarts {2};

    // The first start of each item0 instance fails, as the item content is broken.
    EXPECT_CALL(mRuntime0, StartInstance)
        .Times(4)
        .WillRepeatedly(Invoke([&brokenStarts](const InstanceInfo& instance, InstanceStatus& status) {
            if (brokenStarts-- > 0) {
                return Error(ErrorEnum::eIntegrity);
            }

            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return Error(ErrorEnum::eNone);
        }));
    EXPECT_CALL(mRuntime1, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    // The item is repaired once for all its broken instances.
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Invoke([](const imagemanager::UpdateItemInfo& itemInfo) {
        EXPECT_EQ(itemInfo.mID, "item0");
        EXPECT_EQ(itemInfo.mVersion, "1.0.0");

        return ErrorEnum::eNone;
    }));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStartInstanceInfos.size());

    for (const auto& status : mReceivedStatuses) {
        EXPECT_EQ(status.mState, InstanceStateEnum::eActive) << status;
    }

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, InstancesOfNotRepairedItemFailWithRepairError)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke(ItemsInstalled({"item0"})));
    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Return(ErrorEnum::eIntegrity));
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eRuntime));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(mReceivedStatuses[0].mError.Is(ErrorEnum::eRuntime)) << mReceivedStatuses[0];

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RepairsItemOncePerRequest)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke(ItemsInstalled({"item0"})));

    // The CM copy is broken as well, so the second start fails again.
    EXPECT_CALL(mRuntime0, StartInstance).Times(2).WillRepeatedly(Return(ErrorEnum::eIntegrity));
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(mReceivedStatuses[0].mError.Is(ErrorEnum::eIntegrity)) << mReceivedStatuses[0];

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, DoesNotRepairItemsOfInstancesFailedWithOtherErrors)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke(ItemsInstalled({"item0"})));
    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Return(ErrorEnum::eRuntime));
    EXPECT_CALL(mImageManager, InstallItem).Times(0);

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(mReceivedStatuses[0].mError.Is(ErrorEnum::eRuntime)) << mReceivedStatuses[0];

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RepairsItemFailedOnModuleStart)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The start of the stored instance fails, as the item content is broken: the item is repaired on module start and
    // the instance is started again.
    EXPECT_CALL(mRuntime0, StartInstance)
        .WillOnce(Return(ErrorEnum::eIntegrity))
        .WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Invoke([](const imagemanager::UpdateItemInfo& itemInfo) {
        EXPECT_EQ(itemInfo.mID, "item0");
        EXPECT_EQ(itemInfo.mVersion, "1.0.0");

        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The statuses are sent when the module start repair is finished.
    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eActive) << mReceivedStatuses[0];

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RepairsItemWithBrokenConfigs)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke([](Array<UpdateItemStatus>& statuses) {
        UpdateItemStatus installed;

        installed.mItemID  = "item0";
        installed.mVersion = "1.0.0";
        installed.mState   = ItemStateEnum::eInstalled;

        EXPECT_TRUE(statuses.PushBack(installed).IsNone());

        return ErrorEnum::eNone;
    }));

    // The item config read fails, as the item content is broken, and succeeds after the item is repaired.
    EXPECT_CALL(mOCISpec, LoadItemConfig)
        .WillOnce(Return(ErrorEnum::eIntegrity))
        .WillOnce(DoAll(SetArgReferee<1>(mItemConfig), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Invoke([](const imagemanager::UpdateItemInfo& itemInfo) {
        EXPECT_EQ(itemInfo.mID, "item0");
        EXPECT_EQ(itemInfo.mVersion, "1.0.0");

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eActive) << mReceivedStatuses[0];

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RepairsItemWithBrokenConfigsOncePerRequest)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 1, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 2, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke([](Array<UpdateItemStatus>& statuses) {
        UpdateItemStatus installed;

        installed.mItemID  = "item0";
        installed.mVersion = "1.0.0";
        installed.mState   = ItemStateEnum::eInstalled;

        EXPECT_TRUE(statuses.PushBack(installed).IsNone());

        return ErrorEnum::eNone;
    }));

    std::atomic_int brokenReads {static_cast<int>(cStartInstanceInfos.size())};

    // The item config read of each instance fails at the prepare stage, as the item content is broken.
    EXPECT_CALL(mOCISpec, LoadItemConfig)
        .Times(cStartInstanceInfos.size() * 2)
        .WillRepeatedly(Invoke([this, &brokenReads](const String&, oci::ItemConfig& itemConfig) {
            if (brokenReads-- > 0) {
                return Error(ErrorEnum::eIntegrity);
            }

            itemConfig = mItemConfig;

            return Error(ErrorEnum::eNone);
        }));

    // The item is repaired once for all its instances, the start stage doesn't repair it again.
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Invoke([](const imagemanager::UpdateItemInfo& itemInfo) {
        EXPECT_EQ(itemInfo.mID, "item0");
        EXPECT_EQ(itemInfo.mVersion, "1.0.0");

        return ErrorEnum::eNone;
    }));

    std::atomic_int brokenStarts {1};

    EXPECT_CALL(mRuntime0, StartInstance)
        .Times(cStartInstanceInfos.size())
        .WillRepeatedly(Invoke([&brokenStarts](const InstanceInfo& instance, InstanceStatus& status) {
            if (brokenStarts-- > 0) {
                return Error(ErrorEnum::eIntegrity);
            }

            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return Error(ErrorEnum::eNone);
        }));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStartInstanceInfos.size());

    size_t numFailed = 0;

    for (const auto& status : mReceivedStatuses) {
        if (status.mState == InstanceStateEnum::eFailed) {
            EXPECT_TRUE(status.mError.Is(ErrorEnum::eIntegrity)) << status;
            numFailed++;

            continue;
        }

        EXPECT_EQ(status.mState, InstanceStateEnum::eActive) << status;
    }

    EXPECT_EQ(numFailed, 1);

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, ModuleStartRepairDoesNotBlockStatusesAndRequests)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartInstances(&cStoredInfos.front(), cStoredInfos.size());

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    std::promise<void> installStarted;
    std::promise<void> installReleased;
    auto               installReleasedFuture = installReleased.get_future().share();

    EXPECT_CALL(mRuntime0, StartInstance)
        .WillOnce(Return(ErrorEnum::eIntegrity))
        .WillRepeatedly(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    // The repair waits for the CM, which is connected only after the modules started after the launcher.
    EXPECT_CALL(mImageManager, InstallItem)
        .WillOnce(Invoke([&installStarted, installReleasedFuture](const imagemanager::UpdateItemInfo&) {
            installStarted.set_value();
            installReleasedFuture.wait();

            return ErrorEnum::eNone;
        }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(installStarted.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);

    // The statuses are returned while the repair is in progress.
    auto statuses = std::async(std::launch::async, [this]() {
        StaticArray<InstanceStatus, cMaxNumInstances> result;

        EXPECT_TRUE(mLauncher.GetInstancesStatuses(result).IsNone());

        return std::vector<InstanceStatus>(result.begin(), result.end());
    });

    ASSERT_EQ(statuses.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    const auto repairStatuses = statuses.get();

    ASSERT_EQ(repairStatuses.size(), 1);
    EXPECT_EQ(repairStatuses[0].mState, InstanceStateEnum::eFailed);
    EXPECT_TRUE(repairStatuses[0].mError.Is(ErrorEnum::eIntegrity)) << repairStatuses[0];

    // The request received during the repair waits for it instead of being rejected.
    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke(ItemsInstalled({"item0"})));

    auto request = std::async(
        std::launch::async, [this, &cStartInstances]() { return mLauncher.UpdateInstances({}, cStartInstances); });

    EXPECT_EQ(request.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout);

    installReleased.set_value();

    ASSERT_EQ(request.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    err = request.get();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The statuses of the repair, then of the request.
    for (size_t i = 0; i < 2; ++i) {
        err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        ASSERT_EQ(mReceivedStatuses.Size(), 1);
        EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eActive) << mReceivedStatuses[0];
    }

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RepairsBrokenConfigsOnModuleStart)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartInstances(&cStoredInfos.front(), cStoredInfos.size());

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The configs read fails on module start and on the instance start, as the item content is broken. The item is
    // repaired on module start, the configs are loaded and the instance is started.
    EXPECT_CALL(mOCISpec, LoadItemConfig)
        .Times(3)
        .WillOnce(Return(ErrorEnum::eIntegrity))
        .WillOnce(Return(ErrorEnum::eIntegrity))
        .WillOnce(DoAll(SetArgReferee<1>(mItemConfig), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime0, StartInstance)
        .Times(2)
        .WillRepeatedly(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // The statuses are sent when the module start repair is finished.
    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStoredInfos.size());
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eActive) << mReceivedStatuses[0];

    // The configs are loaded, so the next request neither prepares the running instance again nor repairs the item.
    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke(ItemsInstalled({"item0"})));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eActive) << mReceivedStatuses[0];

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, InstallFailureKeepsRunningInstance)
{
    const std::vector cInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 1, "1.0.0", "runtime0"),
    };
    const Error cInstallError(ErrorEnum::eIntegrity, "update item install failed");

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime0, StartInstance)
        .WillRepeatedly(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    err = mLauncher.UpdateInstances({}, Array<InstanceInfo>(cInstanceInfos.data(), 1));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    ASSERT_EQ(mReceivedStatuses[0].mState, InstanceStateEnum::eActive) << mReceivedStatuses[0];

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mImageManager));

    // The item is installed again for the new instance and fails: the running instance keeps running.
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(cInstallError));

    err = mLauncher.UpdateInstances({}, Array<InstanceInfo>(cInstanceInfos.data(), cInstanceInfos.size()));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cInstanceInfos.size());

    for (const auto& status : mReceivedStatuses) {
        if (status.mInstance == cInstanceInfos[0].mInstance) {
            EXPECT_EQ(status.mState, InstanceStateEnum::eActive) << status;
            EXPECT_TRUE(status.mError.IsNone()) << status;
        } else {
            EXPECT_EQ(status.mState, InstanceStateEnum::eFailed) << status;
            EXPECT_EQ(status.mError, cInstallError) << status;
        }
    }

    EXPECT_CALL(mRuntime0, StopInstance).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, ParallelUpdateInstancesDoesNotInterfere)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 1, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartFirstInstance(&cStartInstanceInfos.front(), 1);
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 0);

    std::promise<void> launchPromise;

    EXPECT_CALL(mRuntime0, StartInstance(cStartInstanceInfos[0], _))
        .WillOnce(Invoke([&launchPromise](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            launchPromise.get_future().wait();

            return ErrorEnum::eNone;
        }));

    err = mLauncher.UpdateInstances({}, cStartFirstInstance);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.Is(ErrorEnum::eWrongState)) << tests::utils::ErrorToStr(err);

    launchPromise.set_value();

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0], CreateInstanceStatus(cStartInstanceInfos[0], InstanceStateEnum::eActive));

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cStartInstanceInfos[0]), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, GetInstancesStatusesReturnsEmptyArray)
{
    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto storedData = std::make_unique<InstanceInfoArray>();

    err = mStorage.GetAllInstancesInfos(*storedData);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(storedData->IsEmpty());

    auto statuses = std::make_unique<InstanceStatusArray>();

    err = mLauncher.GetInstancesStatuses(*statuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(statuses->IsEmpty());
}

TEST_F(LauncherTest, GetInstancesStatuses)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 1, "1.0.0", "runtime1"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance(cStartInstanceInfos[0], _))
        .WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mRuntime1, StartInstance(cStartInstanceInfos[1], _))
        .WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eFailed, status);

            return ErrorEnum::eFailed;
        }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto storedData = std::make_unique<InstanceInfoArray>();

    err = mStorage.GetAllInstancesInfos(*storedData);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(storedData->Size(), cStartInstances.Size());

    auto statuses = std::make_unique<InstanceStatusArray>();

    err = mLauncher.GetInstancesStatuses(*statuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(statuses->Size(), cStartInstances.Size());
    EXPECT_EQ((*statuses)[0], CreateInstanceStatus(cStartInstanceInfos[0], InstanceStateEnum::eActive));
    EXPECT_EQ((*statuses)[1], CreateInstanceStatus(cStartInstanceInfos[1], InstanceStateEnum::eFailed));

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cStartInstanceInfos[0]), _))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, StopInstance(static_cast<const InstanceIdent&>(cStartInstanceInfos[1]), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, GetInstanceMonitoringParams)
{
    auto startInstance = CreateInstanceInfo("item0", 0, "1.0.0", "runtime0");
    startInstance.mMonitoringParams.EmplaceValue();
    startInstance.mMonitoringParams->mAlertRules.EmplaceValue();
    startInstance.mMonitoringParams->mAlertRules->mCPU.SetValue({Time::cSeconds, 10.0, 30.0});

    const Array<InstanceInfo> cStartInstances(&startInstance, 1);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance(startInstance, _))
        .WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mRuntime1, StartInstance).Times(0);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto storedData = std::make_unique<InstanceInfoArray>();

    err = mStorage.GetAllInstancesInfos(*storedData);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(storedData->Size(), cStartInstances.Size());

    auto params = std::make_unique<InstanceMonitoringParams>();

    err = mLauncher.GetInstanceMonitoringParams(startInstance, *params);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(startInstance.mMonitoringParams, *params);

    params = std::make_unique<InstanceMonitoringParams>();

    err = mLauncher.GetInstanceMonitoringParams(
        InstanceIdent {"unknown", "", 999, UpdateItemTypeEnum::eService}, *params);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(startInstance), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, GetInstanceMonitoringData)
{
    const auto                cInstanceInfo = CreateInstanceInfo("item0", 0, "1.0.0", "runtime0");
    const Array<InstanceInfo> cStartInstances(&cInstanceInfo, 1);
    const auto                cMonitoringData = CreateMonitoringData(cInstanceInfo);

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance(cInstanceInfo, _))
        .WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    err = mLauncher.UpdateInstances({}, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, GetInstanceMonitoringData(cInstanceInfo, _))
        .WillOnce(
            DoAll(SetArgReferee<1>(monitoring::InstanceMonitoringData(cMonitoringData)), Return(ErrorEnum::eNone)));

    auto instanceMonitoringData = std::make_unique<monitoring::InstanceMonitoringData>();

    err = mLauncher.GetInstanceMonitoringData(cInstanceInfo, *instanceMonitoringData);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(cMonitoringData, *instanceMonitoringData);

    err = mLauncher.GetInstanceMonitoringData(
        InstanceIdent {"unknown", "", 999, UpdateItemTypeEnum::eService}, *instanceMonitoringData);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cInstanceInfo), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, GetRuntimesInfos)
{
    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, GetRuntimeInfo)
        .WillOnce(DoAll(SetArgReferee<0>(CreateRuntimeInfo("runtime0")), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mRuntime1, GetRuntimeInfo)
        .WillOnce(DoAll(SetArgReferee<0>(CreateRuntimeInfo("runtime1")), Return(ErrorEnum::eNone)));

    auto runtimeInfos = std::make_unique<StaticArray<RuntimeInfo, 2>>();

    err = mLauncher.GetRuntimesInfos(*runtimeInfos);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(runtimeInfos->Size(), GetRuntimesArray().Size());
    EXPECT_STREQ((*runtimeInfos)[0].mRuntimeID.CStr(), "runtime0");
    EXPECT_STREQ((*runtimeInfos)[1].mRuntimeID.CStr(), "runtime1");

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, OnInstanceStatusChanged)
{
    const auto cInstanceInfo   = CreateInstanceInfo("item0", 0, "1.0.0", "runtime0");
    const auto cInstanceStatus = CreateInstanceStatus(cInstanceInfo, InstanceStateEnum::eActive);
    const auto cInactiveStatus = CreateInstanceStatus(cInstanceInfo, InstanceStateEnum::eInactive);

    mStorage.Init({cInstanceInfo});

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.SubscribeListener(mStatusListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    const auto stopStatuses = Array<InstanceStatus>(&cInactiveStatus, 1);

    EXPECT_CALL(mStatusListener, OnInstancesStatusesChanged(stopStatuses)).Times(1);

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cInstanceInfo), _))
        .WillOnce(Invoke([this](const InstanceIdent& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eInactive, status);

            mLauncher.OnInstancesStatusesReceived(Array<InstanceStatus>(&status, 1));

            return ErrorEnum::eNone;
        }));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UnsubscribeListener(mStatusListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UnsubscribeListener(mStatusListener);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RebootRuntimeOnStartInstance)
{
    const auto cInstanceInfo   = CreateInstanceInfo("item0", 0, "1.0.0", "runtime0");
    const auto cInstanceStatus = CreateInstanceStatus(cInstanceInfo, InstanceStateEnum::eActive);

    mStorage.Init({cInstanceInfo});

    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([&](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        auto err = mLauncher.RebootRequired("runtime0");
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        return ErrorEnum::eNone;
    }));

    std::promise<void> rebootPromise;

    EXPECT_CALL(mRuntime0, Reboot()).WillOnce(Invoke([&](void) {
        rebootPromise.set_value();

        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);

    EXPECT_EQ(mReceivedStatuses[0], cInstanceStatus);

    const auto statuses = Array<InstanceStatus>(&cInstanceStatus, 1);

    mLauncher.OnInstancesStatusesReceived(statuses);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0], cInstanceStatus);

    EXPECT_TRUE(rebootPromise.get_future().wait_for(std::chrono::milliseconds(cWaitTimeout.Milliseconds()))
        == std::future_status::ready);

    EXPECT_CALL(mRuntime0, StopInstance(static_cast<const InstanceIdent&>(cInstanceInfo), _))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RebootRuntime)
{
    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.SubscribeListener(mStatusListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    std::promise<void> rebootPromise;

    EXPECT_CALL(mRuntime0, Reboot()).WillOnce(Invoke([&](void) {
        rebootPromise.set_value();
        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.RebootRequired("runtime0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.RebootRequired("unknown_runtime");
    ASSERT_TRUE(err.Is(ErrorEnum::eNone)) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(rebootPromise.get_future().wait_for(std::chrono::milliseconds(cWaitTimeout.Milliseconds()))
        == std::future_status::ready);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UnsubscribeListener(mStatusListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, OnInstancesStatusesReceived)
{
    auto err = mLauncher.Init(mAllocator, GetRuntimesArray(), mImageManager, mItemStatusProvider, mSender, mStorage,
        mOCISpec, mItemInfoProvider, mCloudConnection, mNetworkManager, mInstanceIDProvider, mResourceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.SubscribeListener(mStatusListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    std::promise<void> notifyPromise;

    EXPECT_CALL(mStatusListener, OnInstancesStatusesChanged).WillOnce(Invoke([&](const Array<InstanceStatus>&) {
        auto params = std::make_unique<InstanceMonitoringParams>();

        mLauncher.GetInstanceMonitoringParams({}, *params);

        notifyPromise.set_value();
    }));

    err = mLauncher.OnInstancesStatusesReceived({});
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(notifyPromise.get_future().wait_for(std::chrono::milliseconds(cWaitTimeout.Milliseconds()))
        == std::future_status::ready);
}

} // namespace aos::sm::launcher
