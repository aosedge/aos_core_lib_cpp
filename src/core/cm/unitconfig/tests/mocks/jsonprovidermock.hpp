/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UNITCONFIG_TESTS_MOCKS_JSONPROVIDERMOCK_HPP_
#define AOS_CORE_CM_UNITCONFIG_TESTS_MOCKS_JSONPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/unitconfig/itf/jsonprovider.hpp>

namespace aos::cm::unitconfig {

/**
 * JSON provider mock.
 */
class JSONProviderMock : public JSONProviderItf {
public:
    MOCK_METHOD(Error, UnitConfigFromJSON, (const String& json, aos::UnitConfig& unitConfig), (const, override));
    MOCK_METHOD(Error, UnitConfigToJSON, (const aos::UnitConfig& unitConfig, String& json), (const, override));
};

} // namespace aos::cm::unitconfig

#endif // AOS_CORE_CM_UNITCONFIG_TESTS_MOCKS_JSONPROVIDERMOCK_HPP_
