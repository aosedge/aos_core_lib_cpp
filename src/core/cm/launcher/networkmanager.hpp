/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_NETWORKMANAGER_HPP_
#define AOS_CORE_CM_LAUNCHER_NETWORKMANAGER_HPP_

#include <core/cm/networkmanager/itf/networkmanager.hpp>
#include <core/common/tools/map.hpp>

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Network manager adapter.
 *
 * This class wraps NetworkManagerItf and provides additional functionality for storing
 * network service data.
 */
class NetworkManager {
public:
    /**
     * Initializes manager.
     *
     * @param netMgr network manager interface.
     */
    void Init(networkmanager::NetworkManagerItf& netMgr) { mNetMgr = &netMgr; }

    /**
     * Prepares network manager for balancing.
     */
    void PrepareForBalancing();

    /**
     * Stores network service data for the instance.
     *
     * @param instanceIdent instance identifier.
     * @param data network service data.
     * @return Error.
     */
    Error SetNetworkServiceData(const InstanceIdent& instanceIdent, const networkmanager::NetworkServiceData& data);

    /**
     * Prepares network parameters for the instance.
     *
     * @param instanceIdent instance identifier.
     * @param networkID network identifier (provider/owner).
     * @param nodeID node identifier.
     * @param[out] result prepared network parameters.
     * @return Error.
     */
    Error PrepareInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& networkID,
        const String& nodeID, bool onlyExposedPorts, Optional<InstanceNetworkParameters>& result);

    /**
     * Removes assigned network parameters for the specified instance.
     *
     * @param instanceIdent instance identifier.
     * @param nodeID node identifier.
     * @return Error.
     */
    Error RemoveInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& nodeID);

    /**
     * Restarts DNS server.
     *
     * @return Error.
     */
    Error RestartDNSServer();

    /**
     * Updates network configuration for the given providers and node.
     *
     * @param providers a list of provider ids.
     * @param nodeID node identifier.
     * @return Error.
     */
    Error UpdateProviderNetwork(const Array<StaticString<cIDLen>>& providers, const String& nodeID);

private:
    networkmanager::NetworkManagerItf*                                             mNetMgr {};
    StaticMap<InstanceIdent, networkmanager::NetworkServiceData, cMaxNumInstances> mNetworkServiceData;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
