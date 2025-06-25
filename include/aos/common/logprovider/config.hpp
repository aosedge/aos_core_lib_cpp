/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LOGPROVIDER_CONFIG_HPP_
#define AOS_LOGPROVIDER_CONFIG_HPP_

#include <cstdint>

namespace aos::logprovider {

/*
 * Logging configuration.
 */
struct Config {
    uint64_t mMaxPartSize {};
    uint64_t mMaxPartCount {};
};

} // namespace aos::logprovider

#endif
