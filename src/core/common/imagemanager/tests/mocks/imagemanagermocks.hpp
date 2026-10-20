/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_TESTS_MOCKS_IMAGEMANAGERMOCKS_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_TESTS_MOCKS_IMAGEMANAGERMOCKS_HPP_

#include <gmock/gmock.h>

#include <core/common/imagemanager/itf/itemstatusprovider.hpp>

namespace aos::common::imagemanager {

/**
 * Item status listener mock.
 */
class ItemStatusListenerMock : public ItemStatusListenerItf {
public:
    MOCK_METHOD(void, OnItemStatusChanged, (const UpdateItemStatus& status), (override));
    MOCK_METHOD(void, OnItemRemovedFromCache, (const String& id, const String& version), (override));
};

} // namespace aos::common::imagemanager

#endif
