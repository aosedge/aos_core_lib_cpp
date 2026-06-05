/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_INTERFACEFACTORYMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_INTERFACEFACTORYMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/interfacefactory.hpp>

namespace aos::sm::networkmanager {

class InterfaceFactoryMock : public InterfaceFactoryItf {
public:
    MOCK_METHOD(Error, CreateBridge, (const String&, const String&, const String&), (override));
    MOCK_METHOD(Error, CreateVlan, (const String&, uint64_t), (override));
};

} // namespace aos::sm::networkmanager

#endif
