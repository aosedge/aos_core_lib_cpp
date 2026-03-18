/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_NETWORKPROVIDERMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_NETWORKPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/networkmanager/itf/networkprovider.hpp>

class NetworkProviderMock : public aos::networkmanager::NetworkProviderItf {
public:
    MOCK_METHOD(
        aos::Error, GetNodeNetworkParams, (const aos::String&, const aos::String&, aos::NetworkParams&), (override));
    MOCK_METHOD(aos::Error, AllocateInstanceNetwork,
        (const aos::InstanceIdent&, const aos::String&, const aos::String&, const aos::UpdateItemNetworkParams&,
            aos::InstanceNetworkAllocation&),
        (override));
    MOCK_METHOD(aos::Error, ReleaseInstanceNetwork, (const aos::InstanceIdent&, const aos::String&), (override));
    MOCK_METHOD(aos::Error, ReleaseNodeNetwork, (const aos::String&, const aos::String&), (override));
};

#endif
