/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_STATUSLISTENER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_STATUSLISTENER_HPP_

#include <gmock/gmock.h>

#include <core/cm/imagemanager/itf/statusnotifier.hpp>

namespace aos::cm::imagemanager {

class MockStatusListener : public StatusListenerItf {
public:
    MOCK_METHOD(void, OnImageStatusChanged, (const String& id, const ImageStatus& status), (override));
    MOCK_METHOD(void, OnUpdateItemRemoved, (const String& id), (override));
};

} // namespace aos::cm::imagemanager

#endif
