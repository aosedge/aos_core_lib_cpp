/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_CONFIG_HPP_
#define AOS_CORE_CM_STORAGESTATE_CONFIG_HPP_

namespace aos::cm::storagestate {

/**
 * Storage state configuration.
 */
struct Config {
    StaticString<cFilePathLen> mStorageDir;
    StaticString<cFilePathLen> mStateDir;
};

} // namespace aos::cm::storagestate

#endif
