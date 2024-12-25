/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SPACEALLOCATOR_HPP_
#define AOS_SPACEALLOCATOR_HPP_

#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/time.hpp"
#include "aos/common/types.hpp"

namespace aos::spaceallocator {

/**
 * Item remover interface.
 */
class ItemRemoverItf {
public:
    /**
     * Removes item.
     *
     * @param id item id.
     * @return Error.
     */
    virtual Error RemoveItem(const String& id) = 0;

    /**
     * Destructor.
     */
    virtual ~ItemRemoverItf() = default;
};

class SpaceItf {
public:
    /**
     * Accepts space.
     *
     * @return Error.
     */
    virtual Error Accept() = 0;

    /**
     * Releases space.
     *
     * @return Error.
     */
    virtual Error Release() = 0;

    /**
     * Resizes space.
     *
     * @param size new size.
     * @return Error.
     */
    virtual Error Resize(uint64_t size) = 0;

    /**
     * Returns space size.
     *
     * @return uint64_t.
     */
    virtual uint64_t Size() const = 0;

    /**
     * Destructor.
     */
    virtual ~SpaceItf() = default;
};

/**
 * Space allocator interface.
 */
class SpaceAllocatorItf {
public:
    /**
     * Allocates space.
     *
     * @param size size to allocate.
     * @return RetWithError<UniquePtr<SpaceItf>>.
     */
    virtual RetWithError<UniquePtr<SpaceItf>> AllocateSpace(uint64_t size) = 0;

    /**
     * Frees space.
     *
     * @param size size to free.
     * @return void.
     */
    virtual void FreeSpace(uint64_t size) = 0;

    /**
     * Adds outdated item.
     *
     * @param id item id.
     * @param size item size.
     * @param timestamp item timestamp.
     * @return Error.
     */
    virtual Error AddOutdatedItem(const String& id, uint64_t size, const Time& timestamp) = 0;

    /**
     * Restores outdated item.
     *
     * @param id item id.
     * @return Error.
     */
    virtual Error RestoreOutdatedItem(const String& id) = 0;

    /**
     * Destructor.
     */
    virtual ~SpaceAllocatorItf() = default;
};

/**
 * Space instance.
 */
class Space : public SpaceItf {
public:
    /**
     * Crates space instance.
     */
    explicit Space(size_t size)
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
    Error Resize(uint64_t size) override
    {
        mSize = size;

        return ErrorEnum::eNone;
    }

    /**
     * Returns space size.
     *
     * @return uint64_t.
     */
    uint64_t Size() const override { return mSize; }

private:
    size_t mSize;
};

/**
 * Space allocator instance.
 */
template <size_t cNumAllocations>
class SpaceAllocator : public SpaceAllocatorItf {
public:
    /**
     * Allocates space.
     *
     * @param size size to allocate.
     * @return RetWithError<UniquePtr<SpaceItf>>.
     */
    RetWithError<UniquePtr<SpaceItf>> AllocateSpace(uint64_t size) override
    {
        return UniquePtr<SpaceItf>(new (&mAllocator) Space(size));
    };

    /**
     * Frees space.
     *
     * @param size size to free.
     * @return void.
     */
    void FreeSpace(uint64_t size) override { (void)size; }

    /**
     * Adds outdated item.
     *
     * @param id item id.
     * @param size item size.
     * @param timestamp item timestamp.
     * @return Error.
     */
    Error AddOutdatedItem(const String& id, uint64_t size, const Time& timestamp) override
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

private:
    StaticAllocator<sizeof(Space) * cNumAllocations> mAllocator;
};

} // namespace aos::spaceallocator

#endif
