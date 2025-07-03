/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_COMMUNICATION_MOCK_HPP_
#define AOS_COMMUNICATION_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/cm/communication/communication.hpp"

namespace aos::cm::communication {

/**
 * Communication interface mock.
 */
class CommunicationMock : public CommunicationItf {
public:
    MOCK_METHOD(Error, SendInstanceNewState, (const cloudprotocol::NewState& newState), (override));
    MOCK_METHOD(Error, SendInstanceStateRequest, (const cloudprotocol::StateRequest& stateRequest), (override));
};

} // namespace aos::cm::communication

#endif
