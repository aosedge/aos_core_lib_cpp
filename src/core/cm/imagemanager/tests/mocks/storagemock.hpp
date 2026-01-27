/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_STORAGE_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_STORAGE_HPP_

#include <gmock/gmock.h>

#include <core/cm/imagemanager/itf/storage.hpp>

namespace aos::cm::imagemanager {

/**
 * Storage mock.
 */
class StorageMock : public StorageItf {
public:
    MOCK_METHOD(Error, AddItem, (const ItemInfo& item), (override));
    MOCK_METHOD(Error, RemoveItem, (const String& id, const String& version), (override));
    MOCK_METHOD(
        Error, UpdateItemState, (const String& id, const String& version, ItemState state, Time timestamp), (override));
    MOCK_METHOD(Error, GetAllItemsInfos, (Array<ItemInfo> & items), (override));
    MOCK_METHOD(Error, GetItemInfos, (const String& itemID, Array<ItemInfo>& items), (override));
};

} // namespace aos::cm::imagemanager

#endif
