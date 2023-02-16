/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/launcher.hpp"
#include "log.hpp"

namespace aos {
namespace sm {
namespace launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::RunInstances()
{
    LOG_DBG() << "Run instances";

    return ErrorEnum::eNone;
}

} // namespace launcher
} // namespace sm
} // namespace aos
