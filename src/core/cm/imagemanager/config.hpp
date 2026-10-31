/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_CONFIG_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_CONFIG_HPP_

#include <core/common/imagemanager/config.hpp>

namespace aos::cm::imagemanager {

/**
 * CM image manager config.
 */
struct Config : public common::imagemanager::Config {
    StaticString<cFilePathLen> mDownloadPath;
};

} // namespace aos::cm::imagemanager

#endif
