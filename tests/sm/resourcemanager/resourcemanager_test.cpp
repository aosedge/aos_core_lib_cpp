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

constexpr auto      cTestNodeConfigJSON  = R"({
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
constexpr auto      cEmptyUnitConfigJSON = R"({"vendorVersion" : "1.0", "nodeType" : "mainType", "devices" : []})";
static const String cUnitConfigFilePath  = "test-unit-config.cfg";
static const String cUnitConfigValue     = R"({"version": "1.0.0"})";
static const String cUnitConfigVersion   = "1.0.0";
static const String cNodeType            = "mainType";

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ResourceManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        InitLogs();

        auto err = FS::WriteStringToFile(cUnitConfigFilePath, cUnitConfigValue, S_IRUSR | S_IWUSR);
        EXPECT_TRUE(err.IsNone()) << "SetUp failed to write string to file: " << err.Message();

        mUnitConfig.mVendorVersion = cUnitConfigVersion;
    }

    void InitResourceManager(const ErrorEnum cJSONParseError = ErrorEnum::eNone)
    {
        EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).WillOnce(Invoke([&](const String&, UnitConfig& unitConfig) {
            unitConfig = mUnitConfig;

            return cJSONParseError;
        }));

        auto err = mResourceManager.Init(
            mJsonProvider, mHostDeviceManager, mHostGroupManager, cNodeType, cUnitConfigFilePath);
        ASSERT_TRUE(err.IsNone()) << "Failed to initialize resource manager: " << err.Message();
    }

    void AddRandomDeviceInfo(NodeUnitConfig& nodeConfig)
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

        err = nodeConfig.mDevices.PushBack(Move(randomDeviceInfo));
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add a new resource: " << err;
        }
    }

    void AddNullDeviceInfo(NodeUnitConfig& nodeConfig)
    {
        DeviceInfo nullDeviceInfo;
        nullDeviceInfo.mName        = "null";
        nullDeviceInfo.mSharedCount = 2;

        auto err = nullDeviceInfo.mHostDevices.PushBack("/dev/null");
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add a new host device: " << err;
        }

        err = nodeConfig.mDevices.PushBack(Move(nullDeviceInfo));
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add a new resource: " << err;
        }
    }

    void EnrichUnitConfig(UnitConfig& unitConfig, const String& nodeType, const String& version)
    {
        unitConfig.mVendorVersion = version;

        auto& nodeConfig     = unitConfig.mNodeUnitConfig;
        nodeConfig.mNodeType = nodeType;

        AddRandomDeviceInfo(nodeConfig);
        AddNullDeviceInfo(nodeConfig);
    }

    UnitConfig            mUnitConfig;
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
    FS::Remove(cUnitConfigFilePath);

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).Times(0);

    auto err
        = mResourceManager.Init(mJsonProvider, mHostDeviceManager, mHostGroupManager, cNodeType, cUnitConfigFilePath);
    ASSERT_TRUE(err.IsNone()) << "Failed to initialize resource manager: " << err.Message();
}

TEST_F(ResourceManagerTest, InitSucceedsWhenUnitConfigParseFails)
{
    InitResourceManager(ErrorEnum::eFailed);
}

TEST_F(ResourceManagerTest, GetUnitConfigSucceeds)
{
    InitResourceManager();

    auto ret = mResourceManager.GetVersion();
    ASSERT_TRUE(ret.mError.IsNone()) << "Failed to get unit config info: " << ret.mError.Message();

    EXPECT_EQ(ret.mValue, cUnitConfigVersion) << "lhs: " << ret.mValue.CStr() << " rhs: " << cUnitConfigVersion.CStr();
}

TEST_F(ResourceManagerTest, GetDeviceInfoFails)
{
    DeviceInfo result;

    mUnitConfig.mNodeUnitConfig.mDevices.Clear();
    InitResourceManager();

    auto err = mResourceManager.GetDeviceInfo("random", result);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
    ASSERT_TRUE(result.mName.IsEmpty()) << "Device info name is not empty";
}

TEST_F(ResourceManagerTest, GetDeviceInfoSucceeds)
{
    DeviceInfo result;

    AddRandomDeviceInfo(mUnitConfig.mNodeUnitConfig);

    InitResourceManager();

    auto err = mResourceManager.GetDeviceInfo("random", result);
    ASSERT_TRUE(err.IsNone()) << "Failed to get device info: " << err.Message();
    ASSERT_EQ(result.mName, "random") << "Device info name is not equal to expected";
}

TEST_F(ResourceManagerTest, GetResourceInfoFailsOnEmptyResourcesConfig)
{
    ResourceInfo result;

    // Clear resources
    mUnitConfig.mNodeUnitConfig.mResources.Clear();

    InitResourceManager();

    auto err = mResourceManager.GetResourceInfo("resource-name", result);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
}

TEST_F(ResourceManagerTest, GetResourceInfoFailsResourceNotFound)
{
    ResourceInfo result;

    ResourceInfo resource;
    resource.mName = "resource-one";

    auto err = mUnitConfig.mNodeUnitConfig.mResources.PushBack(resource);
    ASSERT_TRUE(err.IsNone()) << "Failed to add a new resource: " << err.Message();

    mUnitConfig.mNodeUnitConfig.mResources.Back().mValue.mName = "resource-one";

    InitResourceManager();

    err = mResourceManager.GetResourceInfo("resource-not-found", result);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected error not found, got: " << err.Message();
}

TEST_F(ResourceManagerTest, GetResourceSucceeds)
{
    ResourceInfo result;

    ResourceInfo resource;
    resource.mName = "resource-one";

    auto err = mUnitConfig.mNodeUnitConfig.mResources.PushBack(resource);
    ASSERT_TRUE(err.IsNone()) << "Failed to add a new resource: " << err.Message();

    mUnitConfig.mNodeUnitConfig.mResources.Back().mValue.mName = "resource-one";

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

    auto err = mUnitConfig.mNodeUnitConfig.mDevices.PushBack(deviceInfo);
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

    auto err = mUnitConfig.mNodeUnitConfig.mDevices.PushBack(deviceInfo);
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

TEST_F(ResourceManagerTest, CheckUnitConfigFailsOnVendorVersionMatch)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).Times(0);

    auto err = mResourceManager.CheckUnitConfig(mUnitConfig.mVendorVersion, cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << "Expected error invalid argument, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckUnitConfigFailsOnConfigJSONParse)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).WillOnce(Return(ErrorEnum::eFailed));

    auto err = mResourceManager.CheckUnitConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eFailed)) << "Expected error failed, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckUnitConfigFailsOnNodeTypeMismatch)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).WillOnce(Invoke([&](const String&, UnitConfig& unitConfig) {
        unitConfig.mNodeUnitConfig.mNodeType = "wrongType";

        return ErrorEnum::eNone;
    }));
    auto err = mResourceManager.CheckUnitConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << "Expected error invalid argument, got: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckUnitConfigSucceedsOnEmptyUnitConfigDevices)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).WillOnce(Invoke([&](const String&, UnitConfig& unitConfig) {
        unitConfig.mNodeUnitConfig.mNodeType = cNodeType;
        unitConfig.mNodeUnitConfig.mDevices.Clear();

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).Times(0);
    EXPECT_CALL(mHostGroupManager, GroupExists).Times(0);

    auto err = mResourceManager.CheckUnitConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.IsNone()) << "Failed to check unit config: " << err.Message();
}

TEST_F(ResourceManagerTest, CheckUnitConfigSucceedsOnNonEmptyUnitConfigDevices)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).WillOnce(Invoke([&](const String&, UnitConfig& unitConfig) {
        EnrichUnitConfig(unitConfig, cNodeType, "1.0.2");

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).WillRepeatedly(Return(true));
    EXPECT_CALL(mHostGroupManager, GroupExists).WillRepeatedly(Return(true));

    auto err = mResourceManager.CheckUnitConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.IsNone()) << "Failed to check unit config: " << err.Message();
}

TEST_F(ResourceManagerTest, UpdateUnitConfigFailsOnInvalidJSON)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).Times(0);
    EXPECT_CALL(mHostGroupManager, GroupExists).Times(0);
    EXPECT_CALL(mJsonProvider, DumpUnitConfig).Times(0);

    auto err = mResourceManager.UpdateUnitConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_FALSE(err.IsNone()) << "Update unit config should fail on invalid JSON";
}

TEST_F(ResourceManagerTest, UpdateUnitConfigFailsOnJSONDump)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mJsonProvider, DumpUnitConfig).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).Times(0);
    EXPECT_CALL(mHostGroupManager, GroupExists).Times(0);

    auto err = mResourceManager.UpdateUnitConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_FALSE(err.IsNone()) << "Update unit config should fail on JSON dump";
}

TEST_F(ResourceManagerTest, UpdateUnitConfigSucceeds)
{
    InitResourceManager();

    EXPECT_CALL(mJsonProvider, ParseNodeUnitConfig)
        .Times(2)
        .WillRepeatedly(Invoke([&](const String&, UnitConfig& unitConfig) {
            EnrichUnitConfig(unitConfig, cNodeType, "1.0.2");

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mHostDeviceManager, DeviceExists).WillRepeatedly(Return(true));
    EXPECT_CALL(mHostGroupManager, GroupExists).WillRepeatedly(Return(true));

    EXPECT_CALL(mJsonProvider, DumpUnitConfig).WillOnce(Return(ErrorEnum::eNone));

    auto err = mResourceManager.UpdateUnitConfig("1.0.2", cTestNodeConfigJSON);
    ASSERT_TRUE(err.IsNone()) << "Failed to check unit config: " << err.Message();
}

} // namespace resourcemanager
} // namespace sm
} // namespace aos
