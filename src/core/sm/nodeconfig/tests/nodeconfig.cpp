/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>
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

class FailableAllocator : public HeapAllocator {
public:
    void* Allocate(size_t size) override { return mFail ? nullptr : HeapAllocator::Allocate(size); }

    void SetFail(bool fail) { mFail = fail; }

private:
    bool mFail {};
};

class TestListener : public aos::nodeconfig::NodeConfigListenerItf {
public:
    Error OnNodeConfigChanged(const aos::NodeConfig&) override
    {
        mNotified = true;

        return mResult;
    }

    Error mResult;
    bool  mNotified = false;
};

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

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

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

    auto err = mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider);

    EXPECT_TRUE(err.IsNone());
}

TEST_F(NodeConfigTest, InitWithInvalidConfig)
{
    CreateTestConfigFile(cInvalidTestNodeConfig);

    EXPECT_CALL(mJSONProvider, NodeConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    auto err = mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider);

    EXPECT_TRUE(err.IsNone());
}

TEST_F(NodeConfigTest, InitWithMissingConfigFile)
{
    auto err = mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider);

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

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

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

    auto err = mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider);

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

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

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

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    auto newConfig = CreateTestNodeConfig("2.0.0");

    auto err = mNodeConfig.CheckNodeConfig(newConfig);
    EXPECT_TRUE(err.IsNone());
}

TEST_F(NodeConfigTest, CheckNodeConfigSameVersion)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    auto sameVersionConfig = CreateTestNodeConfig("1.0.0");

    auto err = mNodeConfig.CheckNodeConfig(sameVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eAlreadyExist);
}

TEST_F(NodeConfigTest, CheckNodeConfigLowerVersion)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    auto lowerVersionConfig = CreateTestNodeConfig("0.9.0");

    auto err = mNodeConfig.CheckNodeConfig(lowerVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eWrongState);
}

TEST_F(NodeConfigTest, CheckNodeConfigWithFailedState)
{
    CreateTestConfigFile(cInvalidTestNodeConfig);

    EXPECT_CALL(mJSONProvider, NodeConfigFromJSON(_, _)).WillOnce(Return(ErrorEnum::eInvalidArgument));

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    auto newConfig = CreateTestNodeConfig("2.0.0");

    auto err = mNodeConfig.CheckNodeConfig(newConfig);
    EXPECT_EQ(err, ErrorEnum::eInvalidArgument);
}

TEST_F(NodeConfigTest, UpdateNodeConfigSuccess)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

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

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    auto sameVersionConfig = CreateTestNodeConfig("1.0.0");

    auto err = mNodeConfig.UpdateNodeConfig(sameVersionConfig);
    EXPECT_EQ(err, ErrorEnum::eAlreadyExist);
}

TEST_F(NodeConfigTest, UpdateNodeConfigFromAbsentState)
{
    auto err = mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider);
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

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

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

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

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

TEST_F(NodeConfigTest, InitNoMemoryForConfig)
{
    FailableAllocator allocator;

    allocator.SetFail(true);

    EXPECT_TRUE(mNodeConfig.Init(allocator, {cTestConfigFile}, mJSONProvider).IsNone());
}

TEST_F(NodeConfigTest, InitWithUnreadableConfigFile)
{
    ASSERT_TRUE(fs::MakeDirAll(cTestConfigFile).IsNone());

    EXPECT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    NodeConfigStatus status;

    ASSERT_TRUE(mNodeConfig.GetNodeConfigStatus(status).IsNone());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eFailed);
    EXPECT_FALSE(status.mError.IsNone());

    // The version is not checked for the failed config, so it is replaced by the new one.
    EXPECT_CALL(mJSONProvider, NodeConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(fs::RemoveAll(cTestConfigFile).IsNone());

    EXPECT_TRUE(mNodeConfig.UpdateNodeConfig(CreateTestNodeConfig("1.0.0")).IsNone());

    NodeConfigStatus installedStatus;

    ASSERT_TRUE(mNodeConfig.GetNodeConfigStatus(installedStatus).IsNone());
    EXPECT_EQ(installedStatus.mState, UnitConfigStateEnum::eInstalled);
    EXPECT_TRUE(installedStatus.mError.IsNone());
}

TEST_F(NodeConfigTest, CheckNodeConfigInvalidVersion)
{
    CreateTestConfigFile(cValidTestNodeConfig);

    SetupValidNodeConfig();

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    EXPECT_FALSE(mNodeConfig.CheckNodeConfig(CreateTestNodeConfig("invalid")).IsNone());
    EXPECT_FALSE(mNodeConfig.UpdateNodeConfig(CreateTestNodeConfig("invalid")).IsNone());
}

TEST_F(NodeConfigTest, UpdateNodeConfigNoMemory)
{
    FailableAllocator allocator;

    ASSERT_TRUE(mNodeConfig.Init(allocator, {cTestConfigFile}, mJSONProvider).IsNone());

    allocator.SetFail(true);

    EXPECT_EQ(mNodeConfig.UpdateNodeConfig(CreateTestNodeConfig("1.0.0")), ErrorEnum::eNoMemory);
}

TEST_F(NodeConfigTest, UpdateNodeConfigToJSONFails)
{
    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    EXPECT_CALL(mJSONProvider, NodeConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_EQ(mNodeConfig.UpdateNodeConfig(CreateTestNodeConfig("1.0.0")), ErrorEnum::eFailed);

    NodeConfigStatus status;

    ASSERT_TRUE(mNodeConfig.GetNodeConfigStatus(status).IsNone());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eAbsent);
}

TEST_F(NodeConfigTest, UpdateNodeConfigWriteFileFails)
{
    const auto configFile = std::string(cTestConfigFile) + "_dir/config.json";

    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {configFile.c_str()}, mJSONProvider).IsNone());

    EXPECT_CALL(mJSONProvider, NodeConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_FALSE(mNodeConfig.UpdateNodeConfig(CreateTestNodeConfig("1.0.0")).IsNone());

    NodeConfigStatus status;

    ASSERT_TRUE(mNodeConfig.GetNodeConfigStatus(status).IsNone());
    EXPECT_EQ(status.mState, UnitConfigStateEnum::eAbsent);
}

TEST_F(NodeConfigTest, ListenerErrorIsIgnored)
{
    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    TestListener listener;

    listener.mResult = ErrorEnum::eFailed;

    ASSERT_TRUE(mNodeConfig.SubscribeListener(listener).IsNone());

    EXPECT_CALL(mJSONProvider, NodeConfigToJSON(_, _)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_TRUE(mNodeConfig.UpdateNodeConfig(CreateTestNodeConfig("1.0.0")).IsNone());
    EXPECT_TRUE(listener.mNotified);
}

TEST_F(NodeConfigTest, SubscribeListenerLimit)
{
    ASSERT_TRUE(mNodeConfig.Init(mAllocator, {cTestConfigFile}, mJSONProvider).IsNone());

    std::vector<std::unique_ptr<TestListener>> listeners;

    Error err;

    for (size_t i = 0; i < 64 && err.IsNone(); ++i) {
        listeners.push_back(std::make_unique<TestListener>());

        err = mNodeConfig.SubscribeListener(*listeners.back());
    }

    EXPECT_EQ(err, ErrorEnum::eNoMemory);

    TestListener unknown;

    EXPECT_EQ(mNodeConfig.UnsubscribeListener(unknown), ErrorEnum::eNotFound);
}

} // namespace aos::sm::nodeconfig
