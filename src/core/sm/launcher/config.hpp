
/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_CONFIG_HPP_
#define AOS_CORE_SM_LAUNCHER_CONFIG_HPP_

#include <core/common/types/obsolete.hpp>
#include <core/sm/config.hpp>

namespace aos::sm::launcher {
/**
 * Max num of host directory binds.
 */
auto constexpr cMaxNumHostBinds = AOS_CONFIG_LAUNCHER_MAX_NUM_HOST_BINDS;

/**
 * Launcher configuration.
 */
struct Config {
    StaticString<cFilePathLen>                                mWorkDir;
    StaticString<cFilePathLen>                                mStorageDir;
    StaticString<cFilePathLen>                                mStateDir;
    StaticArray<StaticString<cFilePathLen>, cMaxNumHostBinds> mHostBinds;
    StaticArray<Host, cMaxNumHosts>                           mHosts;
    Duration                                                  mRemoveOutdatedPeriod = Time::cHours;
};

} // namespace aos::sm::launcher

#endif
