/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITF_RUNTIME_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_RUNTIME_HPP_

#include <core/common/monitoring/itf/instancemonitoringprovider.hpp>
#include <core/common/types/instance.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Runtime interface.
 */
class RuntimeItf : public monitoring::InstanceMonitoringProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~RuntimeItf() = default;

    /**
     * Starts runtime.
     *
     * @return Error.
     */
    virtual Error Start() = 0;

    /**
     * Stops runtime.
     *
     * @return Error.
     */
    virtual Error Stop() = 0;

    /**
     * Returns runtime info.
     *
     * @param[out] runtimeInfo runtime info.
     * @return Error.
     */
    virtual Error GetRuntimeInfo(RuntimeInfo& runtimeInfo) const = 0;

    /**
     * Initializes instances.
     *
     * Launcher provides list of known instances to runtime at startup. Runtime should stop all instances that are not
     * in the list and properly initialize already running instances. Runtime should not start any instance at this
     * stage, it should only prepare them for future start.
     *
     * @param instancesInfo instances info.
     * @return Error.
     */
    virtual Error InitInstances(const Array<InstanceInfo>& instancesInfo) = 0;

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
     * Reboots runtime.
     *
     * @return Error.
     */
    virtual Error Reboot() = 0;
};

/** @} */

} // namespace aos::sm::launcher

#endif
