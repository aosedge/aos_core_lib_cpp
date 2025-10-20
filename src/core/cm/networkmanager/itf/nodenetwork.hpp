/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_NETWORKMANAGER_ITF_NODENETWORK_HPP_
#define AOS_CORE_CM_NETWORKMANAGER_ITF_NODENETWORK_HPP_

#include <core/common/types/network.hpp>

namespace aos::cm::networkmanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Node network interface.
 */
class NodeNetworkItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeNetworkItf() = default;

    /**
     * Sets node network parameters.
     *
     * @param nodeID node ID.
     * @param networkParameters network parameters.
     * @return Error.
     */
    virtual Error UpdateNetworks(const String& nodeID, const Array<UpdateNetworkParameters>& networkParameters) = 0;
};

/** @}*/

} // namespace aos::cm::networkmanager

#endif
