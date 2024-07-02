/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/iam/nodemanager.hpp"
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
        EXPECT_CALL(mStorage, GetAllNodeIds(_)).WillOnce(Return(ErrorEnum::eNone));
        ASSERT_TRUE(mManager.Init(mStorage).IsNone());
    }

    NodeInfoStorageMock mStorage;
    NodeManager         mManager;
};

template <typename T>
std::vector<T> ConvertToStl(const Array<T>& arr)
{
    return std::vector<T>(arr.begin(), arr.end());
}

NodeInfo CreateNodeInfo(const String& id, aos::NodeStatusEnum status)
{
    NodeInfo info;

    info.mNodeID = id;
    info.mStatus = status;

    return info;
}

/***********************************************************************************************************************
 * NodeInfoListenerMock
 **********************************************************************************************************************/

class NodeInfoListenerMock : public NodeInfoListenerItf {
public:
    MOCK_METHOD(void, OnNodeInfoChange, (const NodeInfo& info));
    MOCK_METHOD(void, OnNodeRemoved, (const String& nodeId));
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(NodeManagerTest, Init)
{
    NodeInfo node0 = CreateNodeInfo("node0", NodeStatusEnum::eProvisioned);
    NodeInfo node1 = CreateNodeInfo("node1", NodeStatusEnum::ePaused);

    EXPECT_CALL(mStorage, GetAllNodeIds(_)).WillOnce(Invoke([&](Array<StaticString<cNodeIDLen>>& dst) {
        dst.PushBack(node0.mNodeID);
        dst.PushBack(node1.mNodeID);

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorage, GetNodeInfo(node0.mNodeID, _)).WillOnce(Invoke([&](const String& nodeId, NodeInfo& nodeInfo) {
        (void)nodeId;
        nodeInfo = node0;

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorage, GetNodeInfo(node1.mNodeID, _)).WillOnce(Invoke([&](const String& nodeId, NodeInfo& nodeInfo) {
        (void)nodeId;
        nodeInfo = node1;

        return ErrorEnum::eNone;
    }));

    ASSERT_TRUE(mManager.Init(mStorage).IsNone());

    StaticArray<StaticString<cNodeIDLen>, 2> ids;

    ASSERT_TRUE(mManager.GetAllNodeIds(ids).IsNone());
    EXPECT_THAT(ConvertToStl(ids), ElementsAre(node0.mNodeID, node1.mNodeID));
}

TEST_F(NodeManagerTest, SetNodeInfoUnprovisioned)
{
    NodeInfo info = CreateNodeInfo("node0", NodeStatusEnum::eUnprovisioned);

    EXPECT_CALL(mStorage, RemoveNodeInfo(info.mNodeID)).WillOnce(Return(ErrorEnum::eNotFound));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());
}

TEST_F(NodeManagerTest, SetNodeInfoProvisioned)
{
    NodeInfo info = CreateNodeInfo("node0", NodeStatusEnum::eProvisioned);

    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());
}

TEST_F(NodeManagerTest, SetNodeStatusUnprovisioned)
{
    StaticString<cNodeIDLen> node0  = "node0";
    NodeStatus               status = NodeStatusEnum::eUnprovisioned;

    EXPECT_CALL(mStorage, RemoveNodeInfo(node0)).WillOnce(Return(ErrorEnum::eNotFound));
    ASSERT_TRUE(mManager.SetNodeStatus(node0, status).IsNone());
}

TEST_F(NodeManagerTest, SetNodeStatusProvisioned)
{
    NodeInfo node0 = CreateNodeInfo("node0", NodeStatusEnum::eProvisioned);

    EXPECT_CALL(mStorage, SetNodeInfo(node0)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeStatus(node0.mNodeID, node0.mStatus).IsNone());
}

TEST_F(NodeManagerTest, GetNodeInfoNotFound)
{
    StaticString<cNodeIDLen> node0 = "node0";
    NodeInfo                 nodeInfo;

    ASSERT_EQ(mManager.GetNodeInfo(node0, nodeInfo), ErrorEnum::eNotFound);
}

TEST_F(NodeManagerTest, GetNodeInfoOk)
{
    NodeInfo info = CreateNodeInfo("node0", NodeStatusEnum::eProvisioned);

    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());

    NodeInfo result;

    ASSERT_TRUE(mManager.GetNodeInfo(info.mNodeID, result).IsNone());
    EXPECT_EQ(result, info);
}

TEST_F(NodeManagerTest, GetAllNodeIds)
{
    StaticString<cNodeIDLen> node0 = "node0";
    StaticString<cNodeIDLen> node1 = "node1";

    NodeStatus status = NodeStatusEnum::eProvisioned;

    EXPECT_CALL(mStorage, SetNodeInfo(_)).WillRepeatedly(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeStatus(node0, status).IsNone());
    ASSERT_TRUE(mManager.SetNodeStatus(node1, status).IsNone());

    StaticArray<StaticString<cNodeIDLen>, 2> ids;

    ASSERT_TRUE(mManager.GetAllNodeIds(ids).IsNone());
    EXPECT_THAT(ConvertToStl(ids), ElementsAre(node0, node1));
}

TEST_F(NodeManagerTest, RemoveNodeInfo)
{
    StaticString<cNodeIDLen> node0 = "node0";

    NodeInfo   nodeInfo;
    NodeStatus status = NodeStatusEnum::eProvisioned;

    EXPECT_CALL(mStorage, SetNodeInfo(Field(&NodeInfo::mNodeID, node0))).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeStatus(node0, status).IsNone());

    ASSERT_EQ(mManager.GetNodeInfo(node0, nodeInfo), ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveNodeInfo(node0)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.RemoveNodeInfo(node0).IsNone());
    ASSERT_EQ(mManager.GetNodeInfo(node0, nodeInfo), ErrorEnum::eNotFound);
}

TEST_F(NodeManagerTest, NotifyNodeInfoChangeOnSetNodeInfo)
{
    NodeInfo info = CreateNodeInfo("node0", NodeStatusEnum::eProvisioned);

    NodeInfoListenerMock listener;

    ASSERT_TRUE(mManager.SubscribeNodeInfoChange(listener).IsNone());

    EXPECT_CALL(listener, OnNodeInfoChange(info)).Times(1);
    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());
}

TEST_F(NodeManagerTest, NotifyNodeRemoved)
{
    NodeInfo info = CreateNodeInfo("node0", NodeStatusEnum::eProvisioned);

    NodeInfoListenerMock listener;

    ASSERT_TRUE(mManager.SubscribeNodeInfoChange(listener).IsNone());

    EXPECT_CALL(listener, OnNodeInfoChange(info)).Times(1);
    EXPECT_CALL(mStorage, SetNodeInfo(info)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.SetNodeInfo(info).IsNone());

    EXPECT_CALL(listener, OnNodeRemoved(info.mNodeID)).Times(1);
    EXPECT_CALL(mStorage, RemoveNodeInfo(info.mNodeID)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_TRUE(mManager.RemoveNodeInfo(info.mNodeID).IsNone());
}
