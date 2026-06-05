/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_TESTS_MOCKS_LAUNCHERMOCK_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_TESTS_MOCKS_LAUNCHERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/launcher/itf/launcher.hpp>

namespace aos::cm::launcher {

/**
 * Launcher mock.
 */
class LauncherMock : public LauncherItf {
public:
    MOCK_METHOD(
        Error, RunInstances, (const Array<RunInstanceRequest>& instances, Array<InstanceStatus>& statuses), (override));
    MOCK_METHOD(Error, GetInstancesStatuses, (Array<InstanceStatus> & statuses), (override));
    MOCK_METHOD(Error, SubscribeListener, (instancestatusprovider::ListenerItf & listener), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (instancestatusprovider::ListenerItf & listener), (override));
};

} // namespace aos::cm::launcher

#endif
