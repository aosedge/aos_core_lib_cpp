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

namespace aos::sm::runner {
/**
 * Instance run status.
 */
struct RunStatus {
    StaticString<cInstanceIDLen> mInstanceID;
    InstanceRunState             mState;
    Error                        mError;

    /**
     * Compares run statuses.
     *
     * @param other run status to compare.
     * @return bool.
     */
    bool operator==(const RunStatus& other) const
    {
        return mInstanceID == other.mInstanceID && mState == other.mState && mError == other.mError;
    }

    /**
     * Compares run statuses.
     *
     * @param other run status to compare.
     * @return bool.
     */
    bool operator!=(const RunStatus& other) const { return !(*this == other); }
};

/**
 * Service run parameters.
 */
struct RunParameters {
    aos::Duration mStartInterval;
    aos::Duration mRestartInterval;
    long          mStartBurst;
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
     * @param runParams runtime parameters.
     * @return RunStatus.
     */
    virtual RunStatus StartInstance(const String& instanceID, const String& runtimeDir, const RunParameters& runParams)
        = 0;

    /**
     * Stops instance.
     *
     * @param instanceID instance ID>
     * @return Error.
     */
    virtual Error StopInstance(const String& instanceID) = 0;

    /**
     * Destructs runner interface.
     */
    virtual ~RunnerItf() = default;
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

    /**
     * Destructs run status receiver interface.
     */
    virtual ~RunStatusReceiverItf() = default;
};

} // namespace aos::sm::runner

#endif
