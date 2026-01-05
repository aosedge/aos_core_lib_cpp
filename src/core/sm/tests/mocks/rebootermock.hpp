/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_REBOOTERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_REBOOTERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/launcher/itf/rebooter.hpp>

namespace aos::sm::launcher {

/**
 * Rebooter mock.
 */
class RebooterMock : public RebooterItf {
public:
    MOCK_METHOD(Error, Reboot, (), (override));
};

} // namespace aos::sm::launcher

#endif
