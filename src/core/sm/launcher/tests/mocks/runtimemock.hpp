/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_TESTS_MOCKS_RUNTIMEMOCK_HPP_
#define AOS_CORE_SM_LAUNCHER_TESTS_MOCKS_RUNTIMEMOCK_HPP_

#include <core/sm/launcher/itf/runtime.hpp>

namespace aos::sm::launcher {

/**
 * Runtime mock.
 */
class RuntimeMock : public RuntimeItf {
public:
    MOCK_METHOD(
        Error, GetInstanceMonitoringData, (const InstanceIdent&, monitoring::InstanceMonitoringData&), (override));
    MOCK_METHOD(Error, Start, (), (override));
    MOCK_METHOD(Error, Stop, (), (override));
    MOCK_METHOD(Error, GetRuntimeInfo, (RuntimeInfo&), (const, override));
    MOCK_METHOD(Error, StartInstance, (const InstanceInfo&, InstanceStatus&), (override));
    MOCK_METHOD(Error, StopInstance, (const InstanceIdent&, InstanceStatus&), (override));
    MOCK_METHOD(Error, Reboot, (), (override));
};

} // namespace aos::sm::launcher

#endif
