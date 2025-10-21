/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_ENVVARHANDLER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_ENVVARHANDLER_HPP_

#include <core/common/types/envvars.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Environment variable handler interface.
 */
class EnvVarHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~EnvVarHandlerItf() = default;

    /**
     * Overrides environment variables.
     *
     * @param envVars environment variables.
     * @return Error.
     */
    virtual Error OverrideEnvVars(const OverrideEnvVarsRequest& envVars) = 0;
};

} // namespace aos::cm::launcher

#endif
