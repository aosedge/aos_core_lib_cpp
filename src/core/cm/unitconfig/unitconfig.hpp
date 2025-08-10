/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UNITCONFIG_UNITCONFIG_HPP_
#define AOS_CORE_CM_UNITCONFIG_UNITCONFIG_HPP_

#include <string>
#include <vector>

#include <core/cm/config.hpp>
#include <core/cm/nodeinfoprovider/nodeinfoprovider.hpp>
#include <core/common/cloudprotocol/desiredstatus.hpp>
#include <core/common/cloudprotocol/unitstatus.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/types.hpp>

namespace aos::cm::unitconfig {

constexpr auto cUnitConfigJSONLen = AOS_CONFIG_UNIT_CONFIG_JSON_LEN;

struct NodeConfigStatus {
    StaticString<cNodeIDLen>   mNodeID;
    StaticString<cNodeTypeLen> mNodeType;
    StaticString<cVersionLen>  mVersion;
    Error                      mError;
};

/**
 * Node config change listener interface.
 */
class NodeConfigChangeListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigChangeListenerItf() = default;

    /**
     * Called when node config changes.
     *
     * @param config node config.
     */
    virtual void OnNodeConfigChange(const cloudprotocol::NodeConfig& config) = 0;
};

/**
 * Node config publisher interface.
 */
class NodeConfigPublisherItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigPublisherItf() = default;

    /**
     * Subscribes to current node config changes.
     *
     * @param listener listener to subscribe to.
     */
    virtual Error SubscribeCurrentNodeConfigChange(NodeConfigChangeListenerItf* listener) = 0;

    /**
     * Unsubscribes from current node config changes.
     *
     * @param listener listener to unsubscribe from.
     */
    virtual void UnsubscribeCurrentNodeConfigChange(NodeConfigChangeListenerItf* listener) = 0;
};

/**
 * Node config status listener interface. (SMController)
 */
class NodeConfigStatusListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigStatusListenerItf() = default;

    /**
     * Called when node config status changes.
     *
     * @param status node config status.
     */
    virtual void OnNodeConfigStatus(const NodeConfigStatus& status) = 0;
};

/**
 * Node config status publisher interface. (SMController)
 */
class NodeConfigStatusPublisherItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigStatusPublisherItf() = default;

    /**
     * Subscribes to node config status changes.
     *
     * @param listener listener to subscribe to.
     */
    virtual void SubscribeNodeConfigStatus(NodeConfigStatusListenerItf* listener) = 0;

    /**
     * Unsubscribes from node config status changes.
     *
     * @param listener listener to unsubscribe from.
     */
    virtual void UnsubscribeNodeConfigStatus(NodeConfigStatusListenerItf* listener) = 0;
};

/**
 * Node config controller interface. (SMController)
 */
class NodeConfigControllerItf : public NodeConfigStatusPublisherItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeConfigControllerItf() = default;

    /**
     * Checks node config.
     *
     * @param nodeID node ID.
     * @param version version.
     * @param nodeConfig node config.
     * @return Error.
     */
    virtual Error CheckNodeConfig(
        const String& nodeID, const String& version, const cloudprotocol::NodeConfig& nodeConfig)
        = 0;

    /**
     * Sets node config.
     *
     * @param nodeID node ID.
     * @param version version.
     * @param nodeConfig node config.
     * @return Error.
     */
    virtual Error SetNodeConfig(
        const String& nodeID, const String& version, const cloudprotocol::NodeConfig& nodeConfig)
        = 0;

    /**
     * Gets node config statuses.
     *
     * @param[out] statuses node config statuses.
     * @return Error.
     */
    virtual Error GetNodeConfigStatuses(Array<NodeConfigStatus>& statuses) = 0;
};

/**
 * Unit config interface.
 */
class UnitConfigItf : public NodeConfigPublisherItf, public NodeConfigStatusListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~UnitConfigItf() = default;

    /**
     * Gets unit config status.
     *
     * @param[out] status unit config status.
     * @return Error.
     */
    virtual Error GetStatus(cloudprotocol::UnitConfigStatus& status) = 0;

    /**
     * Checks unit config.
     *
     * @param config unit config.
     * @return Error.
     */
    virtual Error CheckUnitConfig(const cloudprotocol::UnitConfig& config) = 0;

    /**
     * Gets node config.
     *
     * @param nodeID node ID.
     * @param nodeType node type.
     * @param[out] config node config.
     * @return Error.
     */
    virtual Error GetNodeConfig(const String& nodeID, const String& nodeType, cloudprotocol::NodeConfig& config) = 0;

    /**
     * Gets current node config.
     *
     * @param[out] config current node config.
     * @return Error.
     */
    virtual Error GetCurrentNodeConfig(cloudprotocol::NodeConfig& config) = 0;
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
     * Creates unit config object from a JSON string.
     *
     * @param json JSON string.
     * @param[out] unitConfig unit config object.
     * @return Error.
     */
    virtual Error UnitConfigFromJSON(const String& json, cloudprotocol::UnitConfig& unitConfig) const = 0;

    /**
     * Creates unit config JSON string from a unit config object.
     *
     * @param unitConfig unit config object.
     * @param[out] json JSON string.
     * @return Error.
     */
    virtual Error UnitConfigToJSON(const cloudprotocol::UnitConfig& unitConfig, String& json) const = 0;
};

/**
 * Unit config implementation.
 */
class UnitConfig : public UnitConfigItf {
public:
    /**
     * Destructor.
     */
    ~UnitConfig() = default;

    /**
     * Initializes unit config.
     *
     * @param cfgFile configuration file.
     * @param nodeInfoProvider node info provider.
     * @param nodeConfigController node config controller.
     * @return Error.
     */
    Error Init(const String& cfgFile, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        NodeConfigControllerItf& nodeConfigController, JSONProviderItf& jsonProvider);

    /**
     * Starts unit config.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops unit config.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Gets unit config status.
     *
     * @param[out] status unit config status.
     * @return Error.
     */
    Error GetStatus(cloudprotocol::UnitConfigStatus& status) override;

    /**
     * Checks unit config.
     *
     * @param config unit config.
     * @return Error.
     */
    Error CheckUnitConfig(const cloudprotocol::UnitConfig& config) override;

    /**
     * Gets node config.
     *
     * @param nodeID node ID.
     * @param nodeType node type.
     * @param[out] config node config.
     * @return Error.
     */
    Error GetNodeConfig(const String& nodeID, const String& nodeType, cloudprotocol::NodeConfig& config) override;

    /**
     * Gets current node config.
     *
     * @param[out] config current node config.
     * @return Error.
     */
    Error GetCurrentNodeConfig(cloudprotocol::NodeConfig& config) override;

    /**
     * Updates unit config.
     *
     * @param unitConfig unit config.
     * @return Error.
     */
    Error UpdateUnitConfig(const cloudprotocol::UnitConfig& unitConfig);

    /**
     * Subscribes to current node config changes.
     *
     * @param listener listener to subscribe to.
     */
    Error SubscribeCurrentNodeConfigChange(NodeConfigChangeListenerItf* listener) override;

    /**
     * Unsubscribes from current node config changes.
     *
     * @param listener listener to unsubscribe from.
     */
    void UnsubscribeCurrentNodeConfigChange(NodeConfigChangeListenerItf* listener) override;

    /**
     * Called when node config status changes.
     *
     * @param status node config status.
     */
    void OnNodeConfigStatus(const NodeConfigStatus& status) override;

private:
    Error LoadConfig();
    Error CheckVersion(const String& version);
    Error FindNodeConfig(const String& nodeID, const String& nodeType, const cloudprotocol::UnitConfig& config,
        cloudprotocol::NodeConfig& nodeConfig);

    void UpdateNodeConfigListeners(const cloudprotocol::NodeConfig& nodeConfig);

    StaticString<cFilePathLen>             mCfgFile;
    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    NodeConfigControllerItf*               mNodeConfigController {};
    JSONProviderItf*                       mJSONProvider {};

    StaticString<cNodeIDLen>   mCurNodeID;
    StaticString<cNodeTypeLen> mCurNodeType;

    cloudprotocol::UnitConfig mUnitConfig;
    Error                     mUnitConfigError;

    StaticAllocator<sizeof(NodeInfo) + sizeof(cloudprotocol::UnitConfig) + sizeof(StaticString<cUnitConfigJSONLen>)
        + sizeof(StaticArray<NodeConfigStatus, cMaxNumNodes>)>
        mAllocator;

    StaticArray<NodeConfigStatus, cMaxNumNodes>             mNodeConfigStatuses;
    StaticArray<NodeConfigChangeListenerItf*, cMaxNumNodes> mNodeConfigListeners;

    Mutex mMutex;
};

} // namespace aos::cm::unitconfig

#endif // AOS_CORE_CM_UNITCONFIG_UNITCONFIG_HPP_
