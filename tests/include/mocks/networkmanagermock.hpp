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
    MOCK_METHOD(Error, AddNetworkInfo, (const NetworkInfo& info), (override));
    MOCK_METHOD(Error, GetNetworksInfo, (Array<NetworkInfo> & networks), (const, override));
    MOCK_METHOD(Error, AddInstanceNetworkInfo, (const InstanceNetworkInfo& info), (override));
    MOCK_METHOD(Error, RemoveInstanceNetworkInfo, (const String& instanceID), (override));
    MOCK_METHOD(Error, GetInstanceNetworksInfo, (Array<InstanceNetworkInfo> & networks), (const, override));
    MOCK_METHOD(Error, SetTrafficMonitorData, (const String& chain, const Time& time, uint64_t value), (override));
    MOCK_METHOD(Error, GetTrafficMonitorData, (const String& chain, Time& time, uint64_t& value), (const, override));
    MOCK_METHOD(Error, RemoveTrafficMonitorData, (const String& chain), (override));
};

class TrafficMonitorMock : public TrafficMonitorItf {
public:
    MOCK_METHOD(Error, Start, (), (override));
    MOCK_METHOD(Error, Stop, (), (override));
    MOCK_METHOD(void, SetPeriod, (TrafficPeriod period), (override));
    MOCK_METHOD(Error, StartInstanceMonitoring,
        (const String& instanceID, const String& IPAddress, uint64_t downloadLimit, uint64_t uploadLimit), (override));
    MOCK_METHOD(Error, StopInstanceMonitoring, (const String& instanceID), (override));
    MOCK_METHOD(Error, GetSystemData, (uint64_t & inputTraffic, uint64_t& outputTraffic), (const, override));
    MOCK_METHOD(Error, GetInstanceTraffic, (const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic),
        (const, override));
};

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

class NamespaceManagerMock : public NamespaceManagerItf {
public:
    MOCK_METHOD(Error, CreateNetworkNamespace, (const String& ns), (override));
    MOCK_METHOD(
        RetWithError<StaticString<cFilePathLen>>, GetNetworkNamespacePath, (const String& ns), (const, override));
    MOCK_METHOD(Error, DeleteNetworkNamespace, (const String& ns), (override));
};

class InterfaceManagerMock : public InterfaceManagerItf {
public:
    MOCK_METHOD(Error, DeleteLink, (const String& ifname), (override));
    MOCK_METHOD(Error, SetupLink, (const String& ifname), (override));
    MOCK_METHOD(Error, SetMasterLink, (const String& ifname, const String& master), (override));
};

class InterfaceFactoryMock : public InterfaceFactoryItf {
public:
    MOCK_METHOD(Error, CreateBridge, (const String& name, const String& ip, const String& subnet), (override));
    MOCK_METHOD(Error, CreateVlan, (const String& name, uint64_t vlanId), (override));
};

class RandomMock : public crypto::RandomItf {
public:
    MOCK_METHOD(RetWithError<uint64_t>, RandInt, (uint64_t maxValue), (override));
    MOCK_METHOD(Error, RandBuffer, (Array<uint8_t> & buffer, size_t size), (override));
};

} // namespace aos::sm::networkmanager

#endif
