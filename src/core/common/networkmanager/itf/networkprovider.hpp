/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_NETWORKMANAGER_ITF_NETWORKPROVIDER_HPP_
#define AOS_CORE_COMMON_NETWORKMANAGER_ITF_NETWORKPROVIDER_HPP_

#include <core/common/types/network.hpp>

namespace aos::networkmanager {

/** @addtogroup common Common
 *  @{
 */

/**
 * Network provider interface (SM -> CM network service).
 */
class NetworkProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~NetworkProviderItf() = default;

    /**
     * Gets node network parameters from CM.
     *
     * @param networkID network identifier.
     * @param nodeID node identifier.
     * @param[out] result node network parameters.
     * @return Error.
     */
    virtual Error GetNodeNetworkParams(const String& networkID, const String& nodeID, NetworkParams& result) = 0;

    /**
     * Allocates instance network from CM.
     *
     * @param instance instance identifier.
     * @param networkID network identifier.
     * @param nodeID node identifier.
     * @param serviceData network service data.
     * @param[out] result instance network parameters.
     * @return Error.
     */
    virtual Error AllocateInstanceNetwork(const InstanceIdent& instance, const String& networkID, const String& nodeID,
        const UpdateItemNetworkParams& serviceData, InstanceNetworkAllocation& result)
        = 0;

    /**
     * Releases instance network on CM.
     *
     * @param instance instance identifier.
     * @param nodeID node identifier.
     * @return Error.
     */
    virtual Error ReleaseInstanceNetwork(const InstanceIdent& instance, const String& nodeID) = 0;

    /**
     * Releases node network on CM.
     *
     * @param networkID network identifier.
     * @param nodeID node identifier.
     * @return Error.
     */
    virtual Error ReleaseNodeNetwork(const String& networkID, const String& nodeID) = 0;
};

/** @}*/

} // namespace aos::networkmanager

#endif
