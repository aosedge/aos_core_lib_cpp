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

#include "log.hpp"
#include "mocks/resourcemanagermock.hpp"
#include "utils.hpp"

using namespace ::testing;

namespace aos {
namespace sm {
namespace resourcemanager {

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

constexpr auto      cTestNodeConfigJSON = R"({
	"nodeType": "mainType",
	"devices": [
		{
			"name": "random",
			"sharedCount": 0,
			"groups": [
				"root"
			],
			"hostDevices": [
				"/dev/random"
			]
		},
		{
			"name": "null",
			"sharedCount": 2,
			"hostDevices": [
				"/dev/null"
			]
		},
		{
			"name": "input",
			"sharedCount": 2,
			"hostDevices": [
				"/dev/input/by-path"
			]
		},
		{
			"name": "stdin",
			"sharedCount": 2,
			"hostDevices": [
				"/dev/stdin"
			]
		}
	],
	"resources": [
		{
			"name": "bluetooth",
			"groups": ["bluetooth"]
		},
		{
			"name": "wifi",
			"groups": ["wifi-group"]
		},
		{
			"name": "system-dbus",
			"mounts": [{
				"destination": "/var/run/dbus/system_bus_socket",
				"type": "bind",
				"source": "/var/run/dbus/system_bus_socket",
				"options": ["rw", "bind"]
			}],
			"env": ["DBUS_SYSTEM_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket"]
		}
	]
})";
static const String cConfigFilePath     = "test-config.cfg";
static const String cConfigValue        = R"({"version": "1.0.0"})";
static const String cConfigVersion      = "1.0.0";
static const String cNodeType           = "mainType";

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ResourceManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        InitLog();

        auto err = FS::WriteStringToFile(cConfigFilePath, cConfigValue, S_IRUSR | S_IWUSR);
        EXPECT_TRUE(err.IsNone()) << "SetUp failed to write string to file: " << err.Message();

        mConfig.mVersion = cConfigVersion;
    }

    void InitResourceManager(const ErrorEnum cJSONParseError = ErrorEnum::eNone)
    {
        EXPECT_CALL(mJsonProvider, ParseNodeConfig).WillOnce(Invoke([&](const String&, NodeConfig& config) {
            config = mConfig;

            return cJSONParseError;
        }));

        auto err
            = mResourceManager.Init(mJsonProvider, mHostDeviceManager, mHostGroupManager, cNodeType, cConfigFilePath);
        ASSERT_TRUE(err.IsNone()) << "Failed to initialize resource manager: " << err.Message();
    }

    void AddRandomDeviceInfo(aos::sm::resourcemanager::NodeConfig& config)
    {
        DeviceInfo randomDeviceInfo;
        randomDeviceInfo.mName        = "random";
        randomDeviceInfo.mSharedCount = 0;

        auto err = randomDeviceInfo.mGroups.PushBack("root");
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add a new group: " << err;
        }

        err = randomDeviceInfo.mHostDevices.PushBack("/dev/random");
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add a new host device: " << err;
        }

        err = config.mNodeConfig.mDevices.PushBack(Move(randomDeviceInfo));
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add a new resource: " << err;
        }
    }

    void AddNullDeviceInfo(aos::sm::resourcemanager::NodeConfig& config)
    {
        DeviceInfo nullDeviceInfo;
        nullDeviceInfo.mName        = "null";
        nullDeviceInfo.mSharedCount = 2;

        auto err = nullDeviceInfo.mHostDevices.PushBack("/dev/null");
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add a new host device: " << err;
        }

        err = config.mNodeConfig.mDevices.PushBack(Move(nullDeviceInfo));
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add a new resource: " << err;
        }
    }

    void EnrichUnitConfig(aos::sm::resourcemanager::NodeConfig& config, const String& nodeType, const String& version)
    {
        config.mVersion              = version;
        config.mNodeConfig.mNodeType = nodeType;

        AddRandomDeviceInfo(config);
        AddNullDeviceInfo(config);
    }

    NodeConfig            mConfig;
    JSONProviderMock      mJsonProvider;
    HostDeviceManagerMock mHostDeviceManager;
    HostGroupManagerMock  mHostGroupManager;
    ResourceManager       mResourceManager;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ResourceManagerTest, InitSuccessfullyParsesUnitConfigFile)
{
    InitResourceManager();
}

TEST_F(ResourceManagerTest, InitSucceedsWhenUnitConfigFileDoesNotExist)
{
    FS::Remove(cConfigFilePath);

    EXPECT_CALL(mJsonProvider, ParseNodeConfig).Times(0);

    auto err = mResourceManager.Init(mJsonProvider, mHostDeviceManager, mHostGroupManager, cNodeType, cConfigFilePath);
    ASSERT_TRUE(err.IsNone()) << "Failed to initialize resource manager: " << err.Message();
}

TEST_F(ResourceManagerTest, InitSucceedsWhenUnitConfigParseFails)
{
    InitResourceManager(ErrorEnum::eFailed);
}

TEST_F(ResourceManagerTest, GetUnitConfigSucceeds)
{
    InitResourceManager();

    auto ret = mResourceManager.GetNodeConfigVersion();
    ASSERT_TRUE(ret.mError.IsNone()) << "Failed to get node config version: " << ret.mError.Message();

    EXPECT_EQ(ret.mValue, cConfigVersion) << "lhs: " << ret.mValue.CStr() << " rhs: " << cConfigVersion.CStr();
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
    DeviceInfo result;

    AddRandomDeviceInfo(mConfig);

    InitResourceManager();

    auto err = mResourceManager.GetDeviceInfo("random", result);
    ASSERT_TRUE(err.IsNone()) << "Failed to get device info: " << err.Message();
    ASSERT_EQ(result.mName, "random") << "Device info name is not equal to expected";
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

    mConfig.mNodeConfig.mResources.Back().mValue.mName = "resource-one";

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

    mConfig.mNodeConfig.mResources.Back().mValue.mName = "resource-one";

    InitResourceManager();

    err = mResourceManager.GetResourceInfo("resource-one", result);
    ASSERT_TRUE(err.IsNone()) << "Failed to get resource info: " << err.Message();
    ASSERT_EQ(resource, result) << "Resource info is not equal to expected";
}

TEST_F(ResourceManagerTest, AllocateDeviceFailsDueToConfigParseError)
{
    InitResourceManager(ErrorEnum::eFailed);

    auto err = mResourceManager.AllocateDevice("device-name", "instance-id");
    ASSERT_FALSE(err.IsNone()) << "Allocate device should fail due to config parse error";
}

TEST_F(ResourceManagerTest, AllocateDeviceFailsOnDeviceNotFoundInConfig)
{
    InitResourceManager();

    EXPECT_CALL(mHostDeviceManager, AllocateDevice).Times(0);

    auto err = mResourceManager.AllocateDevice("device-name", "instance-id");
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
}

TEST_F(ResourceManagerTest, AllocateDeviceFailsOnHostDeviceManagerSide)
{
    DeviceInfo deviceInfo;
    deviceInfo.mName = "device-name";

    auto err = mConfig.mNodeConfig.mDevices.PushBack(deviceInfo);
    ASSERT_TRUE(err.IsNone()) << "Failed to add a new resource: " << err.Message();

    InitResourceManager();

    const auto cExpectedError = ErrorEnum::eFailed;
    EXPECT_CALL(mHostDeviceManager, AllocateDevice).WillOnce(Return(cExpectedError));

    err = mResourceManager.AllocateDevice(deviceInfo.mName, "instance-id");
    ASSERT_TRUE(err.Is(cExpectedError));
}

TEST_F(ResourceManagerTest, AllocateDeviceSucceeds)
{
    DeviceInfo deviceInfo;
    deviceInfo.mName = "device-name";

    auto err = mConfig.mNodeConfig.mDevices.PushBack(deviceInfo);
    ASSERT_TRUE(err.IsNone()) << "Failed to add a new resource: " << err.Message();

    InitResourceManager();

    EXPECT_CALL(mHostDeviceManager, AllocateDevice).WillOnce(Return(ErrorEnum::eNone));

    err = mResourceManager.AllocateDevice(deviceInfo.mName, "instance-id");
    ASSERT_TRUE(err.IsNone()) << "Failed to allocate device: " << err.Message();
}

TEST_F(ResourceManagerTest, ReleaseDeviceFails)
{
    InitResourceManager();

    EXPECT_CALL(mHostDeviceManager, RemoveInstanceFromDevice).WillOnce(Return(ErrorEnum::eNotFound));

    auto err = mResourceManager.ReleaseDevice("device-name", "instance-id");
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
}

TEST_F(ResourceManagerTest, ReleaseDeviceSucceeds)
{
    InitResourceManager();

    EXPECT_CALL(mHostDeviceManager, RemoveInstanceFromDevice).WillOnce(Return(ErrorEnum::eNone));

    auto err = mResourceManager.ReleaseDevice("device-name", "instance-id");
    ASSERT_TRUE(err.IsNone()) << "Failed to release device: " << err.Message();
}

TEST_F(ResourceManagerTest, ReleaseDevicesFails)
{
    InitResourceManager();

    EXPECT_CALL(mHostDeviceManager, RemoveInstanceFromAllDevices).WillOnce(Return(ErrorEnum::eNotFound));

    auto err = mResourceManager.ReleaseDevices("instance-id");
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
}

TEST_F(ResourceManagerTest, ReleaseDevicesSucceeds)
{
    InitResourceManager();

    EXPECT_CALL(mHostDeviceManager, RemoveInstanceFromAllDevices).WillOnce(Return(ErrorEnum::eNone));

    auto err = mResourceManager.ReleaseDevices("instance-id");
    ASSERT_TRUE(err.IsNone()) << "Failed to release devices: " << err.Message();
}

TEST_F(ResourceManagerTest, GetDeviceInstancesFails)
{
    Array<StaticString<cInstanceIDLen>> instances;

    InitResourceManager();

    EXPECT_CALL(mHostDeviceManager, GetDeviceInstances).WillOnce(Return(ErrorEnum::eNotFound));

    auto err = mResourceManager.GetDeviceInstances("device-name", instances);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
}

TEST_F(ResourceManagerTest, GetDeviceInstancesSucceeds)
{
    Array<StaticString<cInstanceIDLen>> instances;

    InitResourceManager();

    EXPECT_CALL(mHostDeviceManager, GetDeviceInstances).WillOnce(Return(ErrorEnum::eNone));

    auto err = mResourceManager.GetDeviceInstances("device-name", instances);
    ASSERT_TRUE(err.IsNone()) << "Failed to get device instances: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckNodeConfigFailsOnVendorMatch)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeConfig).Times(0);

    auto err = mResourceManager.CheckNodeConfig(mConfig.mVersion, cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << "Expected error invalid argument, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckNodeConfigFailsOnConfigJSONParse)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeConfig).WillOnce(Return(ErrorEnum::eFailed));

    auto err = mResourceManager.CheckNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eFailed)) << "Expected error failed, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckNodeConfigFailsOnNodeTypeMismatch)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeConfig).WillOnce(Invoke([&](const String&, NodeConfig& config) {
        config.mNodeConfig.mNodeType = "wrongType";

        return ErrorEnum::eNone;
    }));
    auto err = mResourceManager.CheckNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << "Expected error invalid argument, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckNodeConfigSucceedsOnEmptyNodeConfigDevices)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeConfig).WillOnce(Invoke([&](const String&, NodeConfig& config) {
        config.mNodeConfig.mNodeType = cNodeType;
        config.mNodeConfig.mDevices.Clear();

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).Times(0);
    EXPECT_CALL(mHostGroupManager, GroupExists).Times(0);

    auto err = mResourceManager.CheckNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.IsNone()) << "Failed to check unit config: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckUnitConfigSucceedsOnNonEmptyUnitConfigDevices)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeConfig).WillOnce(Invoke([&](const String&, NodeConfig& config) {
        EnrichUnitConfig(config, cNodeType, "1.0.2");

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).WillRepeatedly(Return(true));
    EXPECT_CALL(mHostGroupManager, GroupExists).WillRepeatedly(Return(true));

    auto err = mResourceManager.CheckNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.IsNone()) << "Failed to check unit config: " << err.Message();
}

TEST_F(ResourceManagerTest, UpdateUnitConfigFailsOnInvalidJSON)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeConfig).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).Times(0);
    EXPECT_CALL(mHostGroupManager, GroupExists).Times(0);
    EXPECT_CALL(mJsonProvider, DumpNodeConfig).Times(0);

    auto err = mResourceManager.UpdateNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_FALSE(err.IsNone()) << "Update unit config should fail on invalid JSON";
}

TEST_F(ResourceManagerTest, UpdateUnitConfigFailsOnJSONDump)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeConfig).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mJsonProvider, DumpNodeConfig).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).Times(0);
    EXPECT_CALL(mHostGroupManager, GroupExists).Times(0);

    auto err = mResourceManager.UpdateNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_FALSE(err.IsNone()) << "Update unit config should fail on JSON dump";
}

TEST_F(ResourceManagerTest, UpdateUnitConfigSucceeds)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeConfig)
        .Times(2)
        .WillRepeatedly(Invoke([&](const String&, aos::sm::resourcemanager::NodeConfig& config) {
            EnrichUnitConfig(config, cNodeType, "1.0.2");

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).WillRepeatedly(Return(true));
    EXPECT_CALL(mHostGroupManager, GroupExists).WillRepeatedly(Return(true));

    EXPECT_CALL(mJsonProvider, DumpNodeConfig).WillOnce(Return(ErrorEnum::eNone));

    auto err = mResourceManager.UpdateNodeConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.IsNone()) << "Failed to check unit config: " << err.Message();
}

} // namespace resourcemanager
} // namespace sm
} // namespace aos
