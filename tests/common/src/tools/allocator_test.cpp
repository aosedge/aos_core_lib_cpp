/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/allocator.hpp"

using namespace aos;

TEST(common, Allocator)
{
    StaticAllocator<256> allocator;

    EXPECT_EQ(allocator.MaxSize(), 256);
    EXPECT_EQ(allocator.FreeSize(), 256);

    struct TestItem {
        void*  mData;
        size_t mSize;
    };

    TestItem testData[] = {{nullptr, 32}, {nullptr, 64}, {nullptr, 128}};

    auto freeSize = allocator.MaxSize();

    for (auto& item : testData) {
        item.mData = allocator.Allocate(item.mSize);
        freeSize -= item.mSize;
        EXPECT_EQ(allocator.FreeSize(), freeSize);
    }

    for (size_t i = 0; i < ArraySize(testData); i++) {
        allocator.Free(testData[i].mData);
        if (i < ArraySize(testData) - 1) {
            EXPECT_EQ(allocator.FreeSize(), freeSize);
        } else {
            EXPECT_EQ(allocator.FreeSize(), allocator.MaxSize());
        }
    }
}
