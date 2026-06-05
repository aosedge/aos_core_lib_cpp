/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_ALERTS_CONFIG_HPP_
#define AOS_CORE_CM_ALERTS_CONFIG_HPP_

#include <core/common/tools/time.hpp>

namespace aos::cm::alerts {

/*
 * Configuration.
 */
struct Config {
    Duration mSendPeriod;
};

} // namespace aos::cm::alerts

#endif
