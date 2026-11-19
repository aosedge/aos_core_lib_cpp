/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>

#include <gtest/gtest.h>

#include <core/common/tests/mocks/cloudconnectionmock.hpp>
#include <core/common/tests/mocks/instancestatusprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include <core/cm/monitoring/monitoring.hpp>
#include <core/cm/tests/mocks/nodeinfoprovidermock.hpp>

using namespace testing;

namespace aos::cm::monitoring {
/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {
std::unique_ptr<aos::monitoring::NodeMonitoringData> CreateNodeMonitoringData(
    const std::string& nodeID, const Time& timestamp)
{
    auto monitoring = std::make_unique<aos::monitoring::NodeMonitoringData>();

    monitoring->mNodeID                    = nodeID.c_str();
    monitoring->mMonitoringData.mTimestamp = timestamp;

    return monitoring;
}

class SenderStub : public SenderItf {
public:
    Error SendMonitoring(const aos::Monitoring& monitoring) override
    {
        std::lock_guard lock {mMutex};

        mMessages.push_back(monitoring);
        mCondVar.notify_all();

        return mSendErr;
    }

    void SetSendError(const Error& err)
    {
        std::lock_guard lock {mMutex};

        mSendErr = err;
    }

    bool WaitForMessage(aos::Monitoring& message, std::chrono::milliseconds timeout = std::chrono::seconds(5))
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, timeout, [&]() { return !mMessages.empty(); })) {
            return false;
        }

        message = std::move(mMessages.front());
        mMessages.erase(mMessages.begin());

        return true;
    }

    std::vector<aos::Monitoring> GetMessages()
    {
        std::lock_guard lock {mMutex};

        return mMessages;
    }

private:
    std::mutex                   mMutex;
    std::condition_variable      mCondVar;
    std::vector<aos::Monitoring> mMessages;
    Error                        mSendErr;
};

std::unique_ptr<UnitNodeInfo> CreateUnitNodeInfo(const String& nodeID, NodeState state, bool isConnected)
{
    auto unitNodeInfo = std::make_unique<UnitNodeInfo>();

    unitNodeInfo->mNodeID      = nodeID;
    unitNodeInfo->mState       = state;
    unitNodeInfo->mIsConnected = isConnected;

    return unitNodeInfo;
}

std::unique_ptr<InstanceStatus> CreateInstanceStatus(
    const String& nodeID, const InstanceIdent& ident, InstanceStateEnum state)
{
    auto instanceStatus = std::make_unique<InstanceStatus>();

    instanceStatus->mNodeID                      = nodeID;
    static_cast<InstanceIdent&>(*instanceStatus) = ident;
    instanceStatus->mState                       = state;

    return instanceStatus;
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CMMonitoring : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        auto err = mMonitoring.Init(
            Config {Time::cSeconds * 1}, mSender, mCloudConnection, mInstanceStatusProvider, mNodeInfoProvider);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        EXPECT_CALL(mInstanceStatusProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mInstanceStatusProvider, UnsubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));

        EXPECT_CALL(mNodeInfoProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNodeInfoProvider, UnsubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));
    }

    SenderStub                             mSender;
    cloudconnection::CloudConnectionMock   mCloudConnection;
    instancestatusprovider::ProviderMock   mInstanceStatusProvider;
    nodeinfoprovider::NodeInfoProviderMock mNodeInfoProvider;
    Monitoring                             mMonitoring;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CMMonitoring, OnMonitoringReceived)
{
    cloudconnection::ConnectionListenerItf* connectionListener = nullptr;

    EXPECT_CALL(mCloudConnection, SubscribeListener)
        .WillOnce(Invoke([&connectionListener](cloudconnection::ConnectionListenerItf& listener) {
            connectionListener = &listener;

            return ErrorEnum::eNone;
        }));

    auto err = mMonitoring.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    for (size_t i = 0; i < 2; ++i) {
        auto monitoring = CreateNodeMonitoringData(std::to_string(i), Time::Now());

        monitoring->mMonitoringData.mCPU = 50.0;
        monitoring->mMonitoringData.mRAM = 1024 * 4;

        monitoring->mInstances.EmplaceBack();
        monitoring->mInstances[0].mInstanceIdent
            = InstanceIdent {"service1", "1.0.0", "subject1", 1, UpdateItemTypeEnum::eService};
        monitoring->mInstances[0].mMonitoringData.mCPU = 20.0;

        monitoring->mInstances[0].mMonitoringData.mPartitions.EmplaceBack();
        monitoring->mInstances[0].mMonitoringData.mPartitions[0].mName     = "partition1";
        monitoring->mInstances[0].mMonitoringData.mPartitions[0].mUsedSize = 512.0;

        err = mMonitoring.OnMonitoringReceived(*monitoring);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    connectionListener->OnConnect();

    auto monitoring = std::make_unique<aos::Monitoring>();

    EXPECT_TRUE(mSender.WaitForMessage(*monitoring));

    ASSERT_EQ(monitoring->mNodes.Size(), 2);

    for (size_t i = 0; i < 2; ++i) {
        EXPECT_EQ(monitoring->mNodes[i].mNodeID.CStr(), std::to_string(i));
        EXPECT_EQ(monitoring->mNodes[i].mItems.Size(), 1);
        EXPECT_EQ(monitoring->mNodes[i].mItems[0].mCPU, 50.0);
        EXPECT_EQ(monitoring->mNodes[i].mItems[0].mRAM, 1024 * 4);
    }

    ASSERT_EQ(monitoring->mInstances.Size(), 2);

    for (size_t i = 0; i < 2; ++i) {
        const InstanceIdent instanceIdent {"service1", "1.0.0", "subject1", 1, UpdateItemTypeEnum::eService};

        EXPECT_EQ(static_cast<const InstanceIdent&>(monitoring->mInstances[i]), instanceIdent);
        EXPECT_EQ(monitoring->mInstances[i].mItems.Size(), 1);

        EXPECT_EQ(monitoring->mInstances[i].mNodeID.CStr(), std::to_string(i));
        EXPECT_EQ(monitoring->mInstances[i].mItems[0].mCPU, 20.0);

        EXPECT_EQ(monitoring->mInstances[i].mItems[0].mPartitions.Size(), 1);
        EXPECT_EQ(monitoring->mInstances[i].mItems[0].mPartitions[0].mName, "partition1");
        EXPECT_EQ(monitoring->mInstances[i].mItems[0].mPartitions[0].mUsedSize, 512.0);
    }

    EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mMonitoring.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMMonitoring, OnNodeInfoChanged)
{
    cloudconnection::ConnectionListenerItf* connectionListener = nullptr;

    EXPECT_CALL(mCloudConnection, SubscribeListener)
        .WillOnce(Invoke([&connectionListener](cloudconnection::ConnectionListenerItf& listener) {
            connectionListener = &listener;

            return ErrorEnum::eNone;
        }));

    auto err = mMonitoring.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    std::array nodeInfos = {
        CreateUnitNodeInfo("node1", NodeStateEnum::eUnprovisioned, false),
        CreateUnitNodeInfo("node1", NodeStateEnum::eProvisioned, true),
        CreateUnitNodeInfo("node2", NodeStateEnum::eUnprovisioned, false),
        CreateUnitNodeInfo("node2", NodeStateEnum::eProvisioned, true),
    };

    for (const auto& nodeInfo : nodeInfos) {
        mMonitoring.OnNodeInfoChanged(*nodeInfo);
    }

    connectionListener->OnConnect();

    auto monitoring = std::make_unique<aos::Monitoring>();

    EXPECT_TRUE(mSender.WaitForMessage(*monitoring));

    ASSERT_EQ(monitoring->mNodes.Size(), 2);
    EXPECT_EQ(monitoring->mNodes[0].mNodeID, "node1");

    EXPECT_EQ(monitoring->mNodes[0].mStates.Size(), 2);
    EXPECT_EQ(monitoring->mNodes[0].mStates[0].mState.GetValue(), NodeStateEnum::eUnprovisioned);
    EXPECT_EQ(monitoring->mNodes[0].mStates[0].mIsConnected, false);

    EXPECT_EQ(monitoring->mNodes[0].mStates[1].mState.GetValue(), NodeStateEnum::eProvisioned);
    EXPECT_EQ(monitoring->mNodes[0].mStates[1].mIsConnected, true);

    EXPECT_EQ(monitoring->mNodes[1].mNodeID, "node2");
    EXPECT_EQ(monitoring->mNodes[1].mStates.Size(), 2);

    EXPECT_EQ(monitoring->mNodes[1].mStates[0].mIsConnected, false);
    EXPECT_EQ(monitoring->mNodes[1].mStates[0].mState.GetValue(), NodeStateEnum::eUnprovisioned);

    EXPECT_EQ(monitoring->mNodes[1].mStates[1].mIsConnected, true);
    EXPECT_EQ(monitoring->mNodes[1].mStates[1].mState.GetValue(), NodeStateEnum::eProvisioned);

    EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mMonitoring.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMMonitoring, OnInstancesStatusesChanged)
{
    const InstanceIdent ident0 {"itemID", "1.0.0", "subjectID", 0, UpdateItemTypeEnum::eService};

    cloudconnection::ConnectionListenerItf* connectionListener = nullptr;

    EXPECT_CALL(mCloudConnection, SubscribeListener)
        .WillOnce(Invoke([&connectionListener](cloudconnection::ConnectionListenerItf& listener) {
            connectionListener = &listener;

            return ErrorEnum::eNone;
        }));

    auto err = mMonitoring.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    std::array statuses = {
        CreateInstanceStatus("node1", ident0, InstanceStateEnum::eActivating),
        CreateInstanceStatus("node1", ident0, InstanceStateEnum::eActivating),
        CreateInstanceStatus("node1", ident0, InstanceStateEnum::eActive),
        CreateInstanceStatus("node1", ident0, InstanceStateEnum::eInactive),
        CreateInstanceStatus("node2", ident0, InstanceStateEnum::eActivating),
        CreateInstanceStatus("node1", ident0, InstanceStateEnum::eFailed),
    };

    for (const auto& status : statuses) {
        mMonitoring.OnInstancesStatusesChanged(Array<InstanceStatus> {status.get(), 1});
    }

    connectionListener->OnConnect();

    auto monitoring = std::make_unique<aos::Monitoring>();

    EXPECT_TRUE(mSender.WaitForMessage(*monitoring));

    ASSERT_EQ(monitoring->mInstances.Size(), 2);
    EXPECT_EQ(monitoring->mInstances[0].mNodeID, "node1");

    EXPECT_EQ(monitoring->mInstances[0].mStates.Size(), 4);
    EXPECT_EQ(monitoring->mInstances[0].mStates[0].mState.GetValue(), InstanceStateEnum::eActivating);
    EXPECT_EQ(monitoring->mInstances[0].mStates[1].mState.GetValue(), InstanceStateEnum::eActive);
    EXPECT_EQ(monitoring->mInstances[0].mStates[2].mState.GetValue(), InstanceStateEnum::eInactive);
    EXPECT_EQ(monitoring->mInstances[0].mStates[3].mState.GetValue(), InstanceStateEnum::eFailed);

    EXPECT_EQ(monitoring->mInstances[1].mNodeID, "node2");
    EXPECT_EQ(monitoring->mInstances[1].mStates.Size(), 1);
    EXPECT_EQ(monitoring->mInstances[1].mStates[0].mState.GetValue(), InstanceStateEnum::eActivating);

    EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mMonitoring.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMMonitoring, OnPreinstalledInstancesStatusesChanged)
{
    const InstanceIdent ident0 {"itemID", "1.0.0", "subjectID", 0, UpdateItemTypeEnum::eComponent};

    cloudconnection::ConnectionListenerItf* connectionListener = nullptr;

    EXPECT_CALL(mCloudConnection, SubscribeListener)
        .WillOnce(Invoke([&connectionListener](cloudconnection::ConnectionListenerItf& listener) {
            connectionListener = &listener;

            return ErrorEnum::eNone;
        }));

    auto err = mMonitoring.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto ident0Status           = CreateInstanceStatus("node1", ident0, InstanceStateEnum::eActive);
    ident0Status->mPreinstalled = true;

    mMonitoring.OnInstancesStatusesChanged(Array<InstanceStatus> {ident0Status.get(), 1});

    connectionListener->OnConnect();

    auto monitoring = std::make_unique<aos::Monitoring>();

    EXPECT_TRUE(mSender.WaitForMessage(*monitoring));

    ASSERT_EQ(monitoring->mInstances.Size(), 1);
    EXPECT_EQ(monitoring->mInstances[0].mNodeID, "node1");

    EXPECT_EQ(monitoring->mInstances[0].mStates.Size(), 1);
    EXPECT_EQ(monitoring->mInstances[0].mStates[0].mState.GetValue(), InstanceStateEnum::eActive);
    EXPECT_TRUE(monitoring->mInstances[0].mPreinstalled);

    EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mMonitoring.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

class CMMonitoringErrors : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        auto err = mMonitoring.Init(
            Config {Time::cSeconds * 1}, mSender, mCloudConnection, mInstanceStatusProvider, mNodeInfoProvider);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    void Start()
    {
        EXPECT_CALL(mCloudConnection, SubscribeListener)
            .WillOnce(Invoke([this](cloudconnection::ConnectionListenerItf& listener) {
                mConnectionListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mInstanceStatusProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNodeInfoProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));

        auto err = mMonitoring.Start();
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    void Stop()
    {
        EXPECT_CALL(mInstanceStatusProvider, UnsubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mNodeInfoProvider, UnsubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

        auto err = mMonitoring.Stop();
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    void ExpectUnsubscribeErrors()
    {
        EXPECT_CALL(mInstanceStatusProvider, UnsubscribeListener(_)).WillOnce(Return(ErrorEnum::eFailed));
        EXPECT_CALL(mNodeInfoProvider, UnsubscribeListener(_)).WillOnce(Return(ErrorEnum::eFailed));
        EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eFailed));
    }

    SenderStub                                         mSender;
    StrictMock<cloudconnection::CloudConnectionMock>   mCloudConnection;
    StrictMock<instancestatusprovider::ProviderMock>   mInstanceStatusProvider;
    StrictMock<nodeinfoprovider::NodeInfoProviderMock> mNodeInfoProvider;
    cloudconnection::ConnectionListenerItf*            mConnectionListener = nullptr;
    Monitoring                                         mMonitoring;
};

TEST_F(CMMonitoringErrors, StartStopErrors)
{
    EXPECT_EQ(mMonitoring.Stop(), ErrorEnum::eWrongState);

    EXPECT_CALL(mCloudConnection, SubscribeListener).WillOnce(Return(ErrorEnum::eFailed));
    ExpectUnsubscribeErrors();

    EXPECT_TRUE(mMonitoring.Start().Is(ErrorEnum::eFailed));

    EXPECT_CALL(mCloudConnection, SubscribeListener).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mInstanceStatusProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eNoMemory));
    ExpectUnsubscribeErrors();

    EXPECT_TRUE(mMonitoring.Start().Is(ErrorEnum::eNoMemory));

    EXPECT_CALL(mCloudConnection, SubscribeListener).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mInstanceStatusProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mNodeInfoProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eTimeout));
    ExpectUnsubscribeErrors();

    EXPECT_TRUE(mMonitoring.Start().Is(ErrorEnum::eTimeout));

    Start();

    EXPECT_EQ(mMonitoring.Start(), ErrorEnum::eWrongState);

    // Unsubscribe errors on stop are ignored

    ExpectUnsubscribeErrors();

    auto err = mMonitoring.Stop();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMMonitoringErrors, NodeCapacityExceeded)
{
    Start();

    // Same state is not duplicated, states are rotated when full

    mMonitoring.OnNodeInfoChanged(*CreateUnitNodeInfo("node0", NodeStateEnum::eProvisioned, true));
    mMonitoring.OnNodeInfoChanged(*CreateUnitNodeInfo("node0", NodeStateEnum::eProvisioned, true));

    for (size_t i = 0; i < cMonitoringItemsCount + 1; ++i) {
        mMonitoring.OnNodeInfoChanged(*CreateUnitNodeInfo("node0", NodeStateEnum::eProvisioned, i % 2 == 0));
    }

    for (size_t i = 0; i < cMonitoringItemsCount + 1; ++i) {
        auto monitoring = CreateNodeMonitoringData("node0", Time::Now());

        monitoring->mMonitoringData.mRAM = i;

        auto err = mMonitoring.OnMonitoringReceived(*monitoring);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    for (size_t i = 1; i < cMaxNumNodes; ++i) {
        mMonitoring.OnNodeInfoChanged(
            *CreateUnitNodeInfo(("node" + std::to_string(i)).c_str(), NodeStateEnum::eProvisioned, true));
    }

    mMonitoring.OnNodeInfoChanged(*CreateUnitNodeInfo("extraNode", NodeStateEnum::eProvisioned, true));

    auto err = mMonitoring.OnMonitoringReceived(*CreateNodeMonitoringData("extraNode", Time::Now()));
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    mConnectionListener->OnConnect();

    auto monitoring = std::make_unique<aos::Monitoring>();

    ASSERT_TRUE(mSender.WaitForMessage(*monitoring));

    ASSERT_EQ(monitoring->mNodes.Size(), cMaxNumNodes);
    EXPECT_EQ(monitoring->mNodes[0].mNodeID, "node0");
    EXPECT_EQ(monitoring->mNodes[0].mStates.Size(), cMonitoringItemsCount);
    EXPECT_TRUE(monitoring->mNodes[0].mStates.Back().mIsConnected);
    EXPECT_FALSE(monitoring->mNodes[0].mStates.Front().mIsConnected);
    ASSERT_EQ(monitoring->mNodes[0].mItems.Size(), cMonitoringItemsCount);
    EXPECT_EQ(monitoring->mNodes[0].mItems[0].mRAM, 1);

    Stop();
}

TEST_F(CMMonitoringErrors, InstanceCapacityExceeded)
{
    Start();

    const InstanceIdent ident0 {"itemID", "1.0.0", "subjectID", 0, UpdateItemTypeEnum::eService};

    for (size_t i = 0; i < cMonitoringItemsCount + 1; ++i) {
        auto status = CreateInstanceStatus(
            "node0", ident0, i % 2 == 0 ? InstanceStateEnum::eActive : InstanceStateEnum::eInactive);

        mMonitoring.OnInstancesStatusesChanged(Array<InstanceStatus> {status.get(), 1});
    }

    for (size_t i = 0; i < cMonitoringItemsCount + 1; ++i) {
        auto monitoring = CreateNodeMonitoringData("node0", Time::Now());

        monitoring->mInstances.EmplaceBack();
        monitoring->mInstances[0].mInstanceIdent       = ident0;
        monitoring->mInstances[0].mMonitoringData.mRAM = i;

        auto err = mMonitoring.OnMonitoringReceived(*monitoring);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    std::vector<InstanceStatus> statuses(cMaxNumInstances);

    for (size_t i = 0; i < statuses.size(); ++i) {
        statuses[i].mNodeID    = "node0";
        statuses[i].mItemID    = "itemID";
        statuses[i].mSubjectID = "subjectID";
        statuses[i].mInstance  = i + 1;
        statuses[i].mState     = InstanceStateEnum::eActive;
    }

    mMonitoring.OnInstancesStatusesChanged(Array<InstanceStatus> {statuses.data(), statuses.size()});

    auto monitoring = CreateNodeMonitoringData("node0", Time::Now());

    monitoring->mInstances.EmplaceBack();
    monitoring->mInstances[0].mInstanceIdent
        = InstanceIdent {"itemID", "1.0.0", "subjectID", 1000, UpdateItemTypeEnum::eService};

    auto err = mMonitoring.OnMonitoringReceived(*monitoring);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    mConnectionListener->OnConnect();

    auto message = std::make_unique<aos::Monitoring>();

    ASSERT_TRUE(mSender.WaitForMessage(*message));

    ASSERT_EQ(message->mInstances.Size(), cMaxNumInstances);
    EXPECT_EQ(static_cast<const InstanceIdent&>(message->mInstances[0]), ident0);
    EXPECT_EQ(message->mInstances[0].mStates.Size(), cMonitoringItemsCount);
    ASSERT_EQ(message->mInstances[0].mItems.Size(), cMonitoringItemsCount);
    EXPECT_EQ(message->mInstances[0].mItems[0].mRAM, 1);

    Stop();
}

TEST_F(CMMonitoringErrors, SendErrorKeepsData)
{
    Start();

    mConnectionListener->OnDisconnect();

    mSender.SetSendError(ErrorEnum::eFailed);

    auto err = mMonitoring.OnMonitoringReceived(*CreateNodeMonitoringData("node0", Time::Now()));
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    mConnectionListener->OnConnect();

    auto message = std::make_unique<aos::Monitoring>();

    ASSERT_TRUE(mSender.WaitForMessage(*message));
    EXPECT_EQ(message->mNodes.Size(), 1);

    mSender.SetSendError(ErrorEnum::eNone);

    // Data is not cleared on send error, so it is sent again

    ASSERT_TRUE(mSender.WaitForMessage(*message));
    ASSERT_EQ(message->mNodes.Size(), 1);
    EXPECT_EQ(message->mNodes[0].mNodeID, "node0");

    Stop();
}

} // namespace aos::cm::monitoring
