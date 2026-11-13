/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_ITEMINFOPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_ITEMINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/imagemanager/itf/iteminfoprovider.hpp>

namespace aos::common::imagemanager {

/**
 * Item info provider mock.
 */
class ItemInfoProviderMock : public ItemInfoProviderItf {
public:
    MOCK_METHOD(Error, GetBlobPath, (const String& digest, String& path), (const, override));
    MOCK_METHOD(Error, GetLayerPath, (const String& diffID, String& path), (const, override));
};

} // namespace aos::common::imagemanager

#endif
