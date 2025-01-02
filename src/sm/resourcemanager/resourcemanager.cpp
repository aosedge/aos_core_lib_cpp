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

namespace aos::sm::resourcemanager {

/***********************************************************************************************************************
 * ResourceManager
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ResourceManager::Init(JSONProviderItf& jsonProvider, HostDeviceManagerItf& hostDeviceManager,
    const String& nodeType, const String& configPath)
{
    mJsonProvider      = &jsonProvider;
    mHostDeviceManager = &hostDeviceManager;
    mNodeType          = nodeType;
    mConfigPath        = configPath;

    if (auto err = LoadConfig(); !err.IsNone()) {
        LOG_ERR() << "Failed to load unit config: err=" << err;
    }

    LOG_DBG() << "Node config version: version=" << mConfig.mVersion << ", error=" << mConfigError;

    return ErrorEnum::eNone;
}

RetWithError<StaticString<cVersionLen>> ResourceManager::GetNodeConfigVersion() const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get node config version: version=" << mConfig.mVersion;

    return {mConfig.mVersion, mConfigError};
}

Error ResourceManager::GetNodeConfig(aos::NodeConfig& nodeConfig) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get node config";

    nodeConfig = mConfig.mNodeConfig;

    return mConfigError;
}

Error ResourceManager::GetDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get device info: device=" << deviceName;

    auto err = GetConfigDeviceInfo(deviceName, deviceInfo);
    if (!err.IsNone()) {
        LOG_ERR() << "Device not found: device=" << deviceName;

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::GetResourceInfo(const String& resourceName, ResourceInfo& resourceInfo) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get resource info: resourceName=" << resourceName;

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
    LockGuard lock {mMutex};

    LOG_DBG() << "Allocate device: device=" << deviceName << ", instance=" << instanceID;

    if (!mConfigError.IsNone()) {
        return AOS_ERROR_WRAP(mConfigError);
    }

    DeviceInfo deviceInfo;

    if (auto err = GetConfigDeviceInfo(deviceName, deviceInfo); !err.IsNone()) {
        LOG_ERR() << "Device not found: device=" << deviceName;

        return AOS_ERROR_WRAP(err);
    }

    auto [deviceIt, err] = mAllocatedDevices.Find(deviceName);
    if (!err.IsNone()) {
        StaticArray<StaticString<cInstanceIDLen>, 1> instances;
        instances.PushBack(instanceID);

        return mAllocatedDevices.Set(deviceName, Move(instances));
    }

    auto& instances = deviceIt->mSecond;

    if (instances.Find(instanceID).mError.IsNone()) {
        LOG_WRN() << "Device is already allocated by instance: device=" << deviceName << ", instance=" << instanceID;

        return ErrorEnum::eNone;
    }

    if (deviceInfo.mSharedCount != 0 && instances.Size() >= static_cast<size_t>(deviceInfo.mSharedCount)) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "no device available"));
    }

    if (err = instances.PushBack(instanceID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::ReleaseDevice(const String& deviceName, const String& instanceID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Release device: device=" << deviceName << ", instance=" << instanceID;

    auto [it, err] = mAllocatedDevices.Find(deviceName);

    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "device not found"));
    }

    if (auto count = it->mSecond.Remove(instanceID); count < 1) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance not found"));
    }

    if (it->mSecond.IsEmpty()) {
        mAllocatedDevices.Erase(it);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::ReleaseDevices(const String& instanceID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Release devices: instanceID=" << instanceID;

    Error err = ErrorEnum::eNotFound;

    auto it = mAllocatedDevices.begin();
    while (it != mAllocatedDevices.end()) {
        if (auto count = it->mSecond.Remove(instanceID); count > 0) {
            err = ErrorEnum::eNone;
        }

        if (it->mSecond.IsEmpty()) {
            it = mAllocatedDevices.Erase(it);
        } else {
            ++it;
        }
    }

    return err;
}

Error ResourceManager::ResetAllocatedDevices()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Reset allocated devices";

    mAllocatedDevices.Clear();

    return ErrorEnum::eNone;
}

Error ResourceManager::GetDeviceInstances(
    const String& deviceName, Array<StaticString<cInstanceIDLen>>& instanceIDs) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get device instances: device=" << deviceName;

    auto [it, err] = mAllocatedDevices.Find(deviceName);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instance : it->mSecond) {
        if (err = instanceIDs.PushBack(instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::CheckNodeConfig(const String& version, const String& config) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Check unit config: version=" << version;

    if (version == mConfig.mVersion) {
        LOG_ERR() << "Invalid node config version version";

        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    auto updatedConfig = MakeUnique<sm::resourcemanager::NodeConfig>(&mAllocator);

    auto err = mJsonProvider->NodeConfigFromJSON(config, *updatedConfig);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ValidateNodeConfig(*updatedConfig);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::UpdateNodeConfig(const String& version, const String& config)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update config: version=" << version;

    auto updatedConfig = MakeUnique<sm::resourcemanager::NodeConfig>(&mAllocator);

    if (auto err = mJsonProvider->NodeConfigFromJSON(config, *updatedConfig); !err.IsNone()) {
        LOG_ERR() << "Failed to parse config: err=" << err;

        return AOS_ERROR_WRAP(err);
    }

    updatedConfig->mVersion = version;

    if (auto err = WriteConfig(*updatedConfig); !err.IsNone()) {
        LOG_ERR() << "Failed to write config: err=" << err;

        return err;
    }

    if (auto err = LoadConfig(); !err.IsNone()) {
        LOG_ERR() << "Failed to load config: err=" << err;

        return err;
    }

    for (auto& subscriber : mSubscribers) {
        subscriber->ReceiveNodeConfig(mConfig);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::SubscribeCurrentNodeConfigChange(NodeConfigReceiverItf& receiver)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe to current node config change";

    for (auto& subscriber : mSubscribers) {
        if (subscriber == &receiver) {
            return ErrorEnum::eAlreadyExist;
        }
    }

    if (auto err = mSubscribers.PushBack(&receiver); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::UnsubscribeCurrentNodeConfigChange(NodeConfigReceiverItf& receiver)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe from current node config change";

    return (mSubscribers.Remove(&receiver) > 0) ? ErrorEnum::eNone : ErrorEnum::eNotFound;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ResourceManager::LoadConfig()
{
    auto configJSON = MakeUnique<StaticString<cNodeConfigJSONLen>>(&mAllocator);

    auto err = FS::ReadFileToString(mConfigPath, *configJSON);
    if (!err.IsNone()) {
        if (err == ENOENT) {
            mConfig.mVersion = "0.0.0";

            return ErrorEnum::eNone;
        }

        mConfigError = err;

        return AOS_ERROR_WRAP(err);
    }

    err = mJsonProvider->NodeConfigFromJSON(*configJSON, mConfig);
    if (!err.IsNone()) {
        mConfigError = err;

        return AOS_ERROR_WRAP(err);
    }

    if (err = ValidateNodeConfig(mConfig); !err.IsNone()) {
        mConfigError = err;

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::WriteConfig(const NodeConfig& config)
{
    auto configJSON = MakeUnique<StaticString<cNodeConfigJSONLen>>(&mAllocator);

    if (auto err = mJsonProvider->NodeConfigToJSON(config, *configJSON); !err.IsNone()) {
        LOG_ERR() << "Failed to dump config: err=" << err;

        return AOS_ERROR_WRAP(err);
    }

    if (auto err = FS::WriteStringToFile(mConfigPath, *configJSON, S_IRUSR | S_IWUSR); !err.IsNone()) {
        LOG_ERR() << "Failed to write config: err=" << err;

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceManager::ValidateNodeConfig(const resourcemanager::NodeConfig& config) const
{
    if (!config.mNodeConfig.mNodeType.IsEmpty() && config.mNodeConfig.mNodeType != mNodeType) {
        LOG_ERR() << "Invalid node type";

        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
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
            if (auto err = mHostDeviceManager->CheckDevice(hostDevice); !err.IsNone()) {
                LOG_ERR() << "Host device not found: device=" << hostDevice;

                return AOS_ERROR_WRAP(err);
            }
        }

        // check groups
        for (const auto& group : device.mGroups) {
            if (auto err = mHostDeviceManager->CheckGroup(group); !err.IsNone()) {
                LOG_ERR() << "Host group not found: group=" << group;

                return AOS_ERROR_WRAP(err);
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

} // namespace aos::sm::resourcemanager
