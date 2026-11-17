/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_LAUNCHPOOL_HPP_
#define AOS_CORE_SM_LAUNCHER_LAUNCHPOOL_HPP_

#include <core/common/tools/thread.hpp>
#include <core/common/types/common.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Max size of the launch pool task.
 */
static constexpr auto cLaunchTaskSize = 512;

/**
 * Thread pool which runs the launch tasks: one task per instance or update item.
 */
using LaunchPool = ThreadPool<cMaxNumConcurrentItems, cMaxNumInstances, cLaunchTaskSize>;

/** @}*/

} // namespace aos::sm::launcher

#endif
