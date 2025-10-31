/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UNITCONFIG_UNITCONFIG_HPP_
#define AOS_CORE_CM_UNITCONFIG_UNITCONFIG_HPP_

#include <core/cm/config.hpp>
#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>
#include <core/cm/unitconfig/config.hpp>
#include <core/cm/unitconfig/itf/jsonprovider.hpp>
#include <core/cm/unitconfig/itf/nodeconfighandler.hpp>
#include <core/cm/unitconfig/itf/nodeconfigprovider.hpp>
#include <core/cm/unitconfig/itf/unitconfig.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/common.hpp>
#include <core/common/types/unitconfig.hpp>

namespace aos::cm::unitconfig {

/**
 * Unit config implementation.
 */
class UnitConfig : public UnitConfigItf, public NodeConfigProviderItf, public nodeinfoprovider::NodeInfoListenerItf {
public:
    /**
     * Destructor.
     */
    ~UnitConfig() = default;

    /**
     * Initializes unit config.
     *
     * @param config configuration.
     * @param nodeInfoProvider node info provider.
     * @param nodeConfigHandler node config handler.
     * @param jsonProvider JSON provider.
     * @return Error.
     */
    Error Init(const Config& config, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        NodeConfigHandlerItf& nodeConfigHandler, JSONProviderItf& jsonProvider);

    /**
     * Gets unit config status.
     *
     * @param[out] status unit config status.
     * @return Error.
     */
    Error GetUnitConfigStatus(UnitConfigStatus& status) override;

    /**
     * Checks unit config.
     *
     * @param config unit config.
     * @return Error.
     */
    Error CheckUnitConfig(const aos::UnitConfig& config) override;

    /**
     * Updates unit config.
     *
     * @param config unit config.
     * @return Error.
     */
    Error UpdateUnitConfig(const aos::UnitConfig& config) override;

    /**
     * Gets node config.
     *
     * @param nodeID node ID.
     * @param nodeType node type.
     * @param[out] config node config.
     * @return Error.
     */
    Error GetNodeConfig(const String& nodeID, const String& nodeType, NodeConfig& config) override;

    /**
     * Called when node info changes.
     *
     * @param info unit node info.
     */
    void OnNodeInfoChanged(const UnitNodeInfo& info) override;

private:
    static constexpr auto cUnitConfigJSONLen = AOS_CONFIG_CM_UNITCONFIG_JSON_LEN;

    Error LoadConfig();
    Error CheckVersion(const String& version);
    Error FindNodeConfig(
        const String& nodeID, const String& nodeType, const aos::UnitConfig& config, NodeConfig& nodeConfig);

    StaticString<cFilePathLen>             mUnitConfigFile;
    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    NodeConfigHandlerItf*                  mNodeConfigHandler {};
    JSONProviderItf*                       mJSONProvider {};

    aos::UnitConfig mUnitConfig;
    Error           mUnitConfigError;
    UnitConfigState mUnitConfigState {UnitConfigStateEnum::eAbsent};

    StaticAllocator<sizeof(NodeInfo) + sizeof(StaticString<cUnitConfigJSONLen>) + sizeof(NodeConfig)> mAllocator;

    StaticArray<NodeConfigStatus, cMaxNumNodes> mNodeConfigStatuses;

    Mutex mMutex;
};

} // namespace aos::cm::unitconfig

#endif // AOS_CORE_CM_UNITCONFIG_UNITCONFIG_HPP_
