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

class DNSServerMock : public DNSServerItf {
public:
    MOCK_METHOD(Error, AddHost, (const String&, const DNSAliasesParams&), (override));
    MOCK_METHOD(Error, RemoveHost, (const String&), (override));
};

class DNSNameMock : public DNSNameItf {
public:
    MOCK_METHOD(Error, RemoveOrphans, (const Array<StaticString<cIDLen>>&), (override));
    MOCK_METHOD((RetWithError<DNSServerItf*>), CreateServer, (const String&, const DNSServerParams&), (override));
    MOCK_METHOD(Error, RemoveServer, (const String&), (override));
};

} // namespace aos::sm::networkmanager

#endif
