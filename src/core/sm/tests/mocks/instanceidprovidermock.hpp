/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_INSTANCEIDPROVIDERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_INSTANCEIDPROVIDERMOCK_HPP_

#include <core/sm/launcher/itf/instanceidprovider.hpp>

namespace aos::sm::launcher {

/**
 * Instance ID provider mock.
 */
class InstanceIDProviderMock : public InstanceIDProviderItf {
public:
    MOCK_METHOD(Error, GetInstanceID, (const InstanceIdent& instance, String& instanceID), (const, override));
};

} // namespace aos::sm::launcher

#endif
