/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NETWORKMANAGER_HPP_
#define AOS_NETWORKMANAGER_HPP_

#include "aos/common/types.hpp"

namespace aos {
namespace sm {
namespace networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Network manager interface.
 */
class NetworkManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NetworkManagerItf() = default;

    /**
     * Returns instance's network namespace path.
     *
     * @param instanceID instance id.
     * @param[out] netnsPath instance's network namespace path.
     * @return Error.
     */
    virtual Error GetNetnsPath(const String& instanceID, String& netnsPath) const = 0;

    /**
     * Updates networks.
     *
     * @param networks network parameters.
     * @return Error.
     */
    virtual Error UpdateNetworks(const Array<NetworkParameters>& networks) = 0;

    /**
     * Adds instance to network.
     *
     * @param instanceID instance id.
     * @param networkID network id.
     * @param network network parameters.
     * @return Error.
     */
    virtual Error AddInstanceToNetwork(
        const String& instanceID, const String& networkID, const NetworkParameters& network)
        = 0;

    /**
     * Removes instance from network.
     *
     * @param instanceID instance id.
     * @param networkID network id.
     * @return Error.
     */
    virtual Error RemoveInstanceFromNetwork(const String& instanceID, const String& networkID) = 0;

    /**
     * Returns instance's IP address.
     *
     * @param instanceID instance id.
     * @param networkID network id.
     * @param[out] ip instance's IP address.
     * @return Error.
     */
    virtual Error GetInstanceIP(const String& instanceID, const String& networkID, String& ip) const = 0;

    /**
     * Returns instance's traffic.
     *
     * @param instanceID instance id.
     * @param[out] inputTraffic instance's input traffic.
     * @param[out] outputTraffic instance's output traffic.
     * @return Error.
     */
    virtual Error GetInstanceTraffic(const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic) const
        = 0;

    /**
     * Sets the traffic period.
     *
     * @param period traffic period.
     * @return Error
     */
    virtual Error SetTrafficPeriod(uint32_t period) = 0;
};

/** @}*/

} // namespace networkmanager
} // namespace sm
} // namespace aos

#endif
