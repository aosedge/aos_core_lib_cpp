/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_BANDWIDTHMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_BANDWIDTHMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/networkmanager/itf/bandwidth.hpp>

namespace aos::sm::networkmanager {

class BandwidthMock : public BandwidthItf {
public:
    MOCK_METHOD(Error, Apply, (const String&, const BandwidthParams&), (override));
    MOCK_METHOD(Error, Clear, (const String&), (override));
};

} // namespace aos::sm::networkmanager

#endif
