/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_SPACEALLOCATOR_ITF_SPACEALLOCATOR_HPP_
#define AOS_CORE_COMMON_SPACEALLOCATOR_ITF_SPACEALLOCATOR_HPP_

#include <core/common/tools/time.hpp>

namespace aos::spaceallocator {

/**
 * Item remover interface.
 */
class ItemRemoverItf {
public:
    /**
     * Removes item.
     *
     * It is called with the space allocator lock held, so the implementation must not call the methods of any space
     * allocator of the same partition and must not take a lock that is held by a caller of the space allocator methods.
     *
     * @param id item id.
     * @param version item version.
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> RemoveItem(const String& id, const String& version) = 0;

    /**
     * Destructor.
     */
    virtual ~ItemRemoverItf() = default;
};

/**
 * Space interface.
 */
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
    virtual Error Resize(size_t size) = 0;

    /**
     * Returns space size.
     *
     * @return size_t.
     */
    virtual size_t Size() const = 0;

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
    virtual RetWithError<UniquePtr<SpaceItf>> AllocateSpace(size_t size) = 0;

    /**
     * Frees space.
     *
     * @param size size to free.
     * @return void.
     */
    virtual void FreeSpace(size_t size) = 0;

    /**
     * Adds outdated item.
     *
     * @param id item id.
     * @param version item version.
     * @param timestamp item timestamp.
     * @return Error.
     */
    virtual Error AddOutdatedItem(const String& id, const String& version, const Time& timestamp) = 0;

    /**
     * Restores outdated item.
     *
     * @param id item id.
     * @param version item version.
     * @return Error.
     */
    virtual Error RestoreOutdatedItem(const String& id, const String& version) = 0;

    /**
     * Removes the oldest outdated item.
     *
     * The freed size is already accounted by the space allocator: the caller must not call FreeSpace for it. If the
     * item remover fails, the item is still dropped from the outdated items list and the remover error is returned.
     * The item remover is called with the partition lock taken (shared by all space allocators on the same mount
     * point): the item remover must not call any space allocator of this partition, and this method must not be
     * called while holding a mutex the item remover might need.
     *
     * @return RetWithError<size_t>: freed size, eNotFound if there is no outdated item to remove.
     */
    virtual RetWithError<size_t> RemoveOutdatedItem() = 0;

    /**
     * Allocates done.
     *
     * @return Error.
     */
    virtual Error AllocateDone() = 0;

    /**
     * Destructor.
     */
    virtual ~SpaceAllocatorItf() = default;
};

} // namespace aos::spaceallocator

#endif
