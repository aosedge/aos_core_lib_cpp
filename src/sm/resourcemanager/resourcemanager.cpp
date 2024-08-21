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
    HostGroupManagerItf& hostGroupManager, const String& nodeType, const String& configPath)
{
    mJsonProvider      = &jsonProvider;
    mHostDeviceManager = &hostDeviceManager;
    mHostGroupManager  = &hostGroupManager;
    mNodeType          = nodeType;
    mConfigPath        = configPath;

    if (auto err = LoadConfig(); !err.IsNone()) {
        LOG_ERR() << "Failed to load unit config: " << err;

        mConfigError = err;
    }

    return ErrorEnum::eNone;
}

RetWithError<StaticString<cVersionLen>> ResourceManager::GetNodeConfigVersion() const
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get node config version: version=" << mConfig.mVersion;

    return {mConfig.mVersion, mConfigError};
}

Error ResourceManager::GetDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get device info: device = " << deviceName;

    auto err = GetConfigDeviceInfo(deviceName, deviceInfo);
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

    for (const auto& resource : mConfig.mNodeConfig.mResources) {
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

    if (!mConfigError.IsNone()) {
        return AOS_ERROR_WRAP(mConfigError);
    }

    DeviceInfo deviceInfo;

    auto err = GetConfigDeviceInfo(deviceName, deviceInfo);
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

Error ResourceManager::CheckNodeConfig(const String& version, const String& config) const
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Check unit config: version = " << version;

    if (version == mConfig.mVersion) {
        LOG_ERR() << "Invalid node config version version";

        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    sm::resourcemanager::NodeConfig updatedConfig;
    auto                            err = mJsonProvider->ParseNodeConfig(config, updatedConfig);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ValidateNodeConfig(updatedConfig);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::UpdateNodeConfig(const String& version, const String& config)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Update config: version = " << version;

    sm::resourcemanager::NodeConfig updatedConfig;

    auto err = mJsonProvider->ParseNodeConfig(config, updatedConfig);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to parse config: " << err;

        return AOS_ERROR_WRAP(err);
    }

    updatedConfig.mVersion = version;

    StaticString<cConfigJSONLen> newConfigJson;
    err = mJsonProvider->DumpNodeConfig(updatedConfig, newConfigJson);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to dump config: " << err;

        return AOS_ERROR_WRAP(err);
    }

    err = FS::WriteStringToFile(mConfigPath, newConfigJson, S_IRUSR | S_IWUSR);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to write config: " << err;

        return AOS_ERROR_WRAP(err);
    }

    err = LoadConfig();
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to load config: " << err;

        mConfigError = err;

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ResourceManager::LoadConfig()
{
    StaticString<cConfigJSONLen> config;

    auto err = FS::ReadFileToString(mConfigPath, config);
    if (!err.IsNone()) {
        if (err == ENOENT) {
            mConfig.mVersion = "0.0.0";
            return ErrorEnum::eNone;
        }
    }

    err = mJsonProvider->ParseNodeConfig(config, mConfig);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::ValidateNodeConfig(const aos::sm::resourcemanager::NodeConfig& config) const
{
    if (mNodeType != config.mNodeConfig.mNodeType) {
        LOG_ERR() << "Invalid node type";

        return ErrorEnum::eInvalidArgument;
    }

    if (auto err = ValidateDevices(config.mNodeConfig.mDevices); !err.IsNone()) {
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

Error ResourceManager::GetConfigDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const
{
    for (auto& device : mConfig.mNodeConfig.mDevices) {
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
