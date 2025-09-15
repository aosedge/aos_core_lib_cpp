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

class MockSpace : public SpaceItf {
public:
    MOCK_METHOD(Error, Accept, (), (override));
    MOCK_METHOD(Error, Release, (), (override));
    MOCK_METHOD(Error, Resize, (size_t size), (override));
    MOCK_METHOD(size_t, Size, (), (const, override));
};

class MockSpaceAllocator : public SpaceAllocatorItf {
public:
    MOCK_METHOD(RetWithError<UniquePtr<SpaceItf>>, AllocateSpace, (size_t size), (override));
    MOCK_METHOD(void, FreeSpace, (size_t size), (override));
    MOCK_METHOD(Error, AddOutdatedItem, (const String& id, size_t size, const Time& timestamp), (override));
    MOCK_METHOD(Error, RestoreOutdatedItem, (const String& id), (override));
    MOCK_METHOD(Error, AllocateDone, (), (override));
};

} // namespace aos::spaceallocator

#endif
