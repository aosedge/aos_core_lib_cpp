/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_ITEMSTATUSPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_ITEMSTATUSPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/imagemanager/itf/itemstatusprovider.hpp>

namespace aos::common::imagemanager {

/**
 * Item status provider mock.
 */
class ItemStatusProviderMock : public ItemStatusProviderItf {
public:
    MOCK_METHOD(Error, GetItemsStatuses, (Array<UpdateItemStatus>&), (override));
    MOCK_METHOD(Error, SetItemFail, (const String&, const String&, const Error&), (override));
    MOCK_METHOD(Error, SubscribeListener, (ItemStatusListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (ItemStatusListenerItf&), (override));
};

} // namespace aos::common::imagemanager

#endif
