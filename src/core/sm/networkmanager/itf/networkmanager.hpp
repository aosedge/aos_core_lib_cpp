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
                          public aos::networkmanager::PendingUpdateHandlerItf {
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
     * Does NOT create bridge/VLAN, namespace, or CNI.
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
     * Starts instance network: creates bridge/VLAN if needed, namespace, CNI,
     * monitoring, hosts/resolv.conf. Reads network params from DB.
     * Does NOT call CM.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @param runtimeParams runtime parameters (file paths).
     * @return Error.
     */
    virtual Error StartInstanceNetwork(
        const String& instanceID, const String& networkID, const InstanceNetworkRuntimeParams& runtimeParams)
        = 0;

    /**
     * Stops instance network: removes CNI, namespace, monitoring.
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
