/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>

#include <gtest/gtest.h>

#include "aos/common/tools/fs.hpp"
#include "aos/sm/resourcemanager.hpp"

#include "aos/test/log.hpp"
#include "aos/test/utils.hpp"
#include "mocks/resourcemanagermock.hpp"

using namespace ::testing;

namespace aos::sm::resourcemanager {

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

static constexpr auto cConfigFilePath       = "test-config.cfg";
static constexpr auto cConfigFileContent    = "";
static constexpr auto cTestNodeConfigJSON   = "";
static constexpr auto cConfigVersion        = "1.0.0";
static constexpr auto cDefaultConfigVersion = "0.0.0";
static constexpr auto cNodeType             = "mainType";

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static DeviceInfo CreateDeviceInfo(
    const String& name, int sharedCount, const std::vector<std::string>& hosts, const std::vector<std::string>& groups)
{
    DeviceInfo deviceInfo;

    deviceInfo.mName        = name;
    deviceInfo.mSharedCount = sharedCount;

    for (const auto& host : hosts) {
        deviceInfo.mHostDevices.PushBack(host.c_str());
    }

    for (const auto& group : groups) {
        deviceInfo.mGroups.PushBack(group.c_str());
    }

    return deviceInfo;
}

static NodeConfig CreateNodeConfig(
    const String& nodeType, const String& version, const std::vector<DeviceInfo>& devices = {})
{
    NodeConfig nodeConfig;

    nodeConfig.mVersion              = version;
    nodeConfig.mNodeConfig.mNodeType = nodeType;

    for (const auto& device : devices) {
        nodeConfig.mNodeConfig.mDevices.PushBack(device);
    }

    return nodeConfig;
}

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ResourceManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test::InitLog();

        auto err = FS::WriteStringToFile(cConfigFilePath, cConfigFileContent, S_IRUSR | S_IWUSR);
        EXPECT_TRUE(err.IsNone()) << "SetUp failed to write string to file: " << err.Message();

        mConfig.mVersion = cConfigVersion;
    }

    void InitResourceManager(ErrorEnum nodeConfigError = ErrorEnum::eNone)
    {
        EXPECT_CALL(mJsonProvider, NodeConfigFromJSON).WillOnce(Invoke([&](const String&, NodeConfig& config) {
            if (nodeConfigError == ErrorEnum::eNone) {
                config = mConfig;
            }

            return nodeConfigError;
        }));

        auto err = mResourceManager.Init(mJsonProvider, mHostDeviceManager, cNodeType, cConfigFilePath);
        ASSERT_TRUE(err.IsNone()) << "Failed to initialize resource manager: " << err.Message();
    }

    NodeConfig             mConfig = CreateNodeConfig(cNodeType, cConfigVersion);
    JSONProviderMock       mJsonProvider;
    HostDeviceManagerMock  mHostDeviceManager;
    ResourceManager        mResourceManager;
    NodeConfigReceiverMock mNodeConfigReceiver;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ResourceManagerTest, InitSucceeds)
{
    InitResourceManager();

    aos::NodeConfig nodeConfig;

    ASSERT_TRUE(mResourceManager.GetNodeConfig(nodeConfig).IsNone());
    EXPECT_EQ(nodeConfig, mConfig.mNodeConfig);

    auto [version, err] = mResourceManager.GetNodeConfigVersion();

    ASSERT_TRUE(err.IsNone());
    ASSERT_EQ(version, mConfig.mVersion);
}

TEST_F(ResourceManagerTest, NodeConfigFileMissingIsNotAnError)
{
    FS::Remove(cConfigFilePath);

    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON).Times(0);

    ASSERT_TRUE(mResourceManager.Init(mJsonProvider, mHostDeviceManager, cNodeType, cConfigFilePath).IsNone());

    auto [version, err] = mResourceManager.GetNodeConfigVersion();

    ASSERT_TRUE(err.IsNone());
    ASSERT_EQ(version, String(cDefaultConfigVersion));
}

TEST_F(ResourceManagerTest, InitSucceedsWhenNodeConfigParseFails)
{
    const auto expectedError = ErrorEnum::eFailed;

    InitResourceManager(expectedError);

    aos::NodeConfig nodeConfig;

    ASSERT_TRUE(mResourceManager.GetNodeConfig(nodeConfig).Is(expectedError));
}

TEST_F(ResourceManagerTest, GetDeviceInfoFails)
{
    DeviceInfo result;

    mConfig.mNodeConfig.mDevices.Clear();
    InitResourceManager();

    auto err = mResourceManager.GetDeviceInfo("random", result);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
    ASSERT_TRUE(result.mName.IsEmpty()) << "Device info name is not empty";
}

TEST_F(ResourceManagerTest, GetDeviceInfoSucceeds)
{
    const auto deviceInfo = CreateDeviceInfo("random", 0, {"/dev/random"}, {"root"});

    ASSERT_TRUE(mConfig.mNodeConfig.mDevices.PushBack(deviceInfo).IsNone());
    EXPECT_CALL(mHostDeviceManager, CheckDevice(String("/dev/random"))).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHostDeviceManager, CheckGroup(String("root"))).WillOnce(Return(ErrorEnum::eNone));

    DeviceInfo result;

    InitResourceManager();

    ASSERT_TRUE(mResourceManager.GetDeviceInfo(deviceInfo.mName, result).IsNone());

    ASSERT_EQ(deviceInfo, result) << "Device info name is not equal to expected";
}

TEST_F(ResourceManagerTest, GetResourceInfoFailsOnEmptyResourcesConfig)
{
    ResourceInfo result;

    // Clear resources
    mConfig.mNodeConfig.mResources.Clear();

    InitResourceManager();

    auto err = mResourceManager.GetResourceInfo("resource-name", result);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
}

TEST_F(ResourceManagerTest, GetResourceInfoFailsResourceNotFound)
{
    ResourceInfo result;

    ResourceInfo resource;
    resource.mName = "resource-one";

    auto err = mConfig.mNodeConfig.mResources.PushBack(resource);
    ASSERT_TRUE(err.IsNone()) << "Failed to add a new resource: " << err.Message();

    mConfig.mNodeConfig.mResources.Back().mName = "resource-one";

    InitResourceManager();

    err = mResourceManager.GetResourceInfo("resource-not-found", result);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
}

TEST_F(ResourceManagerTest, GetResourceSucceeds)
{
    ResourceInfo result;

    ResourceInfo resource;
    resource.mName = "resource-one";

    auto err = mConfig.mNodeConfig.mResources.PushBack(resource);
    ASSERT_TRUE(err.IsNone()) << "Failed to add a new resource: " << err.Message();

    mConfig.mNodeConfig.mResources.Back().mName = "resource-one";

    InitResourceManager();

    err = mResourceManager.GetResourceInfo("resource-one", result);
    ASSERT_TRUE(err.IsNone()) << "Failed to get resource info: " << err.Message();
    ASSERT_EQ(resource, result) << "Resource info is not equal to expected";
}

TEST_F(ResourceManagerTest, AllocateDeviceFailsDueToConfigParseError)
{
    InitResourceManager(ErrorEnum::eFailed);

    ASSERT_FALSE(mResourceManager.AllocateDevice("device-name", "instance-id").IsNone());
}

TEST_F(ResourceManagerTest, AllocateDeviceFailsOnDeviceNotFoundInConfig)
{
    mConfig.mNodeConfig.mDevices.Clear();

    InitResourceManager();

    ASSERT_TRUE(mResourceManager.AllocateDevice("device-name", "instance-id").Is(ErrorEnum::eNotFound));
}

TEST_F(ResourceManagerTest, AllocateDevice)
{
    struct TestCase {
        DeviceInfo                                mDeviceInfo;
        std::vector<StaticString<cInstanceIDLen>> mInstancesToAllocate;
        std::vector<StaticString<cInstanceIDLen>> mExpectedInstances;
    } testCases[] = {
        {CreateDeviceInfo("device0", 0, {"/dev/zero"}, {"group0"}), {"instance0", "instance1", "instance2"},
            {"instance0", "instance1", "instance2"}},
        {CreateDeviceInfo("device1", 1, {"/dev/one"}, {"group1"}), {"instance0", "instance1", "instance2"},
            {"instance0"}},
        {CreateDeviceInfo("device2", 2, {"/dev/two"}, {"group2"}), {"instance0", "instance1", "instance2"},
            {"instance0", "instance1"}},
    };

    StaticArray<StaticString<cInstanceIDLen>, ArraySize(testCases)> instances;

    // Initialize resource manager config
    for (const auto& testCase : testCases) {
        ASSERT_TRUE(mConfig.mNodeConfig.mDevices.PushBack(testCase.mDeviceInfo).IsNone());

        EXPECT_CALL(mHostDeviceManager, CheckDevice(testCase.mDeviceInfo.mHostDevices[0]))
            .WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mHostDeviceManager, CheckGroup(testCase.mDeviceInfo.mGroups[0])).WillOnce(Return(ErrorEnum::eNone));
    }

    InitResourceManager();

    for (const auto& testCase : testCases) {
        for (const auto& instance : testCase.mInstancesToAllocate) {
            bool expected = std::find(testCase.mExpectedInstances.begin(), testCase.mExpectedInstances.end(), instance)
                != testCase.mExpectedInstances.end();

            ASSERT_EQ(mResourceManager.AllocateDevice(testCase.mDeviceInfo.mName, instance).IsNone(), expected)
                << "Allocate device expected to return " << (expected ? "error" : "success") << " for instance "
                << instance.CStr();
        }

        instances.Clear();

        ASSERT_TRUE(mResourceManager.GetDeviceInstances(testCase.mDeviceInfo.mName, instances).IsNone());

        ASSERT_EQ(instances,
            Array<StaticString<cInstanceIDLen>>(testCase.mExpectedInstances.data(), testCase.mExpectedInstances.size()))
            << "Allocated instances mismatch for device " << testCase.mDeviceInfo.mName.CStr();
    }
}

TEST_F(ResourceManagerTest, AllocateDeviceForTheSameInstanceIsNotAnError)
{
    const auto deviceInfo = CreateDeviceInfo("device0", 1, {"/dev/zero"}, {"group0"});

    ASSERT_TRUE(mConfig.mNodeConfig.mDevices.PushBack(deviceInfo).IsNone());

    EXPECT_CALL(mHostDeviceManager, CheckDevice(deviceInfo.mHostDevices[0])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHostDeviceManager, CheckGroup(deviceInfo.mGroups[0])).WillOnce(Return(ErrorEnum::eNone));

    InitResourceManager();

    for (size_t i = 0; i < 2; ++i) {
        ASSERT_TRUE(mResourceManager.AllocateDevice(deviceInfo.mName, "instance0").IsNone());
    }

    StaticArray<StaticString<cInstanceIDLen>, 2> instances;

    ASSERT_TRUE(mResourceManager.GetDeviceInstances(deviceInfo.mName, instances).IsNone());

    ASSERT_EQ(instances.Size(), 1);
    ASSERT_EQ(instances[0], String("instance0"));
}

TEST_F(ResourceManagerTest, ReleaseDevice)
{
    const auto device0Info = CreateDeviceInfo("device0", 2, {"/dev/null"}, {"group0"});
    const auto device1Info = CreateDeviceInfo("device1", 2, {"/dev/zero"}, {"group1"});

    ASSERT_TRUE(mConfig.mNodeConfig.mDevices.PushBack(device0Info).IsNone());
    ASSERT_TRUE(mConfig.mNodeConfig.mDevices.PushBack(device1Info).IsNone());

    InitResourceManager();

    ASSERT_TRUE(mResourceManager.ReleaseDevice(device0Info.mName, "unknown").Is(ErrorEnum::eNotFound));
    ASSERT_TRUE(mResourceManager.ReleaseDevices(device1Info.mName).Is(ErrorEnum::eNotFound));

    ASSERT_TRUE(mResourceManager.AllocateDevice(device0Info.mName, "instance0").IsNone());
    ASSERT_TRUE(mResourceManager.AllocateDevice(device1Info.mName, "instance0").IsNone());

    ASSERT_TRUE(mResourceManager.AllocateDevice(device0Info.mName, "instance1").IsNone());

    // shared count exceeded
    ASSERT_FALSE(mResourceManager.AllocateDevice(device0Info.mName, "instance2").IsNone());

    // release one instance
    ASSERT_TRUE(mResourceManager.ReleaseDevice(device0Info.mName, "instance0").IsNone());

    StaticArray<StaticString<cInstanceIDLen>, 2> instances;
    ASSERT_TRUE(mResourceManager.GetDeviceInstances(device0Info.mName, instances).IsNone());

    ASSERT_EQ(instances.Size(), 1);
    ASSERT_EQ(instances[0], "instance1");

    ASSERT_TRUE(mResourceManager.AllocateDevice(device0Info.mName, "instance2").IsNone());

    instances.Clear();
    ASSERT_TRUE(mResourceManager.GetDeviceInstances(device0Info.mName, instances).IsNone());

    ASSERT_EQ(instances.Size(), 2);
    ASSERT_EQ(instances[0], String("instance1"));
    ASSERT_EQ(instances[1], String("instance2"));

    // clear all instances from device
    ASSERT_TRUE(mResourceManager.ReleaseDevice(device0Info.mName, "instance1").IsNone());
    ASSERT_TRUE(mResourceManager.ReleaseDevices("instance2").IsNone());

    instances.Clear();
    ASSERT_TRUE(mResourceManager.GetDeviceInstances(device0Info.mName, instances).Is(ErrorEnum::eNotFound));

    instances.Clear();
    ASSERT_TRUE(mResourceManager.GetDeviceInstances(device1Info.mName, instances).IsNone());

    ASSERT_EQ(instances.Size(), 1);
    ASSERT_EQ(instances[0], "instance0");
}

TEST_F(ResourceManagerTest, ResetAllocatedDevices)
{
    const auto deviceInfo = CreateDeviceInfo("device0", 2, {"/dev/zero"}, {"group0"});

    ASSERT_TRUE(mConfig.mNodeConfig.mDevices.PushBack(deviceInfo).IsNone());

    InitResourceManager();

    ASSERT_TRUE(mResourceManager.AllocateDevice(deviceInfo.mName, "instance0").IsNone());
    ASSERT_TRUE(mResourceManager.AllocateDevice(deviceInfo.mName, "instance1").IsNone());

    StaticArray<StaticString<cInstanceIDLen>, 2> instances;
    ASSERT_TRUE(mResourceManager.GetDeviceInstances(deviceInfo.mName, instances).IsNone());

    ASSERT_EQ(instances.Size(), 2);

    ASSERT_TRUE(mResourceManager.ResetAllocatedDevices().IsNone());

    // device should be empty cause all instances are released
    instances.Clear();
    ASSERT_TRUE(mResourceManager.GetDeviceInstances(deviceInfo.mName, instances).Is(ErrorEnum::eNotFound));
}

TEST_F(ResourceManagerTest, GetDeviceInstancesReturnsNotFound)
{
    InitResourceManager();

    StaticArray<StaticString<cInstanceIDLen>, 1> instances;

    ASSERT_TRUE(mResourceManager.GetDeviceInstances("unknown", instances).Is(ErrorEnum::eNotFound));
}

TEST_F(ResourceManagerTest, CheckNodeConfigFailsOnVendorMatch)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON).Times(0);

    auto err = mResourceManager.CheckNodeConfig(mConfig.mVersion, cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << "Expected error invalid argument, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckNodeConfigFailsOnConfigJSONParse)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON).WillOnce(Return(ErrorEnum::eFailed));

    auto err = mResourceManager.CheckNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eFailed)) << "Expected error failed, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckNodeConfigFailsOnNodeTypeMismatch)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON).WillOnce(Invoke([&](const String&, NodeConfig& config) {
        config.mNodeConfig.mNodeType = "wrongType";

        return ErrorEnum::eNone;
    }));
    auto err = mResourceManager.CheckNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << "Expected error invalid argument, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckNodeConfigSucceedsOnEmptyNodeConfigDevices)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON).WillOnce(Invoke([&](const String&, NodeConfig& config) {
        config.mNodeConfig.mNodeType = cNodeType;
        config.mNodeConfig.mDevices.Clear();

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mHostDeviceManager, CheckDevice).Times(0);
    EXPECT_CALL(mHostDeviceManager, CheckGroup).Times(0);

    auto err = mResourceManager.CheckNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.IsNone()) << "Failed to check unit config: " << err.Message();
}

TEST_F(ResourceManagerTest, UpdateNodeConfigFailsOnInvalidJSON)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_CALL(mHostDeviceManager, CheckDevice).Times(0);
    EXPECT_CALL(mHostDeviceManager, CheckGroup).Times(0);
    EXPECT_CALL(mJsonProvider, NodeConfigToJSON).Times(0);

    auto err = mResourceManager.UpdateNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_FALSE(err.IsNone()) << "Update unit config should fail on invalid JSON";
}

TEST_F(ResourceManagerTest, UpdateNodeConfigFailsOnJSONDump)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mJsonProvider, NodeConfigToJSON).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_CALL(mHostDeviceManager, CheckDevice).Times(0);
    EXPECT_CALL(mHostDeviceManager, CheckGroup).Times(0);

    auto err = mResourceManager.UpdateNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_FALSE(err.IsNone()) << "Update unit config should fail on JSON dump";
}

TEST_F(ResourceManagerTest, UpdateNodeConfigSucceedsAndObserverIsNotified)
{
    const auto device        = CreateDeviceInfo("device-name", 0, {"/dev/random"}, {"root"});
    const auto updatedConfig = CreateNodeConfig(cNodeType, "1.0.2", {device});

    InitResourceManager();

    ASSERT_TRUE(mResourceManager.SubscribeCurrentNodeConfigChange(mNodeConfigReceiver).IsNone());

    EXPECT_CALL(mNodeConfigReceiver, ReceiveNodeConfig(updatedConfig)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mJsonProvider, NodeConfigToJSON).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON)
        .WillRepeatedly(DoAll(SetArgReferee<1>(updatedConfig), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mHostDeviceManager, CheckDevice).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHostDeviceManager, CheckGroup).WillRepeatedly(Return(ErrorEnum::eNone));

    ASSERT_TRUE(mResourceManager.UpdateNodeConfig("1.0.2", cTestNodeConfigJSON).IsNone());

    ASSERT_TRUE(mResourceManager.UnsubscribeCurrentNodeConfigChange(mNodeConfigReceiver).IsNone());
}

TEST_F(ResourceManagerTest, UpdateNodeConfigFailsAndObserverIsNotNotified)
{
    const auto device        = CreateDeviceInfo("device-name", 0, {"/dev/random"}, {"root"});
    const auto updatedConfig = CreateNodeConfig(cNodeType, "1.0.2", {device});

    InitResourceManager();

    ASSERT_TRUE(mResourceManager.SubscribeCurrentNodeConfigChange(mNodeConfigReceiver).IsNone());

    EXPECT_CALL(mNodeConfigReceiver, ReceiveNodeConfig).Times(0);
    EXPECT_CALL(mJsonProvider, NodeConfigToJSON).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mJsonProvider, NodeConfigFromJSON)
        .WillRepeatedly(DoAll(SetArgReferee<1>(updatedConfig), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mHostDeviceManager, CheckDevice).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHostDeviceManager, CheckGroup).WillRepeatedly(Return(ErrorEnum::eNone));

    ASSERT_FALSE(mResourceManager.UpdateNodeConfig("1.0.2", cTestNodeConfigJSON).IsNone());

    ASSERT_TRUE(mResourceManager.UnsubscribeCurrentNodeConfigChange(mNodeConfigReceiver).IsNone());
}

TEST_F(ResourceManagerTest, SubscribeReturnsErrorIfAlreadySubscribed)
{
    ASSERT_TRUE(mResourceManager.SubscribeCurrentNodeConfigChange(mNodeConfigReceiver).IsNone());
    ASSERT_TRUE(mResourceManager.SubscribeCurrentNodeConfigChange(mNodeConfigReceiver).Is(ErrorEnum::eAlreadyExist));
}

} // namespace aos::sm::resourcemanager
