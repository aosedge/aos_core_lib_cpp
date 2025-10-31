/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/cm/tests/mocks/nodeconfighandlermock.hpp>
#include <core/cm/tests/mocks/nodeinfoprovidermock.hpp>
#include <core/cm/unitconfig/tests/mocks/jsonprovidermock.hpp>
#include <core/cm/unitconfig/unitconfig.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>

using namespace testing;

namespace aos::cm::unitconfig {

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
        tests::utils::InitLog();
        fs::Remove(cTestConfigFile);
    }

    void TearDown() override { fs::Remove(cTestConfigFile); }

    aos::UnitConfig CreateTestUnitConfig(const String& version)
    {
        aos::UnitConfig config;
        config.mVersion = version;

        NodeConfig nodeConfig;
        nodeConfig.mNodeType = cTestNodeType;
        config.mNodes.PushBack(nodeConfig);

        return config;
    }

    aos::UnitConfig CreateTestUnitConfigWithNodeID(const String& version = "1.0.0", const String& nodeID = "node0")
    {
        aos::UnitConfig config;
        config.mVersion = version;

        NodeConfig nodeConfig;
        nodeConfig.mNodeID   = nodeID;
        nodeConfig.mNodeType = cTestNodeType;
        config.mNodes.PushBack(nodeConfig);

        return config;
    }

    UnitNodeInfo CreateTestNodeInfo()
    {
        UnitNodeInfo nodeInfo;
        nodeInfo.mNodeID   = cTestNodeID;
        nodeInfo.mNodeType = cTestNodeType;
        return nodeInfo;
    }

    void SetupValidUnitConfig()
    {
        auto config = CreateTestUnitConfig("1.0.0");

        EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(config), Return(ErrorEnum::eNone)));
    }

    void CreateTestConfigFile(const String& jsonConfig = cValidTestUnitConfig)
    {
        auto err = fs::WriteStringToFile(cTestConfigFile, jsonConfig, 0600);
        ASSERT_TRUE(err.IsNone()) << "Failed to create test config file";
    }

    StrictMock<cm::nodeinfoprovider::NodeInfoProviderMock> mNodeInfoProvider;
    StrictMock<NodeConfigHandlerMock>                      mNodeConfigHandler;
    StrictMock<JSONProviderMock>                           mJSONProvider;
    UnitConfig                                             mUnitConfig;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(UnitConfigTest, InitWithValidConfig)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    auto err = mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);

    EXPECT_TRUE(err.IsNone());
}

TEST_F(UnitConfigTest, InitWithInvalidConfig)
{
    CreateTestConfigFile(cInvalidTestUnitConfig);

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    auto err = mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);

    EXPECT_TRUE(err.IsNone());
}

TEST_F(UnitConfigTest, InitWithMissingConfigFile)
{
    auto err = mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);

    EXPECT_TRUE(err.IsNone());

    UnitConfigStatus status;
    err = mUnitConfig.GetUnitConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_TRUE(status.mVersion.IsEmpty());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eAbsent);
}

TEST_F(UnitConfigTest, GetUnitConfigStatusValid)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    UnitConfigStatus status;
    auto             err = mUnitConfig.GetUnitConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mVersion, "1.0.0");
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eInstalled);
    EXPECT_TRUE(status.mError.IsNone());
}

TEST_F(UnitConfigTest, GetUnitConfigStatusWithError)
{
    CreateTestConfigFile(cInvalidTestUnitConfig);

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    auto err = mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);

    EXPECT_TRUE(err.IsNone());

    UnitConfigStatus status;
    err = mUnitConfig.GetUnitConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eFailed);
    EXPECT_FALSE(status.mError.IsNone());
}

TEST_F(UnitConfigTest, GetNodeConfigByType)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    NodeConfig nodeConfig;
    auto       err = mUnitConfig.GetNodeConfig("", cTestNodeType, nodeConfig);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(nodeConfig.mNodeType, cTestNodeType);
    EXPECT_EQ(nodeConfig.mVersion, "1.0.0");
    EXPECT_TRUE(nodeConfig.mNodeID.IsEmpty());
}

TEST_F(UnitConfigTest, GetNodeConfigByID)
{
    CreateTestConfigFile(cNode0TestUnitConfig);

    auto config = CreateTestUnitConfigWithNodeID();

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(config), Return(ErrorEnum::eNone)));

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    NodeConfig nodeConfig;
    auto       err = mUnitConfig.GetNodeConfig(cTestNodeID, "", nodeConfig);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(nodeConfig.mNodeID, cTestNodeID);
    EXPECT_EQ(nodeConfig.mNodeType, cTestNodeType);
    EXPECT_EQ(nodeConfig.mVersion, "1.0.0");
}

TEST_F(UnitConfigTest, GetNodeConfigNotFound)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    NodeConfig nodeConfig;
    auto       err = mUnitConfig.GetNodeConfig("nonexistent", "unknown", nodeConfig);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(nodeConfig.mNodeID, "nonexistent");
    EXPECT_EQ(nodeConfig.mVersion, "1.0.0");
    EXPECT_TRUE(nodeConfig.mNodeType.IsEmpty());
}

TEST_F(UnitConfigTest, CheckUnitConfigValidVersion)
{
    CreateTestConfigFile(cNode0TestUnitConfig);

    auto config = CreateTestUnitConfigWithNodeID("1.0.0", cTestNodeID);

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(config), Return(ErrorEnum::eNone)));

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto newUnitConfig = CreateTestUnitConfigWithNodeID("2.0.0", cTestNodeID);

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIds;
    nodeIds.PushBack(cTestNodeID);

    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeIds), Return(ErrorEnum::eNone)));

    NodeConfigStatus nodeConfigStatus;
    nodeConfigStatus.mVersion = "1.0.0";

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(nodeConfigStatus), Return(ErrorEnum::eNone)));

    EXPECT_CALL(mNodeConfigHandler, CheckNodeConfig(String(cTestNodeID), _)).WillOnce(Return(ErrorEnum::eNone));

    auto err = mUnitConfig.CheckUnitConfig(newUnitConfig);
    EXPECT_TRUE(err.IsNone());
}

TEST_F(UnitConfigTest, CheckUnitConfigSameVersion)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto sameVersionConfig = CreateTestUnitConfig("1.0.0");

    auto err = mUnitConfig.CheckUnitConfig(sameVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eAlreadyExist);
}

TEST_F(UnitConfigTest, CheckUnitConfigLowerVersion)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto lowerVersionConfig = CreateTestUnitConfig("0.9.0");

    auto err = mUnitConfig.CheckUnitConfig(lowerVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eWrongState);
}

TEST_F(UnitConfigTest, UpdateUnitConfigSuccess)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto newUnitConfig = CreateTestUnitConfig("2.0.0");

    EXPECT_CALL(mJSONProvider, UnitConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIds;
    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeIds), Return(ErrorEnum::eNone)));

    auto err = mUnitConfig.UpdateUnitConfig(newUnitConfig);
    EXPECT_TRUE(err.IsNone());

    UnitConfigStatus status;
    err = mUnitConfig.GetUnitConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mVersion, "2.0.0");
}

TEST_F(UnitConfigTest, UpdateUnitConfigSameVersion)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto sameVersionConfig = CreateTestUnitConfig("1.0.0");

    auto err = mUnitConfig.UpdateUnitConfig(sameVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eAlreadyExist);
}

TEST_F(UnitConfigTest, OnNodeInfoChangedUpdatesConfig)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    UnitNodeInfo nodeInfo = CreateTestNodeInfo();

    NodeConfigStatus nodeConfigStatus;
    nodeConfigStatus.mVersion = "0.9.0";

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(nodeConfigStatus), Return(ErrorEnum::eNone)));

    EXPECT_CALL(mNodeConfigHandler, UpdateNodeConfig(String(cTestNodeID), _)).WillOnce(Return(ErrorEnum::eNone));

    mUnitConfig.OnNodeInfoChanged(nodeInfo);
}

TEST_F(UnitConfigTest, OnNodeInfoChangedSkipsIfVersionMatches)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    UnitNodeInfo nodeInfo = CreateTestNodeInfo();

    NodeConfigStatus nodeConfigStatus;
    nodeConfigStatus.mVersion = "1.0.0";

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(nodeConfigStatus), Return(ErrorEnum::eNone)));

    mUnitConfig.OnNodeInfoChanged(nodeInfo);
}

TEST_F(UnitConfigTest, OnNodeInfoChangedWithUnitConfigError)
{
    CreateTestConfigFile(cInvalidTestUnitConfig);

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    UnitNodeInfo nodeInfo = CreateTestNodeInfo();

    mUnitConfig.OnNodeInfoChanged(nodeInfo);
}

TEST_F(UnitConfigTest, VersionComparisonPrerelease)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto prereleaseVersion = CreateTestUnitConfig("1.0.0-alpha");
    auto err               = mUnitConfig.CheckUnitConfig(prereleaseVersion);
    EXPECT_EQ(err, ErrorEnum::eWrongState);

    auto higherPrereleaseVersion = CreateTestUnitConfig("1.0.0-beta");
    err                          = mUnitConfig.CheckUnitConfig(higherPrereleaseVersion);
    EXPECT_EQ(err, ErrorEnum::eWrongState);
}

TEST_F(UnitConfigTest, CheckUnitConfigMultipleNodes)
{
    CreateTestConfigFile(cNode0TestUnitConfig);

    aos::UnitConfig config;
    config.mVersion = "1.0.0";

    NodeConfig node1, node2, node3;
    node1.mNodeID   = "node1";
    node1.mNodeType = cTestNodeType;
    node2.mNodeID   = "node2";
    node2.mNodeType = cTestNodeType;
    node3.mNodeID   = "node3";
    node3.mNodeType = cTestNodeType;

    config.mNodes.PushBack(node1);
    config.mNodes.PushBack(node2);
    config.mNodes.PushBack(node3);

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(config), Return(ErrorEnum::eNone)));

    ASSERT_TRUE(mUnitConfig.Init({cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    aos::UnitConfig newUnitConfig;
    newUnitConfig.mVersion = "2.0.0";
    newUnitConfig.mNodes.PushBack(node1);
    newUnitConfig.mNodes.PushBack(node2);
    newUnitConfig.mNodes.PushBack(node3);

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIds;
    nodeIds.PushBack("node1");
    nodeIds.PushBack("node2");
    nodeIds.PushBack("node3");

    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeIds), Return(ErrorEnum::eNone)));

    NodeConfigStatus status1, status2, status3;
    status1.mVersion = "1.0.0";
    status2.mVersion = "1.0.0";
    status3.mVersion = "1.0.0";

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(status1), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(status2), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(status3), Return(ErrorEnum::eNone)));

    EXPECT_CALL(mNodeConfigHandler, CheckNodeConfig(_, _)).Times(3).WillRepeatedly(Return(ErrorEnum::eNone));

    auto err = mUnitConfig.CheckUnitConfig(newUnitConfig);
    EXPECT_TRUE(err.IsNone());
}

} // namespace aos::cm::unitconfig
