/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_NETWORKMANAGER_TESTS_MOCKS_PENDINGUPDATEHANDLERMOCK_HPP_
#define AOS_CORE_COMMON_NETWORKMANAGER_TESTS_MOCKS_PENDINGUPDATEHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/networkmanager/itf/pendingupdatehandler.hpp>

class PendingUpdateHandlerMock : public aos::networkmanager::PendingUpdateHandlerItf {
public:
    MOCK_METHOD(void, OnPendingFirewallUpdate,
        (const aos::String& nodeID, const aos::networkmanager::PendingFirewallUpdate& update), (override));
};

#endif
