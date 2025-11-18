/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_STATUSLISTENERMOCK_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_STATUSLISTENERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/imagemanager/itf/itemstatusprovider.hpp>

namespace aos::cm::imagemanager {

/**
 * Status listener mock.
 */
class StatusListenerMock : public ItemStatusListenerItf {
public:
    MOCK_METHOD(void, OnItemStatusChanged, (const UpdateItemStatus&), (override));
    MOCK_METHOD(void, OnUpdateItemRemoved, (const String&), (override));
};

} // namespace aos::cm::imagemanager

#endif
