/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_INSTANCERUNNER_HPP_
#ifndef AOS_CORE_CM_LAUNCHER_ITF_INSTANCERUNNER_HPP_

#include <core/common/types/obsolete.hpp>

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
     * Updates instances on specified node.
     *
     * @param nodeID node ID.
     * @param stopInstances instance list to stop.
     * @param startInstances instance list to start.
     * @return Error.
     */
    virtual Error UpdateInstances(const String& nodeID, const Array<aos::InstanceInfo>& stopInstances,
        const Array<aos::InstanceInfo>& startInstances)
        = 0;
}

/** @}*/

} // namespace aos::cm::launcher

#endif // AOS_CORE_CM_LAUNCHER_ITF_STATUSLISTENER_HPP_
