/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NODECONFIG_CONFIG_HPP_
#define AOS_CORE_SM_NODECONFIG_CONFIG_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::nodeconfig {

/**
 * Node config configuration.
 */
struct Config {
    const String& mNodeConfigFile;
};

} // namespace aos::sm::nodeconfig

#endif
