/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_NAMESPACEMANAGERMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_NAMESPACEMANAGERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/namespacemanager.hpp>

namespace aos::sm::networkmanager {

class NamespaceManagerMock : public NamespaceManagerItf {
public:
    MOCK_METHOD(Error, CreateNetworkNamespace, (const String&), (override));
    MOCK_METHOD(RetWithError<StaticString<cFilePathLen>>, GetNetworkNamespacePath, (const String&), (const, override));
    MOCK_METHOD(Error, DeleteNetworkNamespace, (const String&), (override));
};

} // namespace aos::sm::networkmanager

#endif
