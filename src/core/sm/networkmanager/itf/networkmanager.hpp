/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_NETWORKMANAGER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_NETWORKMANAGER_HPP_

#include <core/common/networkmanager/itf/pendingupdatehandler.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/types/network.hpp>
#include <core/sm/config.hpp>
#include <core/sm/smclient/itf/connection.hpp>

#include "instancetrafficprovider.hpp"
#include "interfacefactory.hpp"
#include "interfacemanager.hpp"
#include "namespacemanager.hpp"
#include "storage.hpp"
#include "systemtrafficprovider.hpp"
#include "trafficmonitor.hpp"
#include "types.hpp"

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Network manager interface.
 */
class NetworkManagerItf : public SystemTrafficProviderItf,
                          public InstanceTrafficProviderItf,
                          public aos::networkmanager::PendingUpdateHandlerItf,
                          public aos::sm::smclient::ConnectListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NetworkManagerItf() = default;

    /**
     * Returns instance's network namespace path.
     *
     * @param instanceID instance id.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetNetnsPath(const String& instanceID) const = 0;

    /**
     * Sets the traffic period.
     *
     * @param period traffic period.
     * @return Error.
     */
    virtual Error SetTrafficPeriod(TrafficPeriod period) = 0;

    /**
     * Creates instance network: requests node network from CM, allocates IP, stores in DB.
     * Returns eAlreadyExist if instance network already created.
     * Does NOT create bridge/VLAN, namespace, or attach the instance to the bridge.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @param networkConfig instance network config.
     * @return Error.
     */
    virtual Error CreateInstanceNetwork(
        const String& instanceID, const String& networkID, const InstanceNetworkConfig& networkConfig)
        = 0;

    /**
     * Starts instance network: creates bridge/VLAN if needed, namespace,
     * attaches via BridgeNetworkItf and installs firewall/bandwidth/DNS,
     * monitoring, hosts/resolv.conf. Reads network params from DB.
     * Does NOT call CM.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @return Error.
     */
    virtual Error StartInstanceNetwork(const String& instanceID, const String& networkID) = 0;

    /**
     * Returns the resolver IPs for the instance. Only the addresses are
     * returned; the caller writes resolv.conf itself and must prefix each entry
     * with "nameserver " (e.g. "nameserver 10.0.0.1"). Call after
     * StartInstanceNetwork.
     *
     * @param instanceID instance ID.
     * @param[out] servers resolver IP addresses (no "nameserver" prefix).
     * @return Error.
     */
    virtual Error GetResolvServers(const String& instanceID, Array<StaticString<cIPLen>>& servers) const = 0;

    /**
     * Returns the host entries (IP + hostname) for the instance. The caller
     * writes the hosts file itself, formatting each entry as "<ip>\t<hostname>".
     * Call after StartInstanceNetwork.
     *
     * @param instanceID instance ID.
     * @param[out] hosts host entries.
     * @return Error.
     */
    virtual Error GetHosts(const String& instanceID, Array<Host>& hosts) const = 0;

    /**
     * Stops instance network: tears down DNS/bandwidth/firewall/bridge for the
     * instance, deletes its namespace and monitoring.
     * If last running instance on network — clears bridge/VLAN.
     * Does NOT remove from DB, does NOT call CM.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @return Error.
     */
    virtual Error StopInstanceNetwork(const String& instanceID, const String& networkID) = 0;

    /**
     * Releases instance network: removes from DB, calls CM ReleaseInstanceNetwork.
     * If last instance on network — removes node network info from DB, releases on CM.
     * Requires Stop to be called first.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @return Error.
     */
    virtual Error ReleaseInstanceNetwork(const String& instanceID, const String& networkID) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
