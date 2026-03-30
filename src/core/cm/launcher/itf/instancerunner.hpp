/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_INSTANCERUNNER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_INSTANCERUNNER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Instance runner interface.
 */
class InstanceRunnerItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceRunnerItf() = default;

    /**
     * Runs instances on specified node.
     *
     * @param nodeID node ID.
     * @param instances instance list to run.
     * @return Error.
     */
    virtual Error RunInstances(const String& nodeID, const Array<aos::InstanceInfo>& instances) = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
