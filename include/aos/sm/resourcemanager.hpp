/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RESOURCEMANAGER_HPP_
#define AOS_RESOURCEMANAGER_HPP_

#include "aos/common/tools/error.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/string.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/types.hpp"

namespace aos {
namespace sm {
namespace resourcemanager {

/**
 * Node Config.
 */
struct NodeConfig {
    aos::NodeConfig           mNodeConfig;
    StaticString<cVersionLen> mVersion;
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
     * @param config config object.
     * @param[out] json json representation of config.
     * @return Error.
     */
    virtual Error DumpNodeConfig(const NodeConfig& config, String& json) const = 0;

    /**
     * Parses config object from string.
     *
     * @param json json representation of config.
     * @param[out] config config.
     * @return Error.
     */
    virtual Error ParseNodeConfig(const String& json, NodeConfig& config) const = 0;
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
     * Allocates device for instance.
     *
     * @param deviceInfo device info.
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error AllocateDevice(const DeviceInfo& deviceInfo, const String& instanceID) = 0;

    /**
     * Removes instance from device.
     *
     * @param deviceName device name.
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error RemoveInstanceFromDevice(const String& deviceName, const String& instanceID) = 0;

    /**
     * Removes instance from all devices.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error RemoveInstanceFromAllDevices(const String& instanceID) = 0;

    /**
     * Returns ID list of instances that allocate specific device.
     *
     * @param deviceName device name.
     * @param instances[out] param to store instance ID(s).
     * @return Error.
     */
    virtual Error GetDeviceInstances(const String& deviceName, Array<StaticString<cInstanceIDLen>>& instanceIDs) const
        = 0;

    /**
     * Checks if device exists.
     *
     * @param device device name.
     * @return true if device exists, false otherwise.
     */
    virtual bool DeviceExists(const String& device) const = 0;
};

/**
 * Host group manager interface.
 */
class HostGroupManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~HostGroupManagerItf() = default;

    /**
     * Checks if group exists.
     *
     * @param group group name.
     * @return true if group exists, false otherwise.
     */
    virtual bool GroupExists(const String& group) const = 0;
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
    virtual Error GetResourceInfo(const String& resourceName, ResourceInfo& resourceInfo) const = 0;

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
     * Returns ID list of instances that allocate specific device.
     *
     * @param deviceName device name.
     * @param instances[out] param to store instance ID(s).
     * @return Error.
     */
    virtual Error GetDeviceInstances(const String& deviceName, Array<StaticString<cInstanceIDLen>>& instanceIDs) const
        = 0;

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
     * @param hostGroupManager host group manager.
     * @param nodeType node type.
     * @param configPath path to config file.
     * @result Error.
     */
    Error Init(JSONProviderItf& jsonProvider, HostDeviceManagerItf& hostDeviceManager,
        HostGroupManagerItf& hostGroupManager, const String& nodeType, const String& configPath);

    /**
     * Returns current node config version.
     *
     * @return RetWithError<StaticString<cVersionLen>>.
     */
    RetWithError<StaticString<cVersionLen>> GetNodeConfigVersion() const override;

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
    Error GetResourceInfo(const String& resourceName, ResourceInfo& resourceInfo) const override;

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
     * Returns ID list of instances that allocate specific device.
     *
     * @param deviceName device name.
     * @param instances[out] param to store instance ID(s).
     * @return Error.
     */
    Error GetDeviceInstances(const String& deviceName, Array<StaticString<cInstanceIDLen>>& instanceIDs) const override;

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

private:
    static constexpr auto cConfigJSONLen = AOS_CONFIG_UNIT_CONFIG_JSON_LEN;

    Error LoadConfig();
    Error ValidateNodeConfig(const NodeConfig& config) const;
    Error ValidateDevices(const Array<DeviceInfo>& devices) const;
    Error GetConfigDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const;

    mutable Mutex              mMutex;
    JSONProviderItf*           mJsonProvider {nullptr};
    HostDeviceManagerItf*      mHostDeviceManager {nullptr};
    HostGroupManagerItf*       mHostGroupManager {nullptr};
    StaticString<cNodeTypeLen> mNodeType;
    StaticString<cFilePathLen> mConfigPath;
    Error                      mConfigError {ErrorEnum::eNone};
    NodeConfig                 mConfig;
};

} // namespace resourcemanager
} // namespace sm
} // namespace aos

#endif
