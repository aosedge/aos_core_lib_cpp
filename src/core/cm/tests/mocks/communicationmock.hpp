/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_COMMUNICATIONMOCK_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_COMMUNICATIONMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/communication/communication.hpp>

namespace aos::cm::communication {

/**
 * Communication interface mock.
 */
class CommunicationMock : public CommunicationItf {
public:
    MOCK_METHOD(Error, SendMessage, (const void*), (override));
};

} // namespace aos::cm::communication

#endif
