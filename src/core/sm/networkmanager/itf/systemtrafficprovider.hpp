/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_SYSTEMTRAFFICPROVIDER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_SYSTEMTRAFFICPROVIDER_HPP_

#include <core/common/tools/error.hpp>

namespace aos::sm::networkmanager {

/**
 * System traffic provider interface.
 */
class SystemTrafficProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~SystemTrafficProviderItf() = default;

    /**
     * Gets system traffic.
     *
     * @param[out] inputTraffic system input traffic.
     * @param[out] outputTraffic system output traffic.
     * @return Error.
     */
    virtual Error GetSystemTraffic(uint64_t& inputTraffic, uint64_t& outputTraffic) const = 0;
};

} // namespace aos::sm::networkmanager

#endif
