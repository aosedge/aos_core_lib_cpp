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
class StorageMock : public storage::StorageItf {
public:
    MOCK_METHOD(Error, SetItemState, (const String& id, const String& version, storage::ItemState state), (override));
    MOCK_METHOD(Error, RemoveItem, (const String& id, const String& version), (override));
    MOCK_METHOD(Error, GetItemsInfo, (Array<storage::ItemInfo> & items), (override));
    MOCK_METHOD(Error, GetItemVersionsByID, (const String& id, Array<storage::ItemInfo>& items), (override));
    MOCK_METHOD(Error, AddItem, (const storage::ItemInfo& item), (override));
};

} // namespace aos::cm::imagemanager

#endif
