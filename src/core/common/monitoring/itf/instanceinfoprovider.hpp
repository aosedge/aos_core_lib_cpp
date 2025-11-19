/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_ITF_INSTANCEINFOPROVIDER_HPP_
#define AOS_CORE_COMMON_MONITORING_ITF_INSTANCEINFOPROVIDER_HPP_

#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>

#include "instancemonitoringprovider.hpp"
#include "instanceparamsprovider.hpp"

namespace aos::monitoring {

/**
 * Instance info provider interface.
 */
class InstanceInfoProviderItf : public instancestatusprovider::ProviderItf,
                                public InstanceParamsProviderItf,
                                public InstanceMonitoringProviderItf { };

} // namespace aos::monitoring

#endif
