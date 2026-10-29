/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CURRENTNODE_HWINFOMODULES_FILEHWINFOPROVIDER_CONFIG_HPP_
#define AOS_CORE_IAM_CURRENTNODE_HWINFOMODULES_FILEHWINFOPROVIDER_CONFIG_HPP_

#include <core/common/consts.hpp>
#include <core/common/tools/string.hpp>

namespace aos::iam::currentnode {

/**
 * File hardware info provider configuration.
 */
struct FileHWInfoProviderConfig {
    /**
     * Hardware ID file path.
     */
    StaticString<cFilePathLen> mHWIDPath;
};

} // namespace aos::iam::currentnode

#endif
