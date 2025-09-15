/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_CONFIG_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_CONFIG_HPP_

#include <core/common/types/types.hpp>

namespace aos::cm::imagemanager {

/**
 * Image manager configuration.
 */
struct Config {
    StaticString<cFilePathLen> mInstallPath;
    Duration                   mUpdateItemTTL;

    /**
     * Compares config.
     *
     * @param other config to compare.
     * @return bool.
     */
    bool operator==(const Config& other) const
    {
        return mInstallPath == other.mInstallPath && mUpdateItemTTL == other.mUpdateItemTTL;
    }

    /**
     * Compares config.
     *
     * @param other config to compare.
     * @return bool.
     */
    bool operator!=(const Config& other) const { return !operator==(other); }
};

} // namespace aos::cm::imagemanager

#endif
