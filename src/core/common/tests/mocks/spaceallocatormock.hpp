/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_SPACEALLOCATORMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_SPACEALLOCATORMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/spaceallocator/spaceallocator.hpp>

namespace aos::spaceallocator {

class ItemRemoverMock : public ItemRemoverItf {
public:
    MOCK_METHOD(Error, RemoveItem, (const String& id), (override));
};

} // namespace aos::spaceallocator

#endif
