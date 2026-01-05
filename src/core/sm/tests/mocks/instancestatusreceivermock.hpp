/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_INSTANCESTATUSRECEIVERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_INSTANCESTATUSRECEIVERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/launcher/itf/instancestatusreceiver.hpp>

namespace aos::sm::launcher {

/**
 * Instance status receiver mock.
 */
class InstanceStatusReceiverMock : public InstanceStatusReceiverItf {
public:
    MOCK_METHOD(Error, OnInstancesStatusesReceived, (const Array<InstanceStatus>&), (override));
    MOCK_METHOD(Error, RebootRequired, (const String&), (override));
};

} // namespace aos::sm::launcher

#endif
