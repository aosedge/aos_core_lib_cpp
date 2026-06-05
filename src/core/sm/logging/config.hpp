/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LOGGING_CONFIG_HPP_
#define AOS_CORE_SM_LOGGING_CONFIG_HPP_

#include <cstdint>

namespace aos::sm::logging {

/*
 * Logging configuration.
 */
struct Config {
    size_t mMaxPartSize {};
    size_t mMaxPartCount {};
};

} // namespace aos::sm::logging

#endif
