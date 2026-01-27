/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <core/common/tests/stubs/nodeinfoproviderstub.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include <core/cm/nodeinfoprovider/nodeinfoprovider.hpp>

using namespace testing;

namespace aos::cm::nodeinfoprovider {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

std::unique_ptr<NodeInfo> CreateNodeInfo(
    const String& nodeID, const NodeStateEnum state, bool isConnected, bool hasSMComponent = true)
{
    auto nodeInfo = std::make_unique<NodeInfo>();

    nodeInfo->mNodeID      = nodeID;
    nodeInfo->mState       = state;
    nodeInfo->mIsConnected = isConnected;

    if (hasSMComponent) {
        nodeInfo->mAttrs.PushBack({cAttrAosComponents, "sm"});
    }

    return nodeInfo;
}

/***********************************************************************************************************************
 * Stubs
 **********************************************************************************************************************/

class NodeInfoListenerStub : public NodeInfoListenerItf {
public:
    void OnNodeInfoChanged(const UnitNodeInfo& info) override
    {
        std::lock_guard lock {mMutex};

        LOG_DBG() << "Received node info change" << Log::Field("nodeID", info.mNodeID)
                  << Log::Field("state", info.mState);

        mReceivedInfos.push_back(info);

        mCondVar.notify_all();
    }

    bool Wait(const Duration& timeout, UnitNodeInfo& info)
    {
        std::unique_lock<std::mutex> lock {mMutex};

        if (!mCondVar.wait_for(lock, std::chrono::milliseconds(timeout.Milliseconds()),
                [this]() { return !mReceivedInfos.empty(); })) {
            return false;
        }

        info = std::move(mReceivedInfos.front());
        mReceivedInfos.erase(mReceivedInfos.begin());

        return true;
    }

private:
    std::mutex                mMutex;
    std::condition_variable   mCondVar;
    std::vector<UnitNodeInfo> mReceivedInfos;
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CMNodeInfoProviderTest : public Test {
protected:
    void SetUp() override { tests::utils::InitLog(); }

    iamclient::NodeInfoProviderStub mIAMNodeInfoProvider;
    NodeInfoListenerStub            mListener;
    Config                          mConfig {Time::cMilliseconds * 100};
    NodeInfoProvider                mNodeInfoProvider;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CMNodeInfoProviderTest, KnownInstancesAreProcessedOnStart)
{
    mConfig.mSMConnectionTimeout = Time::cSeconds;

    const std::array cNodes = {
        CreateNodeInfo("node1", NodeStateEnum::eProvisioned, true),
        CreateNodeInfo("node2", NodeStateEnum::eProvisioned, true),
    };

    for (const auto& node : cNodes) {
        mIAMNodeInfoProvider.SetNodeInfo(*node);
    }

    auto err = mNodeInfoProvider.Init(mConfig, mIAMNodeInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.SubscribeListener(mListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    for (const auto& node : cNodes) {
        err = mNodeInfoProvider.OnSMInfoReceived(SMInfo {node->mNodeID, {}, {}});
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    for (const auto& node : cNodes) {
        auto receivedInfo = std::make_unique<UnitNodeInfo>();

        EXPECT_TRUE(mListener.Wait(2 * mConfig.mSMConnectionTimeout, *receivedInfo))
            << "Timeout waiting for node info change";
        EXPECT_STREQ(receivedInfo->mNodeID.CStr(), node->mNodeID.CStr());
        EXPECT_EQ(receivedInfo->mState, NodeStateEnum::eProvisioned);
        EXPECT_TRUE(receivedInfo->mIsConnected);
    }

    err = mNodeInfoProvider.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMNodeInfoProviderTest, GetAllNodeIDs)
{
    auto ids = std::make_unique<StaticArray<StaticString<cIDLen>, cMaxNumNodes>>();

    auto err = mNodeInfoProvider.GetAllNodeIDs(*ids);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_EQ(ids->Size(), 0);

    mIAMNodeInfoProvider.SetNodeInfo(*CreateNodeInfo("node1", NodeStateEnum::eProvisioned, true));
    mIAMNodeInfoProvider.SetNodeInfo(*CreateNodeInfo("node2", NodeStateEnum::eProvisioned, true));

    err = mNodeInfoProvider.Init(mConfig, mIAMNodeInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.GetAllNodeIDs(*ids);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(ids->Size(), 2);
    EXPECT_STREQ((*ids)[0].CStr(), "node1");
    EXPECT_STREQ((*ids)[1].CStr(), "node2");

    err = mNodeInfoProvider.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMNodeInfoProviderTest, GetNodeInfo)
{
    mIAMNodeInfoProvider.SetNodeInfo(*CreateNodeInfo("node1", NodeStateEnum::eProvisioned, true));
    mIAMNodeInfoProvider.SetNodeInfo(*CreateNodeInfo("node2", NodeStateEnum::eProvisioned, true));

    auto err = mNodeInfoProvider.Init(Config {Time::cDay}, mIAMNodeInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto nodeInfo = std::make_unique<UnitNodeInfo>();

    err = mNodeInfoProvider.GetNodeInfo("node1", *nodeInfo);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_STREQ(nodeInfo->mNodeID.CStr(), "node1");
    EXPECT_EQ(nodeInfo->mState, NodeStateEnum::eProvisioned);
    EXPECT_FALSE(nodeInfo->mIsConnected);

    err = mNodeInfoProvider.GetNodeInfo("node2", *nodeInfo);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    EXPECT_STREQ(nodeInfo->mNodeID.CStr(), "node2");
    EXPECT_EQ(nodeInfo->mState, NodeStateEnum::eProvisioned);
    EXPECT_FALSE(nodeInfo->mIsConnected);

    err = mNodeInfoProvider.GetNodeInfo("node3", *nodeInfo);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMNodeInfoProviderTest, NodeWithoutSMComponent)
{
    auto nodeInfo = CreateNodeInfo("node1", NodeStateEnum::eProvisioned, true, false);

    auto err = mNodeInfoProvider.Init(mConfig, mIAMNodeInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.SubscribeListener(mListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    mIAMNodeInfoProvider.SetNodeInfo(*nodeInfo);
    mIAMNodeInfoProvider.NotifyNodeInfoChanged(*nodeInfo);

    auto receivedInfo = std::make_unique<UnitNodeInfo>();
    EXPECT_TRUE(mListener.Wait(Time::cSeconds * 2, *receivedInfo)) << "Timeout waiting for node info change";
    EXPECT_STREQ(receivedInfo->mNodeID.CStr(), nodeInfo->mNodeID.CStr());
    EXPECT_EQ(receivedInfo->mState, NodeStateEnum::eProvisioned);
    EXPECT_TRUE(receivedInfo->mIsConnected);

    err = mNodeInfoProvider.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMNodeInfoProviderTest, NodeWithSMComponent)
{
    const std::array cNodeInfos = {
        CreateNodeInfo("node1", NodeStateEnum::eProvisioned, false),
        CreateNodeInfo("node2", NodeStateEnum::eError, false),
    };

    auto err = mNodeInfoProvider.Init(mConfig, mIAMNodeInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.SubscribeListener(mListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    for (const auto& nodeInfo : cNodeInfos) {
        mIAMNodeInfoProvider.SetNodeInfo(*nodeInfo);
        mIAMNodeInfoProvider.NotifyNodeInfoChanged(*nodeInfo);

        auto receivedInfo = std::make_unique<UnitNodeInfo>();
        EXPECT_TRUE(mListener.Wait(Time::cSeconds * 2, *receivedInfo)) << "Timeout waiting for node info change";
        EXPECT_STREQ(receivedInfo->mNodeID.CStr(), nodeInfo->mNodeID.CStr());
        EXPECT_EQ(receivedInfo->mState, NodeStateEnum::eError);
        EXPECT_FALSE(receivedInfo->mIsConnected);

        const auto onlineNodeInfo = CreateNodeInfo(nodeInfo->mNodeID, NodeStateEnum::eProvisioned, true);

        mIAMNodeInfoProvider.SetNodeInfo(*onlineNodeInfo);
        mIAMNodeInfoProvider.NotifyNodeInfoChanged(*onlineNodeInfo);

        // No SM info received yet, so state should be error on timeout expiry

        receivedInfo = std::make_unique<UnitNodeInfo>();
        EXPECT_TRUE(mListener.Wait(Time::cSeconds * 2, *receivedInfo)) << "Timeout waiting for node info change";
        EXPECT_STREQ(receivedInfo->mNodeID.CStr(), nodeInfo->mNodeID.CStr());
        EXPECT_EQ(receivedInfo->mState, NodeStateEnum::eError);

        // Now SM info is received, state should change to the IAM provided one (online)

        err = mNodeInfoProvider.OnSMInfoReceived(SMInfo {nodeInfo->mNodeID, {}, {}});
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
        receivedInfo = std::make_unique<UnitNodeInfo>();
        EXPECT_TRUE(mListener.Wait(Time::cSeconds * 2, *receivedInfo)) << "Timeout waiting for node info change";
        EXPECT_STREQ(receivedInfo->mNodeID.CStr(), nodeInfo->mNodeID.CStr());
        EXPECT_EQ(receivedInfo->mState, NodeStateEnum::eProvisioned);
        EXPECT_TRUE(receivedInfo->mIsConnected);

        // On SM disconnection, an immediate notification should be sent with isConnected = false

        mNodeInfoProvider.OnSMDisconnected(nodeInfo->mNodeID, ErrorEnum::eNone);
        receivedInfo = std::make_unique<UnitNodeInfo>();
        EXPECT_TRUE(mListener.Wait(Time::cSeconds * 2, *receivedInfo)) << "Timeout waiting for node info change";
        EXPECT_STREQ(receivedInfo->mNodeID.CStr(), nodeInfo->mNodeID.CStr());
        EXPECT_FALSE(receivedInfo->mIsConnected);
    }

    err = mNodeInfoProvider.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMNodeInfoProviderTest, NodeSetToOnlineIfBothIAMAndSMAreReceived)
{
    mConfig.mSMConnectionTimeout = 2 * Time::cSeconds;

    const auto cWaitTimedout  = mConfig.mSMConnectionTimeout + Time::cSeconds;
    const auto cWaitImmediate = Time::cSeconds / 10;

    auto cNodeInfo = CreateNodeInfo("node1", NodeStateEnum::eProvisioned, false);

    auto err = mNodeInfoProvider.Init(mConfig, mIAMNodeInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.SubscribeListener(mListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto receivedInfo = std::make_unique<UnitNodeInfo>();

    err = mNodeInfoProvider.OnSMInfoReceived(SMInfo {cNodeInfo->mNodeID, {}, {}});
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mListener.Wait(cWaitTimedout, *receivedInfo)) << "Timeout waiting for node info change";
    EXPECT_STREQ(receivedInfo->mNodeID.CStr(), cNodeInfo->mNodeID.CStr());
    EXPECT_EQ(receivedInfo->mState, NodeStateEnum::eError);
    EXPECT_FALSE(receivedInfo->mIsConnected);

    mIAMNodeInfoProvider.SetNodeInfo(*cNodeInfo);
    mIAMNodeInfoProvider.NotifyNodeInfoChanged(*cNodeInfo);

    receivedInfo = std::make_unique<UnitNodeInfo>();
    EXPECT_TRUE(mListener.Wait(cWaitImmediate, *receivedInfo)) << "Timeout waiting for node info change";
    EXPECT_STREQ(receivedInfo->mNodeID.CStr(), cNodeInfo->mNodeID.CStr());
    EXPECT_EQ(receivedInfo->mState, NodeStateEnum::eProvisioned);
    EXPECT_TRUE(receivedInfo->mIsConnected);

    err = mNodeInfoProvider.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CMNodeInfoProviderTest, NotifySubscribersOnceSMInfoReceived)
{
    const auto cNodeInfo = CreateNodeInfo("node1", NodeStateEnum::eProvisioned, false);

    auto err = mNodeInfoProvider.Init(mConfig, mIAMNodeInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mNodeInfoProvider.SubscribeListener(mListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    mIAMNodeInfoProvider.SetNodeInfo(*cNodeInfo);
    mIAMNodeInfoProvider.NotifyNodeInfoChanged(*cNodeInfo);

    const auto onlineNodeInfo = CreateNodeInfo(cNodeInfo->mNodeID, NodeStateEnum::eProvisioned, true);

    mIAMNodeInfoProvider.SetNodeInfo(*onlineNodeInfo);
    mIAMNodeInfoProvider.NotifyNodeInfoChanged(*onlineNodeInfo);

    err = mNodeInfoProvider.OnSMInfoReceived(SMInfo {cNodeInfo->mNodeID, {}, {}});
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto receivedInfo = std::make_unique<UnitNodeInfo>();

    EXPECT_TRUE(mListener.Wait(mConfig.mSMConnectionTimeout / 3, *receivedInfo))
        << "Timeout waiting for node info change";
    EXPECT_STREQ(receivedInfo->mNodeID.CStr(), cNodeInfo->mNodeID.CStr());
    EXPECT_EQ(receivedInfo->mState, NodeStateEnum::eProvisioned);
    EXPECT_TRUE(receivedInfo->mIsConnected);

    err = mNodeInfoProvider.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

} // namespace aos::cm::nodeinfoprovider
