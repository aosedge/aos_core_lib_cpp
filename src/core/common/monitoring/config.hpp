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
    Duration mPollPeriod    = AOS_CONFIG_MONITORING_POLL_PERIOD_SEC * Time::cSeconds;
    Duration mAverageWindow = AOS_CONFIG_MONITORING_AVERAGE_WINDOW_SEC * Time::cSeconds;
};

} // namespace aos::monitoring

#endif
