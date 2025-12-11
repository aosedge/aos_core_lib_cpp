/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <future>
#include <list>
#include <queue>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include <core/sm/launcher/launcher.hpp>

#include <core/sm/launcher/itf/rebooter.hpp>
#include <core/sm/launcher/itf/updatechecker.hpp>

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
    return os << "{" << static_cast<const InstanceIdent&>(status) << ":" << status.mRuntimeID.CStr() << ":"
              << status.mState.ToString().CStr() << ":" << tests::utils::ErrorToStr(status.mError) << "}";
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

InstanceInfo CreateInstanceInfo(uint64_t instance, const String& runtimeID)
{
    InstanceInfo info;

    info.mItemID    = "";
    info.mSubjectID = "";
    info.mInstance  = instance;
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

InstanceStatus CreateInstanceStatus(const InstanceIdent& instance, const String& runtimeID, InstanceStateEnum state)
{
    InstanceStatus status;

    SetInstanceStatus(instance, state, status);
    status.mRuntimeID = runtimeID;

    return status;
}

InstanceStatus CreateInstanceStatus(const InstanceInfo& instance, InstanceStateEnum state)
{
    return CreateInstanceStatus(static_cast<const InstanceIdent&>(instance), instance.mRuntimeID, state);
}

monitoring::InstanceMonitoringData CreateMonitoringData(const InstanceInfo& instance)
{
    monitoring::InstanceMonitoringData data;

    data.mInstanceIdent = static_cast<const InstanceIdent&>(instance);
    data.mRuntimeID     = instance.mRuntimeID;

    return data;
}

/***********************************************************************************************************************
 * Stubs
 **********************************************************************************************************************/

class StorageStub : public StorageItf {
public:
    Error Init(const std::vector<InstanceInfo>& data)
    {
        std::lock_guard lock {mMutex};

        for (const auto& instance : data) {
            if (auto err = mData.PushBack(instance); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        return ErrorEnum::eNone;
    }

    Error GetAllInstancesInfos(Array<InstanceInfo>& infos) override
    {
        std::lock_guard lock {mMutex};

        return infos.Assign(mData);
    }

    Error AddInstanceInfo(const InstanceInfo& info) override
    {
        std::lock_guard lock {mMutex};

        if (mData.ContainsIf([&info](const InstanceInfo& existingInfo) {
                return static_cast<const InstanceIdent&>(existingInfo) == static_cast<const InstanceIdent&>(info);
            })) {
            return ErrorEnum::eAlreadyExist;
        }

        return mData.PushBack(info);
    }

    Error RemoveInstanceInfo(const InstanceIdent& ident) override
    {
        std::lock_guard lock {mMutex};

        return mData.RemoveIf(
                   [&ident](const InstanceInfo& info) { return static_cast<const InstanceIdent&>(info) == ident; })
            ? ErrorEnum::eNone
            : ErrorEnum::eNotFound;
    }

private:
    std::mutex        mMutex;
    InstanceInfoArray mData;
};

class SenderStub : public SenderItf {
public:
    Error SendNodeInstancesStatuses(const Array<aos::InstanceStatus>& statuses) override
    {
        std::lock_guard lock {mMutex};

        mStatusesQueue.push(statuses);
        mCondVar.notify_one();

        return ErrorEnum::eNone;
    }

    Error SendUpdateInstancesStatuses(const Array<aos::InstanceStatus>& statuses) override
    {
        std::lock_guard lock {mMutex};

        mStatusesQueue.push(statuses);
        mCondVar.notify_one();

        return ErrorEnum::eNone;
    }

    Error WaitStatuses(Array<InstanceStatus>& statuses, Duration timeout)
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, std::chrono::milliseconds(timeout.Milliseconds()),
                [this]() { return !mStatusesQueue.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        statuses = mStatusesQueue.front();
        mStatusesQueue.pop();

        return ErrorEnum::eNone;
    }

private:
    std::mutex                      mMutex;
    std::condition_variable         mCondVar;
    std::queue<InstanceStatusArray> mStatusesQueue;
};

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

class RuntimeMock : public RuntimeItf {
public:
    MOCK_METHOD(Error, GetInstanceMonitoringData,
        (const InstanceIdent& instanceIdent, monitoring::InstanceMonitoringData& monitoringData), (override));
    MOCK_METHOD(Error, Start, (), (override));
    MOCK_METHOD(Error, Stop, (), (override));
    MOCK_METHOD(Error, GetRuntimeInfo, (RuntimeInfo & runtimeInfo), (const, override));
    MOCK_METHOD(Error, StartInstance, (const InstanceInfo& instance, InstanceStatus& status), (override));
    MOCK_METHOD(Error, StopInstance, (const InstanceIdent& instance, InstanceStatus& status), (override));
    MOCK_METHOD(Error, Reboot, (), (override));
};

class InstanceListenerMock : public instancestatusprovider::ListenerItf {
public:
    MOCK_METHOD(void, OnInstancesStatusesChanged, (const Array<InstanceStatus>& statuses), (override));
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class LauncherTest : public Test {
protected:
    static constexpr auto cWaitTimeout = Time::cSeconds;

    void SetUp() override
    {
        tests::utils::InitLog();

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

    RuntimeMock          mRuntime0;
    RuntimeMock          mRuntime1;
    StorageStub          mStorage;
    SenderStub           mSender;
    InstanceListenerMock mStatusListener;
    InstanceStatusArray  mReceivedStatuses;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(LauncherTest, NoStoredInstancesOnModuleStart)
{
    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, LauncherStartsStoredInstancesOnModuleStart)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo(0, "runtime0"),
        CreateInstanceInfo(1, "runtime1"),
    };
    const std::vector cExpectedStatuses = {
        CreateInstanceStatus(cStoredInfos[0], InstanceStateEnum::eActive),
        CreateInstanceStatus(cStoredInfos[1], InstanceStateEnum::eActive),
    };

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
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

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStoredInfos.size());

    for (size_t i = 0; i < mReceivedStatuses.Size(); ++i) {
        EXPECT_EQ(mReceivedStatuses[i], CreateInstanceStatus(cStoredInfos[i], InstanceStateEnum::eActive));
    }

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, UpdateInstances)
{
    const std::vector cStoredInfos = {
        CreateInstanceInfo(0, "runtime0"),
    };
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo(1, "runtime0"),
        CreateInstanceInfo(2, "runtime1"),
    };
    const Array<InstanceInfo>  cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());
    const Array<InstanceIdent> cStopInstances(&static_cast<const InstanceIdent&>(cStoredInfos.front()), 1);

    mStorage.Init(cStoredInfos);

    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
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

    err = mLauncher.UpdateInstances(cStopInstances, cStartInstances);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), cStopInstances.Size() + cStartInstances.Size());

    size_t i = 0;
    for (size_t j = 0; j < cStopInstances.Size(); ++j, ++i) {
        EXPECT_EQ(mReceivedStatuses[i],
            CreateInstanceStatus(cStopInstances[j], cStoredInfos[j].mRuntimeID, InstanceStateEnum::eInactive));
    }

    for (size_t j = 0; j < cStartInstances.Size(); ++j, ++i) {
        EXPECT_EQ(mReceivedStatuses[i], CreateInstanceStatus(cStartInstances[j], InstanceStateEnum::eActive));
    }

    auto storedData = std::make_unique<InstanceInfoArray>();

    err = mStorage.GetAllInstancesInfos(*storedData);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    storedData->Sort([](const InstanceInfo& a, const InstanceInfo& b) {
        return static_cast<const InstanceIdent&>(a) < static_cast<const InstanceIdent&>(b);
    });

    EXPECT_EQ(*storedData, Array<InstanceInfo>(&cStartInstanceInfos.front(), cStartInstanceInfos.size()));

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, ParallelUpdateInstancesDoesNotInterfere)
{
    const std::vector cStartInstanceInfos = {
        CreateInstanceInfo(0, "runtime0"),
        CreateInstanceInfo(1, "runtime0"),
    };
    const Array<InstanceInfo> cStartFirstInstance(&cStartInstanceInfos.front(), 1);
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.GetInstancesStatuses(mReceivedStatuses);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

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

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, GetInstancesStatusesReturnsEmptyArray)
{
    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
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
        CreateInstanceInfo(0, "runtime0"),
        CreateInstanceInfo(1, "runtime1"),
    };
    const Array<InstanceInfo> cStartInstances(&cStartInstanceInfos.front(), cStartInstanceInfos.size());

    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
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

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, GetInstanceMonitoringParams)
{

    auto startInstance = CreateInstanceInfo(0, "runtime0");
    startInstance.mMonitoringParams.EmplaceValue();
    startInstance.mMonitoringParams->mAlertRules.EmplaceValue();
    startInstance.mMonitoringParams->mAlertRules->mCPU.SetValue({Time::cSeconds, 10.0, 30.0});

    const Array<InstanceInfo> cStartInstances(&startInstance, 1);

    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
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

    auto params = std::make_unique<Optional<InstanceMonitoringParams>>();

    err = mLauncher.GetInstanceMonitoringParams(startInstance, *params);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(startInstance.mMonitoringParams, *params);

    params = std::make_unique<Optional<InstanceMonitoringParams>>();

    err = mLauncher.GetInstanceMonitoringParams(
        InstanceIdent {"unknown", "", 999, UpdateItemTypeEnum::eService}, *params);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, GetInstanceMonitoringData)
{
    const auto                cInstanceInfo = CreateInstanceInfo(0, "runtime0");
    const Array<InstanceInfo> cStartInstances(&cInstanceInfo, 1);
    const auto                cMonitoringData = CreateMonitoringData(cInstanceInfo);

    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
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

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, GetRuntimesInfos)
{
    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
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
    const auto cInstanceInfo   = CreateInstanceInfo(0, "runtime0");
    const auto cInstanceStatus = CreateInstanceStatus(cInstanceInfo, InstanceStateEnum::eActive);

    mStorage.Init({cInstanceInfo});

    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.SubscribeListener(mStatusListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.SubscribeListener(mStatusListener);
    ASSERT_TRUE(err.Is(ErrorEnum::eAlreadyExist)) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        return ErrorEnum::eNone;
    }));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);

    EXPECT_EQ(mReceivedStatuses[0], cInstanceStatus);

    const auto statuses = Array<InstanceStatus>(&cInstanceStatus, 1);

    EXPECT_CALL(mStatusListener, OnInstancesStatusesChanged(statuses)).Times(1);

    mLauncher.OnInstancesStatusesReceived(statuses);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0], cInstanceStatus);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UnsubscribeListener(mStatusListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UnsubscribeListener(mStatusListener);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RebootRuntimeOnStartInstance)
{
    const auto cInstanceInfo   = CreateInstanceInfo(0, "runtime0");
    const auto cInstanceStatus = CreateInstanceStatus(cInstanceInfo, InstanceStateEnum::eActive);

    mStorage.Init({cInstanceInfo});

    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mRuntime0, StartInstance).WillOnce(Invoke([&](const InstanceInfo& instance, InstanceStatus& status) {
        SetInstanceStatus(instance, InstanceStateEnum::eActive, status);

        auto err = mLauncher.RebootRequired("runtime0");
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mRuntime0, Reboot()).WillOnce(Return(ErrorEnum::eNone));

    err = mLauncher.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);

    EXPECT_EQ(mReceivedStatuses[0], cInstanceStatus);

    const auto statuses = Array<InstanceStatus>(&cInstanceStatus, 1);

    mLauncher.OnInstancesStatusesReceived(statuses);

    err = mSender.WaitStatuses(mReceivedStatuses, cWaitTimeout);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(mReceivedStatuses.Size(), 1);
    EXPECT_EQ(mReceivedStatuses[0], cInstanceStatus);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(LauncherTest, RebootRuntime)
{
    auto err = mLauncher.Init(GetRuntimesArray(), mSender, mStorage);
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

    err = mLauncher.RebootRequired("runtime0");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.RebootRequired("unknown_runtime");
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(rebootPromise.get_future().wait_for(std::chrono::milliseconds(cWaitTimeout.Milliseconds()))
        == std::future_status::ready);

    err = mLauncher.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mLauncher.UnsubscribeListener(mStatusListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

} // namespace aos::sm::launcher
