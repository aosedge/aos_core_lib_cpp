/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/iam/nodemanager/nodemanager.hpp>

#include "storagemock.hpp"

using namespace aos;
using namespace aos::iam::nodemanager;
using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class NodeManagerTest : public Test {
protected:
    void SetUp() override
    {
        EXPECT_CALL(mStorage, GetAllNodeIDs(_)).WillOnce(Return(ErrorEnum::eNone));
        ASSERT_TRUE(mManager.Init(mStorage).IsNone());
    }

    StorageMock mStorage;
    NodeManager mManager;
};

template <typename T>
std::vector<T> ConvertToStl(const Array<T>& arr)
{
    return std::vector<T>(arr.begin(), arr.end());
}

NodeInfo CreateNodeInfo(
    const String& id, aos::NodeStateEnum state = NodeStateEnum::eProvisioned, bool isConnected = true)
{
    NodeInfo info;

    info.mNodeID      = id;
    info.mState       = state;
    info.mIsConnected = isConnected;

    return info;
}

/***********************************************************************************************************************
 * NodeInfoListenerMock
 **********************************************************************************************************************/

class NodeInfoListenerMock : public NodeInfoListenerItf {
public:
    MOCK_METHOD(void, OnNodeInfoChange, (const NodeInfo& info));
    MOCK_METHOD(void, OnNodeRemoved, (const String& nodeID));
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(NodeManagerTest, Init)
{
    NodeInfo node0 = CreateNodeInfo("node0", NodeStateEnum::eProvisioned);
    NodeInfo node1 = CreateNodeInfo("node1", NodeStateEnum::ePaused);

    EXPECT_CALL(mStorage, GetAllNodeIDs(_)).WillOnce(Invoke([&](Array<StaticString<cIDLen>>& dst) {
        dst.PushBack(node0.mNodeID);
        dst.PushBack(node1.mNodeID);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorage, GetNodeInfo(node0.mNodeID, _)).WillOnce(Invoke([&](const String& nodeID, NodeInfo& nodeInfo) {
        (void)nodeID;

        nodeInfo = node0;

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorage, GetNodeInfo(node1.mNodeID, _)).WillOnce(Invoke([&](const String& nodeID, NodeInfo& nodeInfo) {
        (void)nodeID;

        nodeInfo = node1;

        return ErrorEnum::eNone;
    }));

    ASSERT_TRUE(mManager.Init(mStorage).IsNone());

    StaticArray<StaticString<cIDLen>, 2> ids;

    ASSERT_TRUE(mManager.GetAllNodeIDs(ids).IsNone());
    EXPECT_THAT(ConvertToStl(ids), ElementsAre(node0.mNodeID, node1.mNodeID));
}

TEST_F(NodeManagerTest, SetNodeInfoUnprovisioned)
{
    NodeInfo info = CreateNodeInfo("node0", NodeStateEnum::eUnprovisioned, false);

    EXPECT_CALL(mStorage, RemoveNodeInfo(info.mNodeID)).WillOnce(Return(ErrorEnum::eNotFound));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());
}

TEST_F(NodeManagerTest, SetNodeInfoOnline)
{
    NodeInfo info = CreateNodeInfo("node0", NodeStateEnum::eProvisioned);

    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());
}

TEST_F(NodeManagerTest, SetNodeInfoOffline)
{
    NodeInfo info = CreateNodeInfo("node0", NodeStateEnum::eProvisioned);

    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());
}

TEST_F(NodeManagerTest, GetNodeInfoNotFound)
{
    StaticString<cIDLen> node0 = "node0";
    NodeInfo             nodeInfo;

    ASSERT_EQ(mManager.GetNodeInfo(node0, nodeInfo), ErrorEnum::eNotFound);
}

TEST_F(NodeManagerTest, GetNodeInfoOk)
{
    NodeInfo info = CreateNodeInfo("node0");

    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());

    NodeInfo result;

    ASSERT_TRUE(mManager.GetNodeInfo(info.mNodeID, result).IsNone());
    EXPECT_EQ(result, info);
}

TEST_F(NodeManagerTest, GetAllNodeIDs)
{
    StaticString<cIDLen> node0 = "node0";
    StaticString<cIDLen> node1 = "node1";

    NodeStateEnum state = NodeStateEnum::eProvisioned;

    EXPECT_CALL(mStorage, SetNodeInfo(_)).WillRepeatedly(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeState(node0, state, true).IsNone());
    ASSERT_TRUE(mManager.SetNodeState(node1, state, true).IsNone());

    StaticArray<StaticString<cIDLen>, 2> ids;

    ASSERT_TRUE(mManager.GetAllNodeIDs(ids).IsNone());
    EXPECT_THAT(ConvertToStl(ids), ElementsAre(node0, node1));
}

TEST_F(NodeManagerTest, RemoveNodeInfo)
{
    StaticString<cIDLen> node0 = "node0";

    NodeInfo      nodeInfo;
    NodeStateEnum state = NodeStateEnum::eProvisioned;

    EXPECT_CALL(mStorage, SetNodeInfo(Field(&NodeInfo::mNodeID, node0))).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeState(node0, state, true).IsNone());

    ASSERT_EQ(mManager.GetNodeInfo(node0, nodeInfo), ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveNodeInfo(node0)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.RemoveNodeInfo(node0).IsNone());
    ASSERT_EQ(mManager.GetNodeInfo(node0, nodeInfo), ErrorEnum::eNotFound);
}

TEST_F(NodeManagerTest, NotifyNodeInfoChangeOnSetNodeInfo)
{
    NodeInfo info = CreateNodeInfo("node0");

    NodeInfoListenerMock listener;

    ASSERT_TRUE(mManager.SubscribeNodeInfoChange(listener).IsNone());

    EXPECT_CALL(listener, OnNodeInfoChange(info)).Times(1);
    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());
}

TEST_F(NodeManagerTest, NotifyNodeRemoved)
{
    NodeInfo info = CreateNodeInfo("node0");

    NodeInfoListenerMock listener;

    ASSERT_TRUE(mManager.SubscribeNodeInfoChange(listener).IsNone());

    EXPECT_CALL(listener, OnNodeInfoChange(info)).Times(1);
    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());

    EXPECT_CALL(listener, OnNodeRemoved(info.mNodeID)).Times(1);
    EXPECT_CALL(mStorage, RemoveNodeInfo(info.mNodeID)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.RemoveNodeInfo(info.mNodeID).IsNone());
}
