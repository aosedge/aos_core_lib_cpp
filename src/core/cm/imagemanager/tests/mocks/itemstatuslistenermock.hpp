/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_ITEMSTATUSLISTENERMOCK_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_ITEMSTATUSLISTENERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/imagemanager/itf/itemstatusprovider.hpp>

namespace aos::cm::imagemanager {

class ItemStatusListenerMock : public ItemStatusListenerItf {
public:
    MOCK_METHOD(void, OnItemsStatusesChanged, (const Array<UpdateItemStatus>&), (override));
    MOCK_METHOD(void, OnItemRemoved, (const String&), (override));
};

} // namespace aos::cm::imagemanager

#endif
