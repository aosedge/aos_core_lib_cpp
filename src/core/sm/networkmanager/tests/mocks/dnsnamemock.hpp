/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_DNSNAMEMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_DNSNAMEMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/dnsname.hpp>

namespace aos::sm::networkmanager {

class DNSNameMock : public DNSNameItf {
public:
    MOCK_METHOD(Error, Start, (), (override));
    MOCK_METHOD(Error, Stop, (), (override));
    MOCK_METHOD(Error, AddInstance, (const String&, const DNSAliasesParams&), (override));
    MOCK_METHOD(Error, RemoveInstance, (const String&), (override));
};

} // namespace aos::sm::networkmanager

#endif
