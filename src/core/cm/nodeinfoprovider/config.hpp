/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_NODEINFOPROVIDER_CONFIG_HPP_
#define AOS_CORE_CM_NODEINFOPROVIDER_CONFIG_HPP_

#include <core/common/tools/time.hpp>

namespace aos::cm::nodeinfoprovider {

/*
 * Configuration.
 */
struct Config {
    Duration mSMConnectionTimeout;
};

} // namespace aos::cm::nodeinfoprovider

#endif
