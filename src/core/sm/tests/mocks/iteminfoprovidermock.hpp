/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_ITEMINFOPROVIDERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_ITEMINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/imagemanager/itf/iteminfoprovider.hpp>

namespace aos::sm::imagemanager {

/**
 * Item info provider mock.
 */
class ItemInfoProviderMock : public ItemInfoProviderItf {
public:
    MOCK_METHOD(Error, GetBlobPath, (const String&, String&), (const, override));
    MOCK_METHOD(Error, GetLayerPath, (const String&, String&), (const, override));
};

} // namespace aos::sm::imagemanager

#endif
