/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_INSTANCETRAFFICPROVIDER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_INSTANCETRAFFICPROVIDER_HPP_

#include <core/common/tools/string.hpp>

namespace aos::sm::networkmanager {

/**
 * Instance traffic provider interface.
 */
class InstanceTrafficProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceTrafficProviderItf() = default;

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
};

} // namespace aos::sm::networkmanager

#endif
