/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_LAUNCHERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_LAUNCHERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/launcher/itf/instancestatusreceiver.hpp>
#include <core/sm/launcher/itf/launcher.hpp>

namespace aos::sm::launcher {

/**
 * Launcher mock.
 */
class LauncherMock : public LauncherItf {
public:
    MOCK_METHOD(Error, UpdateInstances,
        (const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances), (override));
};

} // namespace aos::sm::launcher

#endif
