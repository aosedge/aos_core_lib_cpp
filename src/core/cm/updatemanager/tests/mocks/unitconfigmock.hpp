/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_TESTS_MOCKS_UNITCONFIGMOCK_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_TESTS_MOCKS_UNITCONFIGMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/unitconfig/itf/unitconfig.hpp>

namespace aos::cm::unitconfig {

/**
 * Unit config mock.
 */
class UnitConfigMock : public UnitConfigItf {
public:
    MOCK_METHOD(Error, CheckUnitConfig, (const UnitConfig&), (override));
    MOCK_METHOD(Error, UpdateUnitConfig, (const UnitConfig&), (override));
    MOCK_METHOD(Error, GetUnitConfigStatus, (UnitConfigStatus&), (override));
};

} // namespace aos::cm::unitconfig

#endif
