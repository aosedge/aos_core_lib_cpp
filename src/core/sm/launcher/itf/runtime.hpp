/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITF_RUNTIME_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_RUNTIME_HPP_

#include <core/common/monitoring/itf/instancemonitoringprovider.hpp>

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

/**
 * Runtime factory interface.
 */
class RuntimeFactoryItf {
public:
    /**
     * Destructor.
     */
    virtual ~RuntimeFactoryItf() = default;

    /**
     * Creates runtime of the specified type.
     *
     * @param runtimeType runtime type.
     * @param[out] runtime created runtime.
     * @return Error.
     */
    virtual Error CreateRuntime(const String& runtimeType, RuntimeItf& runtime) = 0;
};

/** @} */

} // namespace aos::sm::launcher

#endif
