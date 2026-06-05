/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_STORAGEMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_STORAGEMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/storage.hpp>

namespace aos::sm::networkmanager {

class StorageMock : public StorageItf {
public:
    MOCK_METHOD(Error, RemoveNetworkInfo, (const String&), (override));
    MOCK_METHOD(Error, AddNetworkInfo, (const NetworkInfo&), (override));
    MOCK_METHOD(Error, GetNetworksInfo, (Array<NetworkInfo>&), (const, override));
    MOCK_METHOD(Error, AddInstanceNetworkInfo, (const InstanceNetworkInfo&), (override));
    MOCK_METHOD(Error, RemoveInstanceNetworkInfo, (const String&), (override));
    MOCK_METHOD(Error, GetInstanceNetworksInfo, (Array<InstanceNetworkInfo>&), (const, override));
    MOCK_METHOD(Error, SetTrafficMonitorData, (const String&, const Time&, uint64_t), (override));
    MOCK_METHOD(Error, GetTrafficMonitorData, (const String&, Time&, uint64_t&), (const, override));
    MOCK_METHOD(Error, RemoveTrafficMonitorData, (const String&), (override));
};

} // namespace aos::sm::networkmanager

#endif
