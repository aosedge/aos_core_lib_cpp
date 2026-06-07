/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_FIREWALLMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_FIREWALLMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/firewall.hpp>

namespace aos::sm::networkmanager {

class FirewallMock : public FirewallItf {
public:
    MOCK_METHOD(Error, Start, (), (override));
    MOCK_METHOD(Error, Stop, (), (override));
    MOCK_METHOD(Error, AddInstance, (const String&, const InstanceFirewallParams&), (override));
    MOCK_METHOD(Error, RemoveInstance, (const String&), (override));
    MOCK_METHOD(Error, UpdateInstance, (const String&, const InstanceFirewallParams&), (override));
    MOCK_METHOD(Error, AddMasquerade, (const String&, const String&), (override));
    MOCK_METHOD(Error, RemoveMasquerade, (const String&, const String&), (override));
};

} // namespace aos::sm::networkmanager

#endif
