/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_RESOURCEMANAGER_CONFIG_HPP_
#define AOS_CORE_SM_RESOURCEMANAGER_CONFIG_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::resourcemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Resource manager config.
 */
struct Config {
    StaticString<cFilePathLen> mResourceInfoFilePath;
};

/** @}*/

} // namespace aos::sm::resourcemanager

#endif
