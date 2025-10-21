/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_NODEENVVARHANDLER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_NODEENVVARHANDLER_HPP_

#include <core/common/types/envvars.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Node environment variable handler interface.
 */
class NodeEnvVarHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeEnvVarHandlerItf() = default;

    /**
     * Overrides environment variables.
     *
     * @param nodeID node ID.
     * @param envVars environment variables.
     * @return Error.
     */
    virtual Error OverrideEnvVars(const String& nodeID, const OverrideEnvVarsRequest& envVars) = 0;
};

} // namespace aos::cm::launcher

#endif
