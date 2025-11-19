/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_ITF_INSTANCEMONITORINGPROVIDER_HPP_
#define AOS_CORE_COMMON_MONITORING_ITF_INSTANCEMONITORINGPROVIDER_HPP_

#include "monitoringdata.hpp"

namespace aos::monitoring {

/**
 * Instance monitoring provider interface.
 */
class InstanceMonitoringProviderItf {
public:
    /**
     * Destructor
     */
    virtual ~InstanceMonitoringProviderItf() = default;

    /**
     * Returns instance monitoring data.
     *
     * @param instanceIdent instance ident.
     * @param[out] monitoringData instance monitoring data.
     * @return Error.
     */
    virtual Error GetInstanceMonitoringData(const InstanceIdent& instanceIdent, InstanceMonitoringData& monitoringData)
        = 0;
};

} // namespace aos::monitoring

#endif
