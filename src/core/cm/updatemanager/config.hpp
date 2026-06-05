/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_CONFIG_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_CONFIG_HPP_

#include <core/common/tools/time.hpp>

namespace aos::cm::updatemanager {

/**
 * Update manager configuration.
 */
struct Config {
    Duration mUnitStatusSendTimeout;

    /**
     * Compares config.
     *
     * @param rhs config to compare.
     * @return bool.
     */
    bool operator==(const Config& rhs) const { return mUnitStatusSendTimeout == rhs.mUnitStatusSendTimeout; }

    /**
     * Compares config.
     *
     * @param other config to compare.
     * @return bool.
     */
    bool operator!=(const Config& rhs) const { return !operator==(rhs); }
};

} // namespace aos::cm::updatemanager

#endif
