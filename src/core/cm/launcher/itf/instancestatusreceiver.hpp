/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_INSTANCESTATUSRECEIVER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_INSTANCESTATUSRECEIVER_HPP_

#include <core/common/types/envvars.hpp>
#include <core/common/types/instance.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Receives instance status updates.
 */
class InstanceStatusReceiverItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceStatusReceiverItf() = default;

    /**
     * Receives instance status.
     *
     * @param status instance status.
     * @return Error.
     */
    virtual Error OnInstanceStatusReceived(const InstanceStatus& status) = 0;

    /**
     * Receives node instances statuses.
     *
     * @param nodeID node ID.
     * @param statuses instance statuses.
     * @return Error.
     */
    virtual Error OnNodeInstancesStatusesReceived(const String& nodeID, const Array<InstanceStatus>& statuses) = 0;

    /**
     * Receives env vars statuses.
     *
     * @param nodeID node ID.
     * @param statuses env vars statuses.
     * @return Error.
     */
    virtual Error OnEnvVarsStatusesReceived(const String& nodeID, const Array<EnvVarsInstanceStatus>& statuses) = 0;
};

} // namespace aos::cm::launcher

#endif
