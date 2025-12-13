/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NODECONFIG_NODECONFIG_HPP_
#define AOS_CORE_SM_NODECONFIG_NODECONFIG_HPP_

#include <core/common/nodeconfig/itf/jsonprovider.hpp>
#include <core/common/nodeconfig/itf/nodeconfigprovider.hpp>
#include <core/common/tools/allocator.hpp>
#include <core/common/tools/thread.hpp>

#include "config.hpp"
#include "itf/nodeconfighandler.hpp"

namespace aos::sm::nodeconfig {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Node config implementation.
 */
class NodeConfig : public NodeConfigHandlerItf, public aos::nodeconfig::NodeConfigProviderItf {
public:
    /**
     * Initializes node config.
     *
     * @param config node config configuration.
     * @param jsonProvider JSON provider.
     * @return Error.
     */
    Error Init(const Config& config, aos::nodeconfig::JSONProviderItf& jsonProvider);

    /**
     * Checks node config.
     *
     * @param config node config.
     * @return Error.
     */
    Error CheckNodeConfig(const aos::NodeConfig& config) override;

    /**
     * Updates node config.
     *
     * @param config node config.
     * @return Error.
     */
    Error UpdateNodeConfig(const aos::NodeConfig& config) override;

    /**
     * Returns node config status.
     *
     * @param[out] status node config status.
     * @return Error.
     */
    Error GetNodeConfigStatus(NodeConfigStatus& status) override;

    /**
     * Returns current node config.
     *
     * @param[out] nodeConfig param to store node config.
     * @return Error.
     */
    Error GetNodeConfig(aos::NodeConfig& nodeConfig) override;

    /**
     * Subscribes listener for node config changes.
     *
     * @param listener listener to subscribe.
     * @return Error.
     */
    Error SubscribeListener(aos::nodeconfig::NodeConfigListenerItf& listener) override;

    /**
     * Unsubscribes listener from node config changes.
     *
     * @param listener listener to unsubscribe.
     * @return Error.
     */
    Error UnsubscribeListener(aos::nodeconfig::NodeConfigListenerItf& listener) override;

private:
    static constexpr auto cMaxNumListeners = 4;
    static constexpr auto cAllocatorSize   = sizeof(StaticString<aos::nodeconfig::cNodeConfigJSONLen>);

    Error LoadConfig();
    Error CheckVersion(const String& version);

    aos::nodeconfig::JSONProviderItf* mJSONProvider {};
    StaticString<cFilePathLen>        mNodeConfigFile;
    aos::NodeConfig                   mNodeConfig;
    NodeConfigState                   mNodeConfigState {UnitConfigStateEnum::eAbsent};
    Error                             mNodeConfigError;
    mutable Mutex                     mMutex;
    StaticArray<aos::nodeconfig::NodeConfigListenerItf*, cMaxNumListeners> mListeners;
    StaticAllocator<cAllocatorSize>                                        mAllocator;
};

/** @}*/

} // namespace aos::sm::nodeconfig

#endif
