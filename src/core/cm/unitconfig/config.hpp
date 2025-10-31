/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UNITCONFIG_CONFIG_HPP_
#define AOS_CORE_CM_UNITCONFIG_CONFIG_HPP_

#include <core/common/types/common.hpp>

namespace aos::cm::unitconfig {

/**
 * Unit config configuration.
 */
struct Config {
    const String& mUnitConfigFile;
};

} // namespace aos::cm::unitconfig

#endif // AOS_CORE_CM_UNITCONFIG_CONFIG_HPP_
