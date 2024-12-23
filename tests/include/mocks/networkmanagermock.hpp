/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NETWORKMANAGER_MOCK_HPP_
#define AOS_NETWORKMANAGER_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/sm/networkmanager.hpp"

namespace aos::sm::networkmanager {

class StorageMock : public StorageItf {
public:
    MOCK_METHOD(Error, RemoveNetworkInfo, (const String& networkID), (override));
    MOCK_METHOD(Error, AddNetworkInfo, (const NetworkParameters& info), (override));
    MOCK_METHOD(Error, GetNetworksInfo, (Array<NetworkParameters> & networks), (const, override));
    MOCK_METHOD(Error, SetTrafficMonitorData, (const String& chain, const Time& time, uint64_t value), (override));
    MOCK_METHOD(Error, GetTrafficMonitorData, (const String& chain, Time& time, uint64_t& value), (const, override));
    MOCK_METHOD(Error, RemoveTrafficMonitorData, (const String& chain), (override));
};

class TrafficMonitorMock : public TrafficMonitorItf {
public:
    MOCK_METHOD(Error, Start, (), (override));
    MOCK_METHOD(Error, Close, (), (override));
    MOCK_METHOD(void, SetPeriod, (TrafficPeriod period), (override));
    MOCK_METHOD(Error, StartInstanceMonitoring,
        (const String& instanceID, const String& IPAddress, uint64_t downloadLimit, uint64_t uploadLimit), (override));
    MOCK_METHOD(Error, StopInstanceMonitoring, (const String& instanceID), (override));
    MOCK_METHOD(Error, GetSystemData, (uint64_t & inputTraffic, uint64_t& outputTraffic), (const, override));
    MOCK_METHOD(Error, GetInstanceTraffic, (const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic),
        (const, override));
};

class NetworkManagerMock : public NetworkManagerItf {
    MOCK_METHOD(Error, GetNetnsPath, (const String& instanceID, String& netnsPath), (const, override));
    MOCK_METHOD(Error, UpdateNetworks, (const Array<aos::NetworkParameters>& networks), (override));
    MOCK_METHOD(Error, AddInstanceToNetwork,
        (const String& instanceID, const String& networkID, const NetworkParams& network), (override));
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
