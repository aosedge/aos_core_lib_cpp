/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/sm/nodeconfig/nodeconfig.hpp>

#include "mocks/jsonprovidermock.hpp"

using namespace testing;

namespace aos::sm::nodeconfig {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestConfigFile = "/tmp/test_node_config.json";

constexpr auto cValidTestNodeConfig = R"({
     "nodeId": "node0",
     "nodeType": "type1",
     "version": "1.0.0"
 })";

constexpr auto cInvalidTestNodeConfig = R"({
     something not valid
 })";

} // namespace

/***********************************************************************************************************************
 * Test fixture
 **********************************************************************************************************************/

class NodeConfigTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();
        fs::Remove(cTestConfigFile);
    }

    void TearDown() override { fs::Remove(cTestConfigFile); }

    aos::NodeConfig CreateTestNodeConfig(const String& version)
    {
        aos::NodeConfig config;
        config.mVersion  = version;
        config.mNodeID   = "node0";
        config.mNodeType = "type1";

        return config;
    }

    void SetupValidNodeConfig()
    {
        auto config = CreateTestNodeConfig("1.0.0");

        EXPECT_CALL(mJSONProvider, NodeConfigFromJSON(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(config), Return(ErrorEnum::eNone)));
    }

    void CreateTestConfigFile(const String& jsonConfig = cValidTestNodeConfig)
    {
        auto err = fs::WriteStringToFile(cTestConfigFile, jsonConfig, 0600);
        ASSERT_TRUE(err.IsNone()) << "Failed to create test config file";
    }

    StrictMock<JSONProviderMock> mJSONProvider;
    NodeConfig                   mNodeConfig;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(NodeConfigTest, InitWithValidConfig)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    auto err = mNodeConfig.Init({cTestConfigFile}, mJSONProvider);

    EXPECT_TRUE(err.IsNone());
}

TEST_F(NodeConfigTest, InitWithInvalidConfig)
{
    CreateTestConfigFile(cInvalidTestNodeConfig);

    EXPECT_CALL(mJSONProvider, NodeConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    auto err = mNodeConfig.Init({cTestConfigFile}, mJSONProvider);

    EXPECT_TRUE(err.IsNone());
}

TEST_F(NodeConfigTest, InitWithMissingConfigFile)
{
    auto err = mNodeConfig.Init({cTestConfigFile}, mJSONProvider);

    EXPECT_TRUE(err.IsNone());

    NodeConfigStatus status;
    err = mNodeConfig.GetNodeConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_TRUE(status.mVersion.IsEmpty());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eAbsent);
}

TEST_F(NodeConfigTest, GetNodeConfigStatusValid)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    NodeConfigStatus status;
    auto             err = mNodeConfig.GetNodeConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mVersion, "1.0.0");
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eInstalled);
    EXPECT_TRUE(status.mError.IsNone());
}

TEST_F(NodeConfigTest, GetNodeConfigStatusWithError)
{
    CreateTestConfigFile(cInvalidTestNodeConfig);

    EXPECT_CALL(mJSONProvider, NodeConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    auto err = mNodeConfig.Init({cTestConfigFile}, mJSONProvider);

    EXPECT_TRUE(err.IsNone());

    NodeConfigStatus status;
    err = mNodeConfig.GetNodeConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eFailed);
    EXPECT_FALSE(status.mError.IsNone());
}

TEST_F(NodeConfigTest, GetNodeConfig)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    aos::NodeConfig config;
    auto            err = mNodeConfig.GetNodeConfig(config);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(config.mNodeID, "node0");
    EXPECT_EQ(config.mNodeType, "type1");
    EXPECT_EQ(config.mVersion, "1.0.0");
}

TEST_F(NodeConfigTest, CheckNodeConfigValidVersion)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    auto newConfig = CreateTestNodeConfig("2.0.0");

    auto err = mNodeConfig.CheckNodeConfig(newConfig);
    EXPECT_TRUE(err.IsNone());
}

TEST_F(NodeConfigTest, CheckNodeConfigSameVersion)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    auto sameVersionConfig = CreateTestNodeConfig("1.0.0");

    auto err = mNodeConfig.CheckNodeConfig(sameVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eAlreadyExist);
}

TEST_F(NodeConfigTest, CheckNodeConfigLowerVersion)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    auto lowerVersionConfig = CreateTestNodeConfig("0.9.0");

    auto err = mNodeConfig.CheckNodeConfig(lowerVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eWrongState);
}

TEST_F(NodeConfigTest, CheckNodeConfigWithFailedState)
{
    CreateTestConfigFile(cInvalidTestNodeConfig);

    EXPECT_CALL(mJSONProvider, NodeConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    auto newConfig = CreateTestNodeConfig("2.0.0");

    auto err = mNodeConfig.CheckNodeConfig(newConfig);
    EXPECT_EQ(err, ErrorEnum::eInvalidArgument);
}

TEST_F(NodeConfigTest, UpdateNodeConfigSuccess)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    auto newConfig = CreateTestNodeConfig("2.0.0");

    EXPECT_CALL(mJSONProvider, NodeConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    auto err = mNodeConfig.UpdateNodeConfig(newConfig);
    EXPECT_TRUE(err.IsNone());

    NodeConfigStatus status;
    err = mNodeConfig.GetNodeConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mVersion, "2.0.0");
}

TEST_F(NodeConfigTest, UpdateNodeConfigSameVersion)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    auto sameVersionConfig = CreateTestNodeConfig("1.0.0");

    auto err = mNodeConfig.UpdateNodeConfig(sameVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eAlreadyExist);
}

TEST_F(NodeConfigTest, UpdateNodeConfigFromAbsentState)
{
    auto err = mNodeConfig.Init({cTestConfigFile}, mJSONProvider);
    EXPECT_TRUE(err.IsNone());

    auto newConfig = CreateTestNodeConfig("1.0.0");

    EXPECT_CALL(mJSONProvider, NodeConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    err = mNodeConfig.UpdateNodeConfig(newConfig);
    EXPECT_TRUE(err.IsNone());

    NodeConfigStatus status;
    err = mNodeConfig.GetNodeConfigStatus(status);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(status.mVersion, "1.0.0");
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eInstalled);
}

TEST_F(NodeConfigTest, SubscribeAndNotifyListener)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    class TestListener : public aos::nodeconfig::NodeConfigListenerItf {
    public:
        Error OnNodeConfigChanged(const aos::NodeConfig& nodeConfig) override
        {
            mReceivedConfig = nodeConfig;
            mNotified       = true;
            return ErrorEnum::eNone;
        }

        aos::NodeConfig mReceivedConfig;
        bool            mNotified = false;
    };

    TestListener listener;

    auto err = mNodeConfig.SubscribeListener(listener);
    EXPECT_TRUE(err.IsNone());

    auto newConfig = CreateTestNodeConfig("2.0.0");

    EXPECT_CALL(mJSONProvider, NodeConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    err = mNodeConfig.UpdateNodeConfig(newConfig);
    EXPECT_TRUE(err.IsNone());
    EXPECT_TRUE(listener.mNotified);
    EXPECT_EQ(listener.mReceivedConfig.mVersion, "2.0.0");
}

TEST_F(NodeConfigTest, UnsubscribeListener)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init({cTestConfigFile}, mJSONProvider).IsNone());

    class TestListener : public aos::nodeconfig::NodeConfigListenerItf {
    public:
        Error OnNodeConfigChanged(const aos::NodeConfig&) override
        {
            mNotified = true;
            return ErrorEnum::eNone;
        }

        bool mNotified = false;
    };

    TestListener listener;

    auto err = mNodeConfig.SubscribeListener(listener);
    EXPECT_TRUE(err.IsNone());

    err = mNodeConfig.UnsubscribeListener(listener);
    EXPECT_TRUE(err.IsNone());

    auto newConfig = CreateTestNodeConfig("2.0.0");

    EXPECT_CALL(mJSONProvider, NodeConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    err = mNodeConfig.UpdateNodeConfig(newConfig);
    EXPECT_TRUE(err.IsNone());
    EXPECT_FALSE(listener.mNotified);
}

} // namespace aos::sm::nodeconfig
