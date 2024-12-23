/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RUNNER_MOCK_HPP_
#define AOS_RUNNER_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/sm/runner.hpp"

namespace aos::sm::runner {

/**
 * Runner mock.
 */
class RunnerMock : public RunnerItf {
public:
    MOCK_METHOD(RunStatus, StartInstance,
        (const String& instanceID, const String& runtimeDir, const RunParameters& runParams), (override));
    MOCK_METHOD(Error, StopInstance, (const String& instanceID), (override));
};

} // namespace aos::sm::runner

#endif
