/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_NETWORKMANAGER_ITF_NETWORKMANAGER_HPP_
#define AOS_CORE_CM_NETWORKMANAGER_ITF_NETWORKMANAGER_HPP_

#include <core/cm/config.hpp>
#include <core/common/types/network.hpp>

namespace aos::cm::networkmanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Represents network configuration parameters for service.
 */
struct NetworkServiceData {
    /**
     * List of hostnames.
     */
    StaticArray<StaticString<cHostNameLen>, cMaxNumHosts> mHosts;

    /**
     * List of allowed service connections.
     */
    StaticArray<StaticString<cConnectionNameLen>, cMaxNumConnections> mAllowedConnections;

    /**
     * List of ports exposed by service instance.
     */
    StaticArray<StaticString<cExposedPortLen>, cMaxNumExposedPorts> mExposedPorts;
};

/**
 * Interface for managing network parameters for service instances.
 */
class NetworkManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NetworkManagerItf() = default;

    /**
     * Prepares and assigns network parameters for a service instance.
     *
     * @param instanceIdent instance identifier.
     * @param networkID identifier of the target network.
     * @param nodeID node identifier.
     * @param networkData service network configuration data.
     * @param[out] result result network parameters.
     * @return Error.
     */
    virtual Error PrepareInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& networkID,
        const String& nodeID, const NetworkServiceData& networkData, NetworkParameters& result)
        = 0;

    /**
     * Removes assigned network parameters for the specified service instance.
     *
     * @param instanceIdent instance identifier.
     * @param nodeID node identifier.
     * @return Error.
     */
    virtual Error RemoveInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& nodeID) = 0;

    /**
     * Restarts DNS server.
     *
     * @return Error.
     */
    virtual Error RestartDNSServer() = 0;

    /**
     * Returns all service instances registered in network manager.
     *
     * @param[out] instances list of instances.
     * @return Error.
     */
    virtual Error GetInstances(Array<InstanceIdent>& instances) const = 0;

    /**
     * Updates network configuration for the given providers and node.
     *
     * @param providers a list of provider ids.
     * @param nodeID node identifier.
     * @return Error.
     */
    virtual Error UpdateProviderNetwork(const Array<StaticString<cIDLen>>& providers, const String& nodeID) = 0;
};

/** @}*/

} // namespace aos::cm::networkmanager

#endif
