/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/cm/tests/mocks/cmnodeinfoprovidermock.hpp>
#include <core/cm/tests/mocks/jsonprovidermock.hpp>
#include <core/cm/tests/mocks/nodeconfigchangelistenermock.hpp>
#include <core/cm/tests/mocks/nodeconfigcontrollermock.hpp>
#include <core/cm/unitconfig/unitconfig.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>

using namespace testing;
using namespace aos::cm::unitconfig;
using namespace aos::cm::nodeinfoprovider;

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestConfigFile = "/tmp/test_unit_config.json";
constexpr auto cTestNodeID     = "node0";
constexpr auto cTestNodeType   = "type1";

constexpr auto cValidTestUnitConfig = R"({
     "formatVersion": "1",
     "version": "1.0.0",
     "nodes": [
         {
             "nodeType": "type1"
         }
     ]
 })";

constexpr auto cNode0TestUnitConfig = R"({
     "formatVersion": "1",
     "version": "1.0.0",
     "nodes": [
         {
             "nodeId": "node0",
             "nodeType": "type1"
         }
     ]
 })";

constexpr auto cInvalidTestUnitConfig = R"({
     "formatVersion": 1,
     "vendorVersion": "1.0.0",
     something not valid
 })";

} // namespace

/***********************************************************************************************************************
 * Test fixture
 **********************************************************************************************************************/

class UnitConfigTest : public Test {
protected:
    void SetUp() override
    {
        aos::tests::utils::InitLog();
        aos::fs::Remove(cTestConfigFile);
    }

    void TearDown() override { aos::fs::Remove(cTestConfigFile); }

    std::shared_ptr<aos::cloudprotocol::UnitConfig> CreateTestUnitConfig(const aos::String& version)
    {
        auto config      = std::make_shared<aos::cloudprotocol::UnitConfig>();
        config->mVersion = version;

        aos::cloudprotocol::NodeConfig nodeConfig;
        nodeConfig.mNodeType = cTestNodeType;
        config->mNodes.PushBack(nodeConfig);

        return config;
    }

    std::shared_ptr<aos::cloudprotocol::UnitConfig> CreateTestUnitConfigWithNodeID(
        const aos::String& version = "1.0.0", const aos::String& nodeID = "node0")
    {
        auto config      = std::make_shared<aos::cloudprotocol::UnitConfig>();
        config->mVersion = version;

        aos::cloudprotocol::NodeConfig nodeConfig;
        nodeConfig.mNodeID   = nodeID;
        nodeConfig.mNodeType = cTestNodeType;
        config->mNodes.PushBack(nodeConfig);

        return config;
    }

    aos::NodeInfo CreateTestNodeInfo()
    {
        aos::NodeInfo nodeInfo;
        nodeInfo.mNodeID   = cTestNodeID;
        nodeInfo.mNodeType = cTestNodeType;
        return nodeInfo;
    }

    void SetupValidUnitConfig()
    {
        aos::NodeInfo nodeInfo = CreateTestNodeInfo();

        EXPECT_CALL(mNodeInfoProvider, GetCurrentNodeID()).WillOnce(Return(aos::String(cTestNodeID)));
        EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(aos::String(cTestNodeID), _))
            .WillOnce(DoAll(SetArgReferee<1>(nodeInfo), Return(aos::ErrorEnum::eNone)));

        auto config = CreateTestUnitConfig("1.0.0");

        EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(*config), Return(aos::ErrorEnum::eNone)));
    }

    void CreateTestConfigFile(const aos::String& jsonConfig = cValidTestUnitConfig)
    {
        auto err = aos::fs::WriteStringToFile(cTestConfigFile, jsonConfig, 0600);
        ASSERT_TRUE(err.IsNone()) << "Failed to create test config file";
    }

    StrictMock<NodeInfoProviderMock>     mNodeInfoProvider;
    StrictMock<NodeConfigControllerMock> mNodeConfigController;
    StrictMock<JSONProviderMock>         mJSONProvider;
    UnitConfig                           mUnitConfig;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(UnitConfigTest, ValidGetStatus)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    auto err = mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider);

    EXPECT_TRUE(err.IsNone());

    aos::cloudprotocol::UnitConfigStatus status;
    err = mUnitConfig.GetStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mVersion, "1.0.0");
    EXPECT_EQ(status.mStatus, aos::ItemStatusEnum::eInstalled);
    EXPECT_TRUE(status.mError.IsNone());

    aos::cloudprotocol::NodeConfig nodeConfig;
    err = mUnitConfig.GetNodeConfig("id1", "type1", nodeConfig);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(nodeConfig.mNodeType, "type1");
}

TEST_F(UnitConfigTest, InvalidGetStatus)
{
    CreateTestConfigFile(cInvalidTestUnitConfig);

    aos::NodeInfo nodeInfo = CreateTestNodeInfo();

    EXPECT_CALL(mNodeInfoProvider, GetCurrentNodeID()).WillOnce(Return(cTestNodeID));

    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(aos::String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(nodeInfo), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _)).WillOnce(Return(aos::ErrorEnum::eInvalidArgument));

    auto err = mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider);

    EXPECT_FALSE(err.IsNone());

    aos::cloudprotocol::UnitConfigStatus status;
    err = mUnitConfig.GetStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mStatus, aos::ItemStatusEnum::eError);
}

TEST_F(UnitConfigTest, CheckUnitConfig)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider).IsNone());

    auto validUnitConfig = CreateTestUnitConfig("2.0.0");

    aos::StaticArray<NodeConfigStatus, aos::cMaxNumNodes> nodeConfigStatuses;
    NodeConfigStatus                                      status1, status2, status3;
    status1.mNodeID   = "id1";
    status1.mNodeType = "type1";
    status1.mVersion  = "1.0.0";
    status2.mNodeID   = "id2";
    status2.mNodeType = "type1";
    status2.mVersion  = "1.0.0";
    status3.mNodeID   = "id3";
    status3.mNodeType = "type1";
    status3.mVersion  = "1.0.0";

    nodeConfigStatuses.PushBack(status1);
    nodeConfigStatuses.PushBack(status2);
    nodeConfigStatuses.PushBack(status3);

    EXPECT_CALL(mNodeConfigController, GetNodeConfigStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeConfigStatuses), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mNodeConfigController, CheckNodeConfig(_, _, _)).Times(3).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    auto err = mUnitConfig.CheckUnitConfig(*validUnitConfig);
    EXPECT_TRUE(err.IsNone());

    auto invalidUnitConfig = CreateTestUnitConfig("1.0.0");
    err                    = mUnitConfig.CheckUnitConfig(*invalidUnitConfig);

    EXPECT_EQ(err, aos::ErrorEnum::eAlreadyExist);
}

TEST_F(UnitConfigTest, UpdateUnitConfigWithListeners)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider).IsNone());

    StrictMock<NodeConfigChangeListenerMock> listener1;
    StrictMock<NodeConfigChangeListenerMock> listener2;

    EXPECT_CALL(listener1, OnNodeConfigChange(_)).Times(1);
    EXPECT_CALL(listener2, OnNodeConfigChange(_)).Times(1);

    ASSERT_TRUE(mUnitConfig.SubscribeCurrentNodeConfigChange(&listener1).IsNone());
    ASSERT_TRUE(mUnitConfig.SubscribeCurrentNodeConfigChange(&listener2).IsNone());

    auto newUnitConfig = CreateTestUnitConfig("2.0.0");

    aos::StaticArray<NodeConfigStatus, aos::cMaxNumNodes> nodeConfigStatuses;
    NodeConfigStatus                                      status1, status2, status3;
    status1.mNodeID   = "id1";
    status1.mNodeType = "type1";
    status1.mVersion  = "1.0.0";
    status2.mNodeID   = "id2";
    status2.mNodeType = "type1";
    status2.mVersion  = "1.0.0";
    status3.mNodeID   = cTestNodeID; // This is our current node ID
    status3.mNodeType = "type1";
    status3.mVersion  = "1.0.0";

    nodeConfigStatuses.PushBack(status1);
    nodeConfigStatuses.PushBack(status2);
    nodeConfigStatuses.PushBack(status3);

    EXPECT_CALL(mNodeConfigController, GetNodeConfigStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeConfigStatuses), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mNodeConfigController, SetNodeConfig(_, _, _)).Times(3).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mJSONProvider, UnitConfigToJSON(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    auto err = mUnitConfig.UpdateUnitConfig(*newUnitConfig);
    EXPECT_TRUE(err.IsNone());

    aos::cloudprotocol::UnitConfigStatus status;
    err = mUnitConfig.GetStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mVersion, "2.0.0");
}

TEST_F(UnitConfigTest, UpdateUnitConfigWithoutListeners)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider).IsNone());

    StrictMock<NodeConfigChangeListenerMock> listener1;
    StrictMock<NodeConfigChangeListenerMock> listener2;

    EXPECT_CALL(listener1, OnNodeConfigChange(_)).Times(0);
    EXPECT_CALL(listener2, OnNodeConfigChange(_)).Times(0);

    ASSERT_TRUE(mUnitConfig.SubscribeCurrentNodeConfigChange(&listener1).IsNone());
    ASSERT_TRUE(mUnitConfig.SubscribeCurrentNodeConfigChange(&listener2).IsNone());

    auto newUnitConfig = CreateTestUnitConfig("2.0.0");

    aos::StaticArray<NodeConfigStatus, aos::cMaxNumNodes> nodeConfigStatuses;
    NodeConfigStatus                                      status1, status2;
    status1.mNodeID   = "id1";
    status1.mNodeType = "type1";
    status1.mVersion  = "1.0.0";
    status2.mNodeID   = "id2";
    status2.mNodeType = "type1";
    status2.mVersion  = "1.0.0";

    nodeConfigStatuses.PushBack(status1);
    nodeConfigStatuses.PushBack(status2);

    EXPECT_CALL(mNodeConfigController, GetNodeConfigStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeConfigStatuses), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mNodeConfigController, SetNodeConfig(_, _, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mJSONProvider, UnitConfigToJSON(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    auto err = mUnitConfig.UpdateUnitConfig(*newUnitConfig);
    EXPECT_TRUE(err.IsNone());

    aos::cloudprotocol::UnitConfigStatus status;
    err = mUnitConfig.GetStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mVersion, "2.0.0");
}

TEST_F(UnitConfigTest, NodeConfigStatus)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider).IsNone());

    EXPECT_CALL(mNodeConfigController, SetNodeConfig(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    NodeConfigStatus status;
    status.mNodeID   = "id1";
    status.mNodeType = "type1";
    status.mVersion  = "2.0.0";

    mUnitConfig.OnNodeConfigStatus(status);
}

TEST_F(UnitConfigTest, CurrentNodeConfigUpdate)
{
    CreateTestConfigFile(cNode0TestUnitConfig);

    aos::NodeInfo nodeInfo = CreateTestNodeInfo();

    EXPECT_CALL(mNodeInfoProvider, GetCurrentNodeID()).WillOnce(Return(cTestNodeID));

    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(aos::String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(nodeInfo), Return(aos::ErrorEnum::eNone)));

    auto unitConfig = CreateTestUnitConfigWithNodeID();

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(*unitConfig), Return(aos::ErrorEnum::eNone)));

    ASSERT_TRUE(mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider).IsNone());

    StrictMock<NodeConfigChangeListenerMock> curNodeConfigListener;
    EXPECT_CALL(curNodeConfigListener, OnNodeConfigChange(_)).Times(1);

    ASSERT_TRUE(mUnitConfig.SubscribeCurrentNodeConfigChange(&curNodeConfigListener).IsNone());

    EXPECT_CALL(mNodeConfigController, SetNodeConfig(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    NodeConfigStatus status;
    status.mNodeID   = "node0";
    status.mNodeType = "type1";
    status.mVersion  = "2.0.0";

    mUnitConfig.OnNodeConfigStatus(status);
}

TEST_F(UnitConfigTest, VersionComparison)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider).IsNone());

    auto sameVersion = CreateTestUnitConfig("1.0.0");
    auto err         = mUnitConfig.CheckUnitConfig(*sameVersion);
    EXPECT_EQ(err, aos::ErrorEnum::eAlreadyExist);

    auto lowerVersion = CreateTestUnitConfig("0.9.0");
    err               = mUnitConfig.CheckUnitConfig(*lowerVersion);
    EXPECT_EQ(err, aos::ErrorEnum::eWrongState);

    auto prereleaseVersion = CreateTestUnitConfig("1.0.0-alpha");
    err                    = mUnitConfig.CheckUnitConfig(*prereleaseVersion);
    EXPECT_EQ(err, aos::ErrorEnum::eWrongState);

    auto higherPrereleaseVersion = CreateTestUnitConfig("1.0.0-beta");
    err                          = mUnitConfig.CheckUnitConfig(*higherPrereleaseVersion);
    EXPECT_EQ(err, aos::ErrorEnum::eWrongState);
}

TEST_F(UnitConfigTest, MultipleListeners)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider).IsNone());

    StrictMock<NodeConfigChangeListenerMock> listener1;
    StrictMock<NodeConfigChangeListenerMock> listener2;

    EXPECT_CALL(listener1, OnNodeConfigChange(_)).Times(1);
    EXPECT_CALL(listener2, OnNodeConfigChange(_)).Times(1);

    ASSERT_TRUE(mUnitConfig.SubscribeCurrentNodeConfigChange(&listener1).IsNone());
    ASSERT_TRUE(mUnitConfig.SubscribeCurrentNodeConfigChange(&listener2).IsNone());

    EXPECT_CALL(mNodeConfigController, SetNodeConfig(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    NodeConfigStatus status;
    status.mNodeID   = cTestNodeID;
    status.mNodeType = cTestNodeType;
    status.mVersion  = "2.0.0";

    mUnitConfig.OnNodeConfigStatus(status);
}

TEST_F(UnitConfigTest, UnsubscribeListener)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init(cTestConfigFile, mNodeInfoProvider, mNodeConfigController, mJSONProvider).IsNone());

    StrictMock<NodeConfigChangeListenerMock> listener;
    EXPECT_CALL(listener, OnNodeConfigChange(_)).Times(1);

    ASSERT_TRUE(mUnitConfig.SubscribeCurrentNodeConfigChange(&listener).IsNone());

    EXPECT_CALL(mNodeConfigController, SetNodeConfig(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    NodeConfigStatus status;
    status.mNodeID   = cTestNodeID;
    status.mNodeType = cTestNodeType;
    status.mVersion  = "2.0.0";

    mUnitConfig.OnNodeConfigStatus(status);

    mUnitConfig.UnsubscribeCurrentNodeConfigChange(&listener);

    EXPECT_CALL(mNodeConfigController, SetNodeConfig(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    mUnitConfig.OnNodeConfigStatus(status);
}
