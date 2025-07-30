/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_UNITCONFIG_HPP_
#define AOS_CM_UNITCONFIG_HPP_

#include <string>
#include <vector>

#include <aos/cm/nodeinfoprovider.hpp>
#include <aos/common/types.hpp>

namespace aos::cloudprotocol {
// Stubs
struct NodeConfig { };

struct NodeConfigStatus { };

struct UnitConfigStatus { };

struct UnitConfig { };

struct NodeConfig { };

} // namespace aos::cloudprotocol

namespace aos::cm::unitconfig {

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
    virtual void SubscribeCurrentNodeConfigChange(NodeConfigChangeListenerItf* listener) = 0;

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
    virtual void OnNodeConfigStatus(const cloudprotocol::NodeConfigStatus& status) = 0;
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
        const std::string& nodeID, const std::string& version, const cloudprotocol::NodeConfig& nodeConfig)
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
        const std::string& nodeID, const std::string& version, const cloudprotocol::NodeConfig& nodeConfig)
        = 0;

    /**
     * Gets node config statuses.
     *
     * @param[out] statuses node config statuses.
     * @return Error.
     */
    virtual Error GetNodeConfigStatuses(std::vector<cloudprotocol::NodeConfigStatus>& statuses) = 0;
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
    virtual Error GetNodeConfig(
        const std::string& nodeID, const std::string& nodeType, cloudprotocol::NodeConfig& config)
        = 0;

    /**
     * Gets current node config.
     *
     * @param[out] config current node config.
     * @return Error.
     */
    virtual Error GetCurrentNodeConfig(cloudprotocol::NodeConfig& config) = 0;
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
        NodeConfigControllerItf& nodeConfigController);

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
    Error GetNodeConfig(
        const std::string& nodeID, const std::string& nodeType, cloudprotocol::NodeConfig& config) override;

    /**
     * Gets current node config.
     *
     * @param[out] config current node config.
     * @return Error.
     */
    Error GetCurrentNodeConfig(cloudprotocol::NodeConfig& config) override;

    /**
     * Subscribes to current node config changes.
     *
     * @param listener listener to subscribe to.
     */
    void SubscribeCurrentNodeConfigChange(NodeConfigChangeListenerItf* listener) override;

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
    void OnNodeConfigStatus(const cloudprotocol::NodeConfigStatus& status) override;
};

} // namespace aos::cm::unitconfig

#endif // AOS_CM_UNITCONFIG_HPP_
