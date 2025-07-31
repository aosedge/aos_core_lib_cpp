/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_RUNNERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_RUNNERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/runner/runner.hpp>

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
