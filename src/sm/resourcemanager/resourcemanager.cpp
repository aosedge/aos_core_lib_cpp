/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <unistd.h>

#include "aos/common/tools/fs.hpp"
#include "aos/sm/resourcemanager.hpp"
#include "log.hpp"

namespace aos {
namespace sm {
namespace resourcemanager {

/***********************************************************************************************************************
 * ResourceManager
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ResourceManager::Init(JSONProviderItf& jsonProvider, HostDeviceManagerItf& hostDeviceManager,
    HostGroupManagerItf& hostGroupManager, const String& nodeType, const String& unitConfigPath)
{
    mJsonProvider      = &jsonProvider;
    mHostDeviceManager = &hostDeviceManager;
    mHostGroupManager  = &hostGroupManager;
    mNodeType          = nodeType;
    mUnitConfigPath    = unitConfigPath;

    auto err = LoadUnitConfig();
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to load unit config: " << err;

        mUnitConfigError = err;
    }

    return ErrorEnum::eNone;
}

RetWithError<StaticString<cVersionLen>> ResourceManager::GetVersion() const
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get unit config info";

    return {mUnitConfig.mVendorVersion, ErrorEnum::eNone};
}

Error ResourceManager::GetDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get device info: device = " << deviceName;

    auto err = GetUnitConfigDeviceInfo(deviceName, deviceInfo);
    if (!err.IsNone()) {
        LOG_ERR() << "Device not found: device = " << deviceName;

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::GetResourceInfo(const String& resourceName, ResourceInfo& resourceInfo) const
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get resource info: resourceName = " << resourceName;

    for (const auto& resource : mUnitConfig.mNodeUnitConfig.mResources) {
        if (resource.mName == resourceName) {
            resourceInfo = resource;

            return ErrorEnum::eNone;
        }
    }

    return ErrorEnum::eNotFound;
}

Error ResourceManager::AllocateDevice(const String& deviceName, const String& instanceID)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Allocate device: device = " << deviceName << ", instance = " << instanceID;

    if (!mUnitConfigError.IsNone()) {
        return AOS_ERROR_WRAP(mUnitConfigError);
    }

    DeviceInfo deviceInfo;

    auto err = GetUnitConfigDeviceInfo(deviceName, deviceInfo);
    if (!err.IsNone()) {
        LOG_ERR() << "Device not found: device = " << deviceName;

        return AOS_ERROR_WRAP(err);
    }

    return mHostDeviceManager->AllocateDevice(deviceInfo, instanceID);
}

Error ResourceManager::ReleaseDevice(const String& deviceName, const String& instanceID)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Release device: device = " << deviceName << ", instance = " << instanceID;

    return mHostDeviceManager->RemoveInstanceFromDevice(deviceName, instanceID);
}

Error ResourceManager::ReleaseDevices(const String& instanceID)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Release devices: instanceID = " << instanceID;

    return mHostDeviceManager->RemoveInstanceFromAllDevices(instanceID);
}

Error ResourceManager::GetDeviceInstances(
    const String& deviceName, Array<StaticString<cInstanceIDLen>>& instanceIDs) const
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get device instances: device = " << deviceName;

    return mHostDeviceManager->GetDeviceInstances(deviceName, instanceIDs);
}

Error ResourceManager::CheckUnitConfig(const String& version, const String& unitConfig) const
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Check unit config: version = " << version;

    if (version == mUnitConfig.mVendorVersion) {
        LOG_INF() << "Invalid vendor version";

        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    UnitConfig updatedUnitConfig;
    auto       err = mJsonProvider->ParseNodeUnitConfig(unitConfig, updatedUnitConfig);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ValidateUnitConfig(updatedUnitConfig.mNodeUnitConfig);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::UpdateUnitConfig(const String& version, const String& unitConfig)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Update unit config: version = " << version;

    UnitConfig updatedUnitConfig;

    auto err = mJsonProvider->ParseNodeUnitConfig(unitConfig, updatedUnitConfig);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to parse update unit config: " << err;

        return AOS_ERROR_WRAP(err);
    }

    updatedUnitConfig.mVendorVersion = version;

    StaticString<cConfigJSONLen> newUnitConfigJson;
    err = mJsonProvider->DumpUnitConfig(updatedUnitConfig, newUnitConfigJson);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to dump update unit config: " << err;

        return AOS_ERROR_WRAP(err);
    }

    err = FS::WriteStringToFile(mUnitConfigPath, newUnitConfigJson, S_IRUSR | S_IWUSR);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to write update unit config: " << err;

        return AOS_ERROR_WRAP(err);
    }

    err = LoadUnitConfig();
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to load updated unit config: " << err;

        mUnitConfigError = err;

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ResourceManager::LoadUnitConfig()
{
    StaticString<cConfigJSONLen> unitConfig;

    auto err = FS::ReadFileToString(mUnitConfigPath, unitConfig);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to read unit config file: path = " << mUnitConfigPath << ", error = " << err;

        return err;
    }

    err = mJsonProvider->ParseNodeUnitConfig(unitConfig, mUnitConfig);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to parse unit config file: " << err;

        return err;
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::ValidateUnitConfig(const NodeUnitConfig& nodeUnitConfig) const
{
    if (mNodeType != nodeUnitConfig.mNodeType) {
        LOG_ERR() << "Invalid node type";

        return ErrorEnum::eInvalidArgument;
    }

    if (auto err = ValidateDevices(nodeUnitConfig.mDevices); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::ValidateDevices(const Array<DeviceInfo>& devices) const
{
    for (const auto& device : devices) {
        // check host devices
        for (const auto& hostDevice : device.mHostDevices) {
            if (!mHostDeviceManager->DeviceExists(hostDevice)) {
                LOG_ERR() << "Host device not found: device = " << hostDevice;

                return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
            }
        }

        // check groups
        for (const auto& group : device.mGroups) {
            if (!mHostGroupManager->GroupExists(group)) {
                LOG_ERR() << "Host group not found: group = " << group;

                return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::GetUnitConfigDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const
{
    for (auto& device : mUnitConfig.mNodeUnitConfig.mDevices) {
        if (device.mName == deviceName) {
            deviceInfo = device;

            return ErrorEnum::eNone;
        }
    }

    return ErrorEnum::eNotFound;
}

} // namespace resourcemanager
} // namespace sm
} // namespace aos
