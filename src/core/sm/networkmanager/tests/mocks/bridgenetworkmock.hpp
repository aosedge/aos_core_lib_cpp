/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_BRIDGENETWORKMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_BRIDGENETWORKMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/bridgenetwork.hpp>

namespace aos::sm::networkmanager {

class BridgeNetworkMock : public BridgeNetworkItf {
public:
    MOCK_METHOD(Error, Attach, (const String&, const BridgeParams&, BridgeAttachResult&), (override));
    MOCK_METHOD(Error, Detach, (const String&, const String&, const String&), (override));
};

} // namespace aos::sm::networkmanager

#endif
