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
#include <core/common/tests/stubs/failingallocator.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>

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

    aos::UnitConfig CreateTestUnitConfigWithNodeID(const String& version = "1.0.0", const String& nodeID = cTestNodeID)
    {
        aos::UnitConfig config;
        config.mVersion = version;

        NodeConfig nodeConfig;
        nodeConfig.mNodeID   = nodeID;
        nodeConfig.mNodeType = cTestNodeType;
        config.mNodes.PushBack(nodeConfig);

        return config;
    }

    UnitNodeInfo CreateTestNodeInfo(const String& nodeID = cTestNodeID)
    {
        UnitNodeInfo nodeInfo;
        nodeInfo.mNodeID      = nodeID;
        nodeInfo.mNodeType    = cTestNodeType;
        nodeInfo.mIsConnected = true;
        return nodeInfo;
    }

    void SetupValidUnitConfig()
    {
        auto config = CreateTestUnitConfig("1.0.0");

        EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(config), Return(ErrorEnum::eNone)));
    }

    void InitInstalled(const String& version = "1.0.0")
    {
        CreateTestConfigFile(cValidTestUnitConfig);

        auto config = CreateTestUnitConfig(version);

        EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(config), Return(ErrorEnum::eNone)));

        ASSERT_TRUE(
            mUnitConfig.Init(mTestAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider)
                .IsNone());
    }

    void InitFailed()
    {
        CreateTestConfigFile(cInvalidTestUnitConfig);

        EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

        ASSERT_TRUE(
            mUnitConfig.Init(mTestAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider)
                .IsNone());
    }

    void ExpectNodeIDs(const std::vector<std::string>& ids, Error err = ErrorEnum::eNone)
    {
        StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIds;

        for (const auto& id : ids) {
            nodeIds.PushBack(id.c_str());
        }

        EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_)).WillOnce(DoAll(SetArgReferee<0>(nodeIds), Return(err)));
    }

    void CreateTestConfigFile(const String& jsonConfig = cValidTestUnitConfig)
    {
        auto err = fs::WriteStringToFile(cTestConfigFile, jsonConfig, 0600);
        ASSERT_TRUE(err.IsNone()) << "Failed to create test config file";
    }

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator           mAllocator;
    tests::FailingAllocator mTestAllocator;

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

    auto err = mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);

    EXPECT_TRUE(err.IsNone());
}

TEST_F(UnitConfigTest, InitWithInvalidConfig)
{
    CreateTestConfigFile(cInvalidTestUnitConfig);

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    auto err = mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);

    EXPECT_TRUE(err.IsNone());
}

TEST_F(UnitConfigTest, InitWithMissingConfigFile)
{
    auto err = mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);

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

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

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

    auto err = mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);

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

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

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

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

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

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

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

    auto config = CreateTestUnitConfigWithNodeID("1.0.0");

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(config), Return(ErrorEnum::eNone)));

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto newUnitConfig = CreateTestUnitConfigWithNodeID("2.0.0");

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIds;
    nodeIds.PushBack(cTestNodeID);

    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeIds), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo()), Return(ErrorEnum::eNone)));

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

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto sameVersionConfig = CreateTestUnitConfig("1.0.0");

    auto err = mUnitConfig.CheckUnitConfig(sameVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eAlreadyExist);
}

TEST_F(UnitConfigTest, CheckUnitConfigLowerVersion)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto lowerVersionConfig = CreateTestUnitConfig("0.9.0");

    auto err = mUnitConfig.CheckUnitConfig(lowerVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eWrongState);
}

TEST_F(UnitConfigTest, UpdateUnitConfigSuccess)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

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

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto sameVersionConfig = CreateTestUnitConfig("1.0.0");

    auto err = mUnitConfig.UpdateUnitConfig(sameVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eAlreadyExist);
}

TEST_F(UnitConfigTest, OnNodeInfoChangedUpdatesConfig)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

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

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    UnitNodeInfo nodeInfo = CreateTestNodeInfo();

    NodeConfigStatus nodeConfigStatus;
    nodeConfigStatus.mVersion = "1.0.0";

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(nodeConfigStatus), Return(ErrorEnum::eNone)));

    mUnitConfig.OnNodeInfoChanged(nodeInfo);
}

TEST_F(UnitConfigTest, OnNodeInfoChangedSkipsWhenUnitConfigAbsent)
{
    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    UnitConfigStatus status;
    ASSERT_TRUE(mUnitConfig.GetUnitConfigStatus(status).IsNone());
    ASSERT_EQ(status.mState, UnitConfigStateEnum::eAbsent);

    UnitNodeInfo nodeInfo = CreateTestNodeInfo();

    // Strict mocks: neither GetNodeConfigStatus nor UpdateNodeConfig must be called when unit config is absent.
    mUnitConfig.OnNodeInfoChanged(nodeInfo);
}

TEST_F(UnitConfigTest, OnNodeInfoChangedWithUnitConfigError)
{
    CreateTestConfigFile(cInvalidTestUnitConfig);

    EXPECT_CALL(mJSONProvider, UnitConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    UnitNodeInfo nodeInfo = CreateTestNodeInfo();

    mUnitConfig.OnNodeInfoChanged(nodeInfo);
}

TEST_F(UnitConfigTest, VersionComparisonPrerelease)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto prereleaseVersion = CreateTestUnitConfig("1.0.0-alpha");
    auto err               = mUnitConfig.CheckUnitConfig(prereleaseVersion);
    EXPECT_EQ(err, ErrorEnum::eWrongState);

    auto higherPrereleaseVersion = CreateTestUnitConfig("1.0.0-beta");
    err                          = mUnitConfig.CheckUnitConfig(higherPrereleaseVersion);
    EXPECT_EQ(err, ErrorEnum::eWrongState);
}

TEST_F(UnitConfigTest, UpdateUnitConfigSkipsOfflineNodeThenSendsOnConnect)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    SetupValidUnitConfig();

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    auto newUnitConfig = CreateTestUnitConfig("2.0.0");

    EXPECT_CALL(mJSONProvider, UnitConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIds;
    nodeIds.PushBack("node0");
    nodeIds.PushBack("node1");

    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeIds), Return(ErrorEnum::eNone)));

    UnitNodeInfo offlineNodeInfo = CreateTestNodeInfo("node1");
    offlineNodeInfo.mIsConnected = false;

    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String("node0"), _))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo("node0")), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String("node1"), _))
        .WillOnce(DoAll(SetArgReferee<1>(offlineNodeInfo), Return(ErrorEnum::eNone)));

    EXPECT_CALL(mNodeConfigHandler, UpdateNodeConfig(String("node0"), _)).WillOnce(Return(ErrorEnum::eNone));

    auto err = mUnitConfig.UpdateUnitConfig(newUnitConfig);
    EXPECT_TRUE(err.IsNone());

    // node1 comes online — its config is outdated, update must be sent
    NodeConfigStatus node1Status;
    node1Status.mVersion = "1.0.0";

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String("node1"), _))
        .WillOnce(DoAll(SetArgReferee<1>(node1Status), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeConfigHandler, UpdateNodeConfig(String("node1"), _)).WillOnce(Return(ErrorEnum::eNone));

    mUnitConfig.OnNodeInfoChanged(CreateTestNodeInfo("node1"));
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

    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

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
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo("node1")), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo("node2")), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo("node3")), Return(ErrorEnum::eNone)));

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

TEST_F(UnitConfigTest, StartStop)
{
    ASSERT_TRUE(
        mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider).IsNone());

    EXPECT_CALL(mNodeInfoProvider, SubscribeListener(Ref(mUnitConfig))).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_TRUE(mUnitConfig.Start().IsNone());

    EXPECT_CALL(mNodeInfoProvider, UnsubscribeListener(Ref(mUnitConfig))).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_EQ(mUnitConfig.Stop(), ErrorEnum::eFailed);
}

TEST_F(UnitConfigTest, InitNoMemory)
{
    CreateTestConfigFile(cValidTestUnitConfig);

    mTestAllocator.FailAll();

    auto err
        = mUnitConfig.Init(mTestAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);
    EXPECT_TRUE(err.IsNone());
}

TEST_F(UnitConfigTest, InitWithUnreadableConfigFile)
{
    ASSERT_TRUE(fs::MakeDirAll(cTestConfigFile).IsNone());

    auto err = mUnitConfig.Init(mAllocator, {cTestConfigFile}, mNodeInfoProvider, mNodeConfigHandler, mJSONProvider);
    EXPECT_TRUE(err.IsNone());

    UnitConfigStatus status;

    EXPECT_TRUE(mUnitConfig.GetUnitConfigStatus(status).IsNone());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eFailed);
    EXPECT_FALSE(status.mError.IsNone());

    EXPECT_TRUE(fs::RemoveAll(cTestConfigFile).IsNone());
}

TEST_F(UnitConfigTest, CheckUnitConfigErrors)
{
    InitInstalled();

    EXPECT_FALSE(mUnitConfig.CheckUnitConfig(CreateTestUnitConfig("invalid")).IsNone());

    const auto newConfig = CreateTestUnitConfigWithNodeID("2.0.0");

    NodeConfigStatus oldStatus;
    oldStatus.mVersion = "1.0.0";

    ExpectNodeIDs({cTestNodeID}, ErrorEnum::eFailed);
    EXPECT_EQ(mUnitConfig.CheckUnitConfig(newConfig), ErrorEnum::eFailed);

    ExpectNodeIDs({cTestNodeID});
    mTestAllocator.FailAll();
    EXPECT_EQ(mUnitConfig.CheckUnitConfig(newConfig), ErrorEnum::eNoMemory);
    mTestAllocator.Reset();

    ExpectNodeIDs({cTestNodeID});
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _)).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_EQ(mUnitConfig.CheckUnitConfig(newConfig), ErrorEnum::eNotFound);

    // Node is not connected: skipped

    auto offlineNodeInfo         = CreateTestNodeInfo();
    offlineNodeInfo.mIsConnected = false;

    ExpectNodeIDs({cTestNodeID});
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(offlineNodeInfo), Return(ErrorEnum::eNone)));
    EXPECT_TRUE(mUnitConfig.CheckUnitConfig(newConfig).IsNone());

    ExpectNodeIDs({cTestNodeID});
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _)).WillOnce(Return(ErrorEnum::eTimeout));
    EXPECT_EQ(mUnitConfig.CheckUnitConfig(newConfig), ErrorEnum::eTimeout);

    ExpectNodeIDs({cTestNodeID});
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(oldStatus), Return(ErrorEnum::eNone)));
    mTestAllocator.FailAt(1, tests::FailingAllocator::cUnlimited);
    EXPECT_EQ(mUnitConfig.CheckUnitConfig(newConfig), ErrorEnum::eNoMemory);
    mTestAllocator.Reset();

    ExpectNodeIDs({cTestNodeID});
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(oldStatus), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeConfigHandler, CheckNodeConfig(String(cTestNodeID), _))
        .WillOnce(Return(ErrorEnum::eInvalidArgument));
    EXPECT_EQ(mUnitConfig.CheckUnitConfig(newConfig), ErrorEnum::eInvalidArgument);
}

TEST_F(UnitConfigTest, CheckUnitConfigInFailedState)
{
    InitFailed();

    NodeConfigStatus oldStatus;
    oldStatus.mVersion = "1.0.0";

    ExpectNodeIDs({cTestNodeID});
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(oldStatus), Return(ErrorEnum::eNone)));

    // Node config can't be found as unit config is in failed state
    EXPECT_EQ(mUnitConfig.CheckUnitConfig(CreateTestUnitConfigWithNodeID("2.0.0")), ErrorEnum::eInvalidArgument);
}

TEST_F(UnitConfigTest, UpdateUnitConfigErrors)
{
    InitInstalled();

    EXPECT_EQ(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("0.5.0")), ErrorEnum::eWrongState);

    mTestAllocator.FailAll();
    EXPECT_EQ(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("2.0.0")), ErrorEnum::eNoMemory);
    mTestAllocator.Reset();

    EXPECT_CALL(mJSONProvider, UnitConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));
    EXPECT_EQ(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("3.0.0")), ErrorEnum::eInvalidArgument);

    EXPECT_CALL(mJSONProvider, UnitConfigToJSON(_, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    ExpectNodeIDs({}, ErrorEnum::eFailed);
    EXPECT_EQ(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("4.0.0")), ErrorEnum::eFailed);

    ExpectNodeIDs({cTestNodeID});
    mTestAllocator.FailAt(1, tests::FailingAllocator::cUnlimited);
    EXPECT_EQ(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("5.0.0")), ErrorEnum::eNoMemory);

    ExpectNodeIDs({cTestNodeID});
    mTestAllocator.FailAt(2, tests::FailingAllocator::cUnlimited);
    EXPECT_EQ(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("6.0.0")), ErrorEnum::eNoMemory);
    mTestAllocator.Reset();

    ExpectNodeIDs({cTestNodeID});
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _)).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_EQ(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("7.0.0")), ErrorEnum::eNotFound);

    ExpectNodeIDs({cTestNodeID});
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(CreateTestNodeInfo()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeConfigHandler, UpdateNodeConfig(String(cTestNodeID), _)).WillOnce(Return(ErrorEnum::eTimeout));
    EXPECT_EQ(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("8.0.0")), ErrorEnum::eTimeout);

    UnitConfigStatus status;

    EXPECT_TRUE(mUnitConfig.GetUnitConfigStatus(status).IsNone());
    EXPECT_EQ(status.mVersion, "8.0.0");
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eInstalled);
}

TEST_F(UnitConfigTest, UpdateUnitConfigInFailedState)
{
    InitFailed();

    EXPECT_CALL(mJSONProvider, UnitConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));
    ExpectNodeIDs({});

    EXPECT_TRUE(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("0.1.0")).IsNone());

    UnitConfigStatus status;

    EXPECT_TRUE(mUnitConfig.GetUnitConfigStatus(status).IsNone());
    EXPECT_EQ(status.mVersion, "0.1.0");
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eInstalled);
    EXPECT_TRUE(status.mError.IsNone());
}

TEST_F(UnitConfigTest, UpdateUnitConfigWriteError)
{
    ASSERT_TRUE(mUnitConfig
                    .Init(mAllocator, {"/nonexistent_unit_config_dir/unit_config.json"}, mNodeInfoProvider,
                        mNodeConfigHandler, mJSONProvider)
                    .IsNone());

    EXPECT_CALL(mJSONProvider, UnitConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_FALSE(mUnitConfig.UpdateUnitConfig(CreateTestUnitConfig("1.0.0")).IsNone());

    UnitConfigStatus status;

    EXPECT_TRUE(mUnitConfig.GetUnitConfigStatus(status).IsNone());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eAbsent);
}

TEST_F(UnitConfigTest, OnNodeInfoChangedErrors)
{
    InitInstalled();

    NodeConfigStatus oldStatus;
    oldStatus.mVersion = "0.9.0";

    // Node is not connected

    auto offlineNodeInfo         = CreateTestNodeInfo();
    offlineNodeInfo.mIsConnected = false;

    mUnitConfig.OnNodeInfoChanged(offlineNodeInfo);

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _)).WillOnce(Return(ErrorEnum::eFailed));

    mUnitConfig.OnNodeInfoChanged(CreateTestNodeInfo());

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(oldStatus), Return(ErrorEnum::eNone)));

    mTestAllocator.FailAll();
    mUnitConfig.OnNodeInfoChanged(CreateTestNodeInfo());
    mTestAllocator.Reset();

    EXPECT_CALL(mNodeConfigHandler, GetNodeConfigStatus(String(cTestNodeID), _))
        .WillOnce(DoAll(SetArgReferee<1>(oldStatus), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeConfigHandler, UpdateNodeConfig(String(cTestNodeID), _)).WillOnce(Return(ErrorEnum::eFailed));

    mUnitConfig.OnNodeInfoChanged(CreateTestNodeInfo());
}

} // namespace aos::cm::unitconfig
