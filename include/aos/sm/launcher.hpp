/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LAUNCHER_HPP_
#define LAUNCHER_HPP_

#include "aos/common/error.hpp"

namespace aos {
namespace sm {
namespace launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Launches service instances.
 */
class Launcher {
public:
    /**
     * Runs instances.
     *
     * @return Error
     */
    Error RunInstances();
};

/** @}*/

} // namespace launcher
} // namespace sm
} // namespace aos

#endif
