/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RUNNER_STUB_HPP_
#define AOS_RUNNER_STUB_HPP_

#include "aos/sm/runner.hpp"

namespace aos::sm::runner {

/**
 * Runner stub.
 */
class RunnerStub : public RunnerItf {
public:
    /**
     * Starts instance.
     *
     * @param instanceID instance ID.
     * @param runtimeDir directory with runtime spec.
     * @return RunStatus.
     */
    RunStatus StartInstance(const String& instanceID, const String& runtimeDir, const RunParameters& runParams) override
    {
        (void)instanceID;
        (void)runtimeDir;
        (void)runParams;

        return RunStatus {instanceID, InstanceRunStateEnum::eActive, ErrorEnum::eNone};
    }

    /**
     * Stops instance.
     *
     * @param instanceID instance ID>
     * @return Error.
     */
    Error StopInstance(const String& instanceID) override
    {
        (void)instanceID;

        return ErrorEnum::eNone;
    }
};

} // namespace aos::sm::runner

#endif
