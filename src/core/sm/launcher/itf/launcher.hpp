/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITF_LAUNCHER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_LAUNCHER_HPP_

#include <core/common/types/common.hpp>

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
     * Update running instances.
     *
     * @param stopInstances instances to stop.
     * @param startInstances instances to start.
     * @return Error.
     */
    virtual Error UpdateInstances(const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances)
        = 0;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
