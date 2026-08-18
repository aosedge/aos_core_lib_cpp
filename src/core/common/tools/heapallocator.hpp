/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_HEAPALLOCATOR_HPP_
#define AOS_CORE_COMMON_TOOLS_HEAPALLOCATOR_HPP_

#include <cstdlib>

#include "memory.hpp"

namespace aos {

/**
 * Heap allocator instance. Backs the AllocatorItf interface with the standard heap
 * (malloc/free). Intended for hosted targets (e.g. Linux) where dynamic memory allocation
 * is acceptable, and for unit tests; safety critical/embedded targets should provide their
 * own custom AllocatorItf implementation.
 */
class HeapAllocator : public AllocatorItf {
public:
    /**
     * Allocates data with specified size.
     *
     * @param size allocate size.
     * @return void* pointer to allocated data, nullptr if allocation failed.
     */
    void* Allocate(size_t size) override { return std::malloc(size); }

    /**
     * Frees previously allocated data.
     *
     * @param data allocated data to free.
     */
    void Free(void* data) override { std::free(data); }
};

} // namespace aos

#endif
