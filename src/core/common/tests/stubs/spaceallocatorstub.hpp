/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_STUBS_SPACEALLOCATORSTUB_HPP_
#define AOS_CORE_COMMON_TESTS_STUBS_SPACEALLOCATORSTUB_HPP_

#include <core/common/spaceallocator/spaceallocator.hpp>
#include <core/common/tools/memory.hpp>

namespace aos::spaceallocator {

/**
 * Space stub.
 */
class SpaceStub : public SpaceItf {
public:
    /**
     * Creates space stub.
     *
     * @param size space size.
     */
    explicit SpaceStub(size_t size)
        : mSize(size)
    {
    }

    /**
     * Accepts space.
     *
     * @return Error.
     */
    Error Accept() override { return ErrorEnum::eNone; }

    /**
     * Releases space.
     *
     * @return Error.
     */
    Error Release() override { return ErrorEnum::eNone; }

    /**
     * Resizes space.
     *
     * @param size new size.
     * @return Error.
     */
    Error Resize(size_t size) override
    {
        mSize = size;

        return ErrorEnum::eNone;
    }

    /**
     * Returns space size.
     *
     * @return uint64_t.
     */
    size_t Size() const override { return mSize; }

private:
    size_t mSize = 0;
};

/**
 * Space allocator stub.
 */
class SpaceAllocatorStub : public SpaceAllocatorItf {
public:
    /**
     * Allocates space.
     *
     * @param size size to allocate.
     * @return RetWithError<UniquePtr<SpaceItf>>.
     */
    RetWithError<UniquePtr<SpaceItf>> AllocateSpace(size_t size) override
    {
        return {UniquePtr<SpaceItf>(MakeUnique<SpaceStub>(&mAllocator, size))};
    }

    /**
     * Frees space.
     *
     * @param size size to free.
     * @return void.
     */
    void FreeSpace(size_t size) override
    {
        (void)size;

        return;
    }

    /**
     * Adds outdated item.
     *
     * @param id item id.
     * @param size item size.
     * @param timestamp item timestamp.
     * @return Error.
     */
    Error AddOutdatedItem(const String& id, size_t size, const Time& timestamp) override
    {
        (void)id;
        (void)size;
        (void)timestamp;

        return ErrorEnum::eNone;
    }

    /**
     * Restores outdated item.
     *
     * @param id item id.
     * @return Error.
     */
    Error RestoreOutdatedItem(const String& id) override
    {
        (void)id;

        return ErrorEnum::eNone;
    }

    /**
     * Allocates done.
     *
     * @return Error.
     */
    Error AllocateDone() override { return ErrorEnum::eNone; }

private:
    StaticAllocator<1024> mAllocator;
};

} // namespace aos::spaceallocator

#endif
