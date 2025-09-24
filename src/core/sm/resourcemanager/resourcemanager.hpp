/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_RESOURCEMANAGER_RESOURCEMANAGER_HPP_
#define AOS_CORE_SM_RESOURCEMANAGER_RESOURCEMANAGER_HPP_

#include <core/common/tools/error.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/noncopyable.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/types.hpp>
#include <core/sm/config.hpp>

namespace aos::sm::resourcemanager {

/**
 * Node config JSON length.
 */
static constexpr auto cNodeConfigJSONLen = AOS_CONFIG_RESOURCEMANAGER_NODE_CONFIG_JSON_LEN;

/**
 * Node Config.
 */
struct NodeConfig {
    StaticString<cVersionLen>                                   mVersion;
    StaticString<cNodeTypeLen>                                  mNodeType;
    StaticString<cNodeIDLen>                                    mNodeID;
    Optional<AlertRules>                                        mAlertRules;
    Optional<ResourceRatios>                                    mResourceRatios;
    StaticArray<DeviceInfo, cMaxNumNodeDevices>                 mDevices;
    StaticArray<ResourceInfoObsolete, cMaxNumNodeResources>     mResources;
    StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels> mLabels;
    uint64_t                                                    mPriority {0};

    /**
     * Compares node config.
     *
     * @param nodeConfig node config to compare.
     * @return bool.
     */
    bool operator==(const NodeConfig& nodeConfig) const
    {
        return mNodeType == nodeConfig.mNodeType && mNodeID == nodeConfig.mNodeID
            && mAlertRules == nodeConfig.mAlertRules && mResourceRatios == nodeConfig.mResourceRatios
            && mDevices == nodeConfig.mDevices && mResources == nodeConfig.mResources && mLabels == nodeConfig.mLabels
            && mPriority == nodeConfig.mPriority;
    }

    /**
     * Compares node config.
     *
     * @param nodeConfig node config to compare.
     * @return bool.
     */
    bool operator!=(const NodeConfig& nodeConfig) const { return !operator==(nodeConfig); }
};

/**
 * JSON provider interface.
 */
class JSONProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~JSONProviderItf() = default;

    /**
     * Dumps config object into string.
     *
     * @param nodeConfig node config object.
     * @param[out] json node config JSON string.
     * @return Error.
     */
    virtual Error NodeConfigToJSON(const NodeConfig& nodeConfig, String& json) const = 0;

    /**
     * Creates node config object from a JSON string.
     *
     * @param json node config JSON string.
     * @param[out] nodeConfig node config object.
     * @return Error.
     */
    virtual Error NodeConfigFromJSON(const String& json, NodeConfig& nodeConfig) const = 0;
};

/**
 * Host device manager interface.
 */
class HostDeviceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~HostDeviceManagerItf() = default;

    /**
     * Checks if device exists.
     *
     * @param device device name.
     * @return true if device exists, false otherwise.
     */
    virtual Error CheckDevice(const String& device) const = 0;

    /**
     * Checks if group exists.
     *
     * @param group group name.
     * @return true if group exists, false otherwise.
     */
    virtual Error CheckGroup(const String& group) const = 0;
};

/**
 * Node config receiver interface.
 */
class NodeConfigReceiverItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigReceiverItf() = default;

    /**
     * Receives node config.
     *
     * @param nodeConfig node config.
     * @return Error.
     */
    virtual Error ReceiveNodeConfig(const NodeConfig& nodeConfig) = 0;
};

/**
 * Resource manager interface.
 */
class ResourceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ResourceManagerItf() = default;

    /**
     * Returns current node config version.
     *
     * @return RetWithError<StaticString<cVersionLen>>.
     */
    virtual RetWithError<StaticString<cVersionLen>> GetNodeConfigVersion() const = 0;

    /**
     * Returns node config.
     *
     * @param nodeConfig[out] param to store node config.
     * @return Error.
     */
    virtual Error GetNodeConfig(NodeConfig& nodeConfig) const = 0;

    /**
     * Gets device info by name.
     *
     * @param deviceName device name.
     * @param[out] deviceInfo param to store device info.
     * @return Error.
     */
    virtual Error GetDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const = 0;

    /**
     * Gets resource info by name.
     *
     * @param resourceName resource name.
     * @param[out] resourceInfo param to store resource info.
     * @return Error.
     */
    virtual Error GetResourceInfo(const String& resourceName, ResourceInfoObsolete& resourceInfo) const = 0;

    /**
     * Allocates device by name.
     *
     * @param deviceName device name.
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error AllocateDevice(const String& deviceName, const String& instanceID) = 0;

    /**
     * Releases device for instance.
     *
     * @param deviceName device name.
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error ReleaseDevice(const String& deviceName, const String& instanceID) = 0;

    /**
     * Releases all previously allocated devices for instance.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error ReleaseDevices(const String& instanceID) = 0;

    /**
     * Resets allocated devices.
     *
     * @return Error.
     */
    virtual Error ResetAllocatedDevices() = 0;

    /**
     * Returns ID list of instances that allocate specific device.
     *
     * @param deviceName device name.
     * @param instances[out] param to store instance ID(s).
     * @return Error.
     */
    virtual Error GetDeviceInstances(const String& deviceName, Array<StaticString<cIDLen>>& instanceIDs) const = 0;

    /**
     * Checks configuration.
     *
     * @param version config version
     * @param config string with configuration.
     * @return Error.
     */
    virtual Error CheckNodeConfig(const String& version, const String& config) const = 0;

    /**
     * Updates configuration.
     *
     * @param version config version.
     * @param config string with configuration.
     * @return Error.
     */
    virtual Error UpdateNodeConfig(const String& version, const String& config) = 0;

    /**
     * Subscribes to current node config change.
     *
     * @param receiver node config receiver.
     * @return Error.
     */
    virtual Error SubscribeCurrentNodeConfigChange(NodeConfigReceiverItf& receiver) = 0;

    /**
     * Unsubscribes to current node config change.
     *
     * @param receiver node config receiver.
     * @return Error.
     */
    virtual Error UnsubscribeCurrentNodeConfigChange(NodeConfigReceiverItf& receiver) = 0;
};

/**
 * Resource manager instance.
 */

class ResourceManager : public ResourceManagerItf, private NonCopyable {
public:
    /**
     * Initializes the object.
     *
     * @param jsonProvider JSON provider.
     * @param hostDeviceManager host device manager.
     * @param nodeType node type.
     * @param configPath path to config file.
     * @result Error.
     */
    Error Init(JSONProviderItf& jsonProvider, HostDeviceManagerItf& hostDeviceManager, const String& nodeType,
        const String& configPath);

    /**
     * Returns current node config version.
     *
     * @return RetWithError<StaticString<cVersionLen>>.
     */
    RetWithError<StaticString<cVersionLen>> GetNodeConfigVersion() const override;

    /**
     * Returns node config.
     *
     * @param nodeConfig[out] param to store node config.
     * @return Error.
     */
    Error GetNodeConfig(NodeConfig& nodeConfig) const override;

    /**
     * Gets device info by name.
     *
     * @param deviceName device name.
     * @param[out] deviceInfo param to store device info.
     * @return Error.
     */
    Error GetDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const override;

    /**
     * Gets resource info by name.
     *
     * @param resourceName resource name.
     * @param[out] resourceInfo param to store resource info.
     * @return Error.
     */
    Error GetResourceInfo(const String& resourceName, ResourceInfoObsolete& resourceInfo) const override;

    /**
     * Allocates device by name.
     *
     * @param deviceName device name.
     * @param instanceID instance ID.
     * @return Error.
     */
    Error AllocateDevice(const String& deviceName, const String& instanceID) override;

    /**
     * Releases device for instance.
     *
     * @param deviceName device name.
     * @param instanceID instance ID.
     * @return Error.
     */
    Error ReleaseDevice(const String& deviceName, const String& instanceID) override;

    /**
     * Releases all previously allocated devices for instance.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    Error ReleaseDevices(const String& instanceID) override;

    /**
     * Resets allocated devices.
     *
     * @return Error.
     */
    Error ResetAllocatedDevices() override;

    /**
     * Returns ID list of instances that allocate specific device.
     *
     * @param deviceName device name.
     * @param instances[out] param to store instance ID(s).
     * @return Error.
     */
    Error GetDeviceInstances(const String& deviceName, Array<StaticString<cIDLen>>& instanceIDs) const override;

    /**
     * Checks configuration.
     *
     * @param version unit config version
     * @param config string with  configuration.
     * @return Error.
     */
    Error CheckNodeConfig(const String& version, const String& config) const override;

    /**
     * Updates configuration.
     *
     * @param version unit config version.
     * @param config string with configuration.
     * @return Error.
     */
    Error UpdateNodeConfig(const String& version, const String& config) override;

    /**
     * Subscribes to current node config change.
     *
     * @param receiver node config receiver.
     * @return Error.
     */
    Error SubscribeCurrentNodeConfigChange(NodeConfigReceiverItf& receiver) override;

    /**
     * Unsubscribes to current node config change.
     *
     * @param receiver node config receiver.
     * @return Error.
     */
    Error UnsubscribeCurrentNodeConfigChange(NodeConfigReceiverItf& receiver) override;

private:
    static constexpr auto cMaxNodeConfigChangeSubscribers = 2;
    static constexpr auto cAllocatorSize = Max(sizeof(StaticString<cNodeConfigJSONLen>) + sizeof(NodeConfig),
        sizeof(DeviceInfo) + sizeof(StaticArray<StaticString<cIDLen>, cMaxNumInstances>));

    Error LoadConfig();
    Error WriteConfig(const NodeConfig& config);
    Error ValidateNodeConfig(const NodeConfig& config) const;
    Error ValidateDevices(const Array<DeviceInfo>& devices) const;
    Error GetConfigDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const;

    mutable Mutex                                                        mMutex;
    mutable StaticAllocator<cAllocatorSize>                              mAllocator;
    JSONProviderItf*                                                     mJsonProvider {nullptr};
    HostDeviceManagerItf*                                                mHostDeviceManager {nullptr};
    StaticString<cNodeTypeLen>                                           mNodeType;
    StaticString<cFilePathLen>                                           mConfigPath;
    Error                                                                mConfigError {ErrorEnum::eNone};
    NodeConfig                                                           mConfig;
    StaticArray<NodeConfigReceiverItf*, cMaxNodeConfigChangeSubscribers> mSubscribers;

    mutable StaticMap<StaticString<cDeviceNameLen>, StaticArray<StaticString<cIDLen>, cMaxNumInstances>,
        cMaxNumNodeDevices>
        mAllocatedDevices;
};

} // namespace aos::sm::resourcemanager

#endif
