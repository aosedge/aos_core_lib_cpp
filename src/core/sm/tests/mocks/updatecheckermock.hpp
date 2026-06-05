/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_UPDATECHECKERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_UPDATECHECKERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/launcher/itf/updatechecker.hpp>

namespace aos::sm::launcher {

/**
 * Update checker mock.
 */
class UpdateCheckerMock : public UpdateCheckerItf {
public:
    MOCK_METHOD(Error, Check, (), (override));
};

} // namespace aos::sm::launcher

#endif
