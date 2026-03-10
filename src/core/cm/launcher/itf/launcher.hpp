/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_LAUNCHER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_LAUNCHER_HPP_

#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/types/instance.hpp>

#include <core/cm/launcher/itf/types.hpp>

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Instance launcher interface.
 */
class LauncherItf : public instancestatusprovider::ProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~LauncherItf() = default;

    /**
     * Schedules and run instances.
     *
     * @param instances instances info.
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    virtual Error RunInstances(const Array<RunInstanceRequest>& instances, Array<InstanceStatus>& statuses) = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
