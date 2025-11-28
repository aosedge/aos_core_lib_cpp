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
     * @param timestamp item timestamp.
     * @return Error.
     */
    virtual Error AddOutdatedItem(const String& id, const Time& timestamp) = 0;

    /**
     * Restores outdated item.
     *
     * @param id item id.
     * @return Error.
     */
    virtual Error RestoreOutdatedItem(const String& id) = 0;

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
