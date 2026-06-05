/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CONSTS_HPP_
#define AOS_CORE_COMMON_CONSTS_HPP_

#include <cstddef>

#include "config.hpp"

namespace aos {

/*
 * URL len.
 */
constexpr auto cURLLen = AOS_CONFIG_URL_LEN;

/*
 * Maximum number of URLs.
 */
constexpr auto cMaxNumURLs = AOS_CONFIG_MAX_NUM_URLS;

/*
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_FILE_PATH_LEN;

/**
 * Max number of concurrent items.
 */
constexpr auto cMaxNumConcurrentItems = AOS_CONFIG_MAX_NUM_CONCURRENT_ITEMS;

/*
 * Kilobyte size in bytes.
 */
constexpr size_t cKilobyte = 1024;

} // namespace aos

#endif
