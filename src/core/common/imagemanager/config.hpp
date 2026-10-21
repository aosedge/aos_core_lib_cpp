/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_CONFIG_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_CONFIG_HPP_

#include <core/common/tools/fs.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/time.hpp>

namespace aos::common::imagemanager {

/**
 * Image manager config.
 */
struct Config {
    StaticString<cFilePathLen> mImagePath;
    size_t                     mPartLimit {};
    Duration                   mItemCacheTTL {};
    Duration                   mRemoveOutdatedPeriod {};
};

} // namespace aos::common::imagemanager

#endif
