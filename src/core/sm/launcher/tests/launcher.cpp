/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <future>
#include <list>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/monitoring/monitoring.hpp>
#include <core/common/tests/mocks/instancestatusprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/sm/launcher/itf/rebooter.hpp>
#include <core/sm/launcher/itf/updatechecker.hpp>
#include <core/sm/launcher/launcher.hpp>

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

InstanceInfo CreateInstanceInfo(const String& itemID, uint64_t instance, const String& version, const String& runtimeID)
{
    InstanceInfo info;

    info.mItemID    = itemID;
    info.mSubjectID = "";
    info.mInstance  = instance;
    info.mVersion   = version;
    info.mRuntimeID = runtimeID;

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

        EXPECT_CALL(mRuntime1, Start).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mRuntime1, Stop).WillRepeatedly(Return(ErrorEnum::eNone));
        EXPECT_CALL(mRuntime1, GetRuntimeInfo)
            .WillRepeatedly(DoAll(SetArgReferee<0>(CreateRuntimeInfo("runtime1")), Return(ErrorEnum::eNone)));
    }

    StaticArray<RuntimeItf*, cMaxNumNodeRuntimes> GetRuntimesArray()
    {
        StaticArray<RuntimeItf*, cMaxNumNodeRuntimes> runtimes;

        runtimes.PushBack(&mRuntime0);
        runtimes.PushBack(&mRuntime1);

        return runtimes;
    }

    Launcher mLauncher;

    StrictMock<RuntimeMock>                  mRuntime0;
    StrictMock<RuntimeMock>                  mRuntime1;
    NiceMock<imagemanager::ImageManagerMock> mImageManager;
    StorageStub                              mStorage;
    SenderStub                               mSender;
    instancestatusprovider::ListenerMock     mStatusListener;
    InstanceStatusArray                      mReceivedStatuses;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(LauncherTest, NoStoredInstancesOnModuleStart)
{
    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, SendActiveComponentNodeInstancesStatusOnModuleStart)
{
    const std::vector cRuntime0Components = {
        CreateInstanceStatus(CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"), InstanceStateEnum::eActive,
            UpdateItemTypeEnum::eComponent),
        CreateInstanceStatus(CreateInstanceInfo("item1", 1, "1.0.0", "runtime0"), InstanceStateEnum::eInactive,
            UpdateItemTypeEnum::eComponent, true),
        CreateInstanceStatus(CreateInstanceInfo("item2", 2, "1.0.0", "runtime0"), InstanceStateEnum::eActive,
            UpdateItemTypeEnum::eService),
    };

    const auto cPreinstalledComponent = static_cast<const InstanceIdent&>(cRuntime0Components[1]);

    EXPECT_CALL(mRuntime0, Start).WillOnce(Invoke([&]() {
        mLauncher.OnInstancesStatusesReceived(
            Array<InstanceStatus>(cRuntime0Components.data(), cRuntime0Components.size()));

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([&](const InstanceInfo& info, InstanceStatus& status) {
        EXPECT_EQ(static_cast<const InstanceIdent&>(info), cPreinstalledComponent);

        status = cRuntime0Components[1];

        return ErrorEnum::eNone;
    }));

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0], cRuntime0Components[1]);

    EXPECT_CALL(mRuntime0, StopInstance(cPreinstalledComponent, _)).Times(0);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, DoNotSendUpdateInstancesStatusesBeforeModuleStart)
{
    const std::vector cRuntime0Components = {
        CreateInstanceStatus(CreateInstanceInfo("item1", 1, "1.0.0", "runtime0"), InstanceStateEnum::eInactive,
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

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mImageManager, GetAllInstalledItems).WillOnce(Return(ErrorEnum::eNone));

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

    EXPECT_CALL(mImageManager, RemoveUpdateItem(cStoredInfos[0].mItemID, cStoredInfos[0].mVersion))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mImageManager, InstallUpdateItem)
        .WillRepeatedly(Invoke([&](const imagemanager::UpdateItemInfo& itemInfo) {
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
    startInstanceInfos[0].mNetworkParameters.EmplaceValue();
    startInstanceInfos[0].mNetworkParameters->mIP = "newIP";

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mImageManager, GetAllInstalledItems).WillOnce(Return(ErrorEnum::eNone));

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

TEST_F(LauncherTest, ParallelUpdateInstancesDoesNotInterfere)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo("item0", 0, "1.0.0", "runtime0"),
        CreateInstanceInfo("item0", 1, "1.0.0", "runtime0"),
    };
    const Array<InstanceInfo> cStartFirstInstance(&cStartInstanceInfos.front(), 1);
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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
    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance(cStartInstanceInfos[0], _))
        .WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mRuntime1, StartInstance(cStartInstanceInfos[1], _))
        .WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
            SetInstanceStatus(instance, InstanceStateEnum::eFailed, status);

            return ErrorEnum::eNone;
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

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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
    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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

    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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
    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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
    auto err = mLauncher.Init(GetRuntimesArray(), mImageManager, mSender, mStorage);
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
