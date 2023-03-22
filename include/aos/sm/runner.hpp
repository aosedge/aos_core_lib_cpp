/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RUNNER_HPP_
#define AOS_RUNNER_HPP_

#include "aos/common/tools/array.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/types.hpp"

namespace aos {
namespace sm {
namespace runner {

/**
 * Instance run status.
 */
struct RunStatus {
    char             mInstanceID[cInstanceIDLen + 1];
    InstanceRunState mState;
    Error            mError;
};

/**
 * Runner interface.
 */
class RunnerItf {
public:
    /**
     * Starts instance.
     *
     * @param instanceID instance ID.
     * @param runtimeDir directory with runtime spec.
     * @return RunStatus.
     */
    virtual RunStatus StartInstance(const char* instanceID, const char* runtimeDir) = 0;

    /**
     * Stops instance.
     *
     * @param instanceID instance ID>
     * @return Error.
     */
    virtual Error StopInstance(const char* instanceID) = 0;
};

/**
 * Instance run status receiver interface.
 */
class RunStatusReceiverItf {
public:
    /**
     * Updates run instances status.
     *
     * @param instances instances state.
     * @return Error.
     */
    virtual Error UpdateRunStatus(const Array<RunStatus>& instances) = 0;
};

} // namespace runner
} // namespace sm
} // namespace aos

#endif
