/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_INTERFACEMANAGERMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_INTERFACEMANAGERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/interfacemanager.hpp>

namespace aos::sm::networkmanager {

class InterfaceManagerMock : public InterfaceManagerItf {
public:
    MOCK_METHOD(Error, DeleteLink, (const String&), (override));
    MOCK_METHOD(Error, SetupLink, (const String&), (override));
    MOCK_METHOD(Error, SetMasterLink, (const String&, const String&), (override));
};

} // namespace aos::sm::networkmanager

#endif
