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

} // namespace aos::spaceallocator

#endif
