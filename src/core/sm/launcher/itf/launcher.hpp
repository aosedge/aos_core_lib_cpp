/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITF_LAUNCHER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_LAUNCHER_HPP_

#include <core/common/types/common.hpp>
#include <core/common/types/instance.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Launcher interface.
 */
class LauncherItf {
public:
    /**
     * Destructor.
     */
    virtual ~LauncherItf() = default;

    /**
     * Runs instances.
     *
     * @param instances instances to run.
     * @return Error.
     */
    virtual Error RunInstances(const Array<InstanceInfo>& instances) = 0;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
