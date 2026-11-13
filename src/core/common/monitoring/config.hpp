/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_CONFIG_HPP_
#define AOS_CORE_COMMON_MONITORING_CONFIG_HPP_

#include <core/common/tools/time.hpp>

namespace aos::monitoring {

/**
 * Monitoring config.
 */
struct Config {
    Duration mPollPeriod;
    Duration mAverageWindow;
};

} // namespace aos::monitoring

#endif
