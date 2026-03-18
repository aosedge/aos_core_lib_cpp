/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_NETWORKPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_NETWORKPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/networkmanager/itf/networkprovider.hpp>

namespace aos::networkmanager {

/**
 * Network provider mock.
 */
class NetworkProviderMock : public NetworkProviderItf {
public:
    MOCK_METHOD(Error, GetNodeNetworkParams, (const String& networkID, const String& nodeID, NetworkParams& result),
        (override));
    MOCK_METHOD(Error, AllocateInstanceNetwork,
        (const InstanceIdent& instance, const String& networkID, const String& nodeID,
            const UpdateItemNetworkParams& serviceData, InstanceNetworkAllocation& result),
        (override));
    MOCK_METHOD(Error, ReleaseInstanceNetwork, (const InstanceIdent& instance, const String& nodeID), (override));
    MOCK_METHOD(Error, ReleaseNodeNetwork, (const String& networkID, const String& nodeID), (override));
};

} // namespace aos::networkmanager

#endif
