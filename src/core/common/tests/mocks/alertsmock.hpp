/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_ALERTSMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_ALERTSMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/alerts/alerts.hpp>

namespace aos::alerts {

class AlertSenderMock : public SenderItf {
public:
    MOCK_METHOD(Error, SendAlert, (const AlertVariant&), (override));
};

} // namespace aos::alerts

#endif
