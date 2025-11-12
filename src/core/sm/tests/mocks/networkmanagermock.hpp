/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_NETWORKMANAGERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_NETWORKMANAGERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/networkmanager.hpp>

namespace aos::sm::networkmanager {

class NetworkManagerMock : public NetworkManagerItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cFilePathLen>>, GetNetnsPath, (const String& instanceID), (const, override));
    MOCK_METHOD(Error, UpdateNetworks, (const Array<aos::NetworkParameters>& networks), (override));
    MOCK_METHOD(Error, AddInstanceToNetwork,
        (const String& instanceID, const String& networkID, const InstanceNetworkParameters& instanceNetworkParameters),
        (override));
    MOCK_METHOD(Error, RemoveInstanceFromNetwork, (const String& instanceID, const String& networkID), (override));
    MOCK_METHOD(
        Error, GetInstanceIP, (const String& instanceID, const String& networkID, String& ip), (const, override));
    MOCK_METHOD(Error, GetInstanceTraffic, (const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic),
        (const, override));
    MOCK_METHOD(Error, GetSystemTraffic, (uint64_t & inputTraffic, uint64_t& outputTraffic), (const, override));
    MOCK_METHOD(Error, SetTrafficPeriod, (TrafficPeriod period), (override));
};

} // namespace aos::sm::networkmanager

#endif
