/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tools/heapallocator.hpp>
#include <core/common/tools/memory.hpp>

using namespace aos;

TEST(AllocatorTest, HeapAllocator)
{
    HeapAllocator allocator;

    auto* data = allocator.Allocate(128);
    ASSERT_NE(data, nullptr);

    allocator.Free(data);

    struct TestStruct {
        TestStruct(int a, int b)
            : mA(a)
            , mB(b)
        {
        }

        int mA;
        int mB;
    };

    auto uPtr = MakeUnique<TestStruct>(&allocator, 1, 2);
    ASSERT_TRUE(uPtr);
    EXPECT_EQ(uPtr->mA, 1);
    EXPECT_EQ(uPtr->mB, 2);

    auto shPtr = MakeShared<TestStruct>(&allocator, 3, 4);
    ASSERT_TRUE(shPtr);
    EXPECT_EQ(shPtr->mA, 3);
    EXPECT_EQ(shPtr->mB, 4);

    auto shPtr2 = shPtr;
    EXPECT_EQ(shPtr2->mA, 3);

    shPtr.Reset();
    EXPECT_FALSE(shPtr);
    EXPECT_TRUE(shPtr2);
    EXPECT_EQ(shPtr2->mB, 4);
}
