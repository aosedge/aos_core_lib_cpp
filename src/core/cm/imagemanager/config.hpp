/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_CONFIG_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_CONFIG_HPP_

#include <core/common/consts.hpp>
#include <core/common/tools/string.hpp>

namespace aos::cm::imagemanager {

/**
 * Image manager configuration.
 */
struct Config {
    StaticString<cFilePathLen> mInstallPath;
    StaticString<cFilePathLen> mDownloadPath;
    Duration                   mUpdateItemTTL;
    Duration                   mRemoveOutdatedPeriod;

    /**
     * Compares config.
     *
     * @param other config to compare.
     * @return bool.
     */
    friend bool operator==(const Config& lhs, const Config& other)
    {
        return lhs.mInstallPath == other.mInstallPath && lhs.mDownloadPath == other.mDownloadPath
            && lhs.mUpdateItemTTL == other.mUpdateItemTTL && lhs.mRemoveOutdatedPeriod == other.mRemoveOutdatedPeriod;
    };

    /**
     * Compares config.
     *
     * @param other config to compare.
     * @return bool.
     */
    friend bool operator!=(const Config& lhs, const Config& other) { return !(lhs == other); };
};

} // namespace aos::cm::imagemanager

#endif
