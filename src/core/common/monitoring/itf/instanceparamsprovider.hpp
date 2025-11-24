/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_ITF_INSTANCEPARAMSPROVIDER_HPP_
#define AOS_CORE_COMMON_MONITORING_ITF_INSTANCEPARAMSPROVIDER_HPP_

#include <core/common/types/common.hpp>
#include <core/common/types/monitoring.hpp>

namespace aos::monitoring {

/**
 * Instance info provider interface.
 */
class InstanceParamsProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceParamsProviderItf() = default;

    /**
     * Gets instance monitoring parameters.
     *
     * @param instanceIdent instance ident.
     * @param[out] params instance monitoring parameters.
     * @return Error eNotSupported if instance monitoring is not supported.
     */
    virtual Error GetInstanceMonitoringParams(
        const InstanceIdent& instanceIdent, Optional<InstanceMonitoringParams>& params) const
        = 0;
};

} // namespace aos::monitoring

#endif
