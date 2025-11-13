/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_LAUNCHER_ITF_RUNTIME_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_RUNTIME_HPP_

#include <core/common/types/monitoring.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Runtime interface.
 */
class RuntimeItf {
public:
    /**
     * Destructor.
     */
    virtual ~RuntimeItf() = default;

    /**
     * Returns runtime info.
     *
     * @param[out] runtimeInfo runtime info.
     * @return Error.
     */
    virtual Error GetInfo(RuntimeInfo& runtimeInfo) const = 0;

    /**
     * Returns instance status.
     *
     * @param instance instance to get status for.
     * @param[out] status instance status.
     * @return Error.
     */
    virtual Error GetInstanceStatus(const InstanceIdent& instance, InstanceStatus& status) const = 0;

    /**
     * Returns all instances statuses.
     *
     * @param[out] statuses array to store instances statuses.
     * @return Error.
     */
    virtual Error GetAllInstancesStatuses(Array<InstanceStatus>& statuses) const = 0;

    /**
     * Start instance.
     *
     * @param instance instance to start.
     * @param[out] status instance status.
     * @return Error.
     */
    virtual Error StartInstance(const InstanceInfo& instance, InstanceStatus& status) = 0;

    /**
     * Stop instance.
     *
     * @param instance instance to stop.
     * @param[out] status instance status.
     * @return Error.
     */
    virtual Error StopInstance(const InstanceIdent& instance, InstanceStatus& status) = 0;

    /**
     * Returns instance monitoring data.
     *
     * @param instance instance to get monitoring data for.
     * @param[out] monitoring instance monitoring data.
     * @return Error.
     */
    virtual Error GetInstanceMonitoring(const InstanceIdent& instance, MonitoringData& monitoring) = 0;
};

} // namespace aos::sm::launcher

#endif
