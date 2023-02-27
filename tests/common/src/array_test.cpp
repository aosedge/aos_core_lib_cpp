/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/array.hpp"

using namespace aos;

TEST(common, Array)
{
    constexpr auto cNumItems = 32;

    // Array from buffer

    StaticBuffer<sizeof(int) * cNumItems> buffer;
    Array<int>                            bufferArray(buffer);

    EXPECT_EQ(bufferArray.Size(), 0);
    EXPECT_EQ(bufferArray.MaxSize(), cNumItems);

    // Static array

    StaticArray<int, cNumItems> staticArray(3);

    EXPECT_EQ(staticArray.Size(), 3);
    EXPECT_EQ(staticArray.MaxSize(), cNumItems);

    // Dynamic array

    DynamicArray<int, cNumItems> dynamicArray(cNumItems);

    EXPECT_EQ(dynamicArray.Size(), cNumItems);
    EXPECT_EQ(dynamicArray.MaxSize(), cNumItems);

    // Fill array by push

    for (size_t i = 0; i < cNumItems; i++) {
        EXPECT_TRUE(bufferArray.PushBack(i).IsNone());
    }

    EXPECT_EQ(bufferArray.Size(), cNumItems);

    // Fill array by index

    for (size_t i = 0; i < dynamicArray.Size(); i++) {
        dynamicArray[i] = i;
    }

    // Check At

    for (size_t i = 0; i < bufferArray.Size(); i++) {
        auto ret1 = bufferArray.At(i);
        EXPECT_TRUE(ret1.mError.IsNone());

        auto ret2 = dynamicArray.At(i);
        EXPECT_TRUE(ret2.mError.IsNone());

        EXPECT_EQ(*ret1.mValue, *ret2.mValue);
    }

    // Range base loop

    auto i = 0;

    for (const auto& value : bufferArray) {
        EXPECT_EQ(value, dynamicArray[i++]);
    }

    // Check pop operation

    for (i = cNumItems - 1; i >= 0; i--) {
        auto ret1 = bufferArray.PopBack();
        EXPECT_TRUE(ret1.mError.IsNone());

        auto ret2 = dynamicArray.PopBack();
        EXPECT_TRUE(ret2.mError.IsNone());

        EXPECT_EQ(*ret1.mValue, *ret2.mValue);
    }

    // Test const array

    static const int cArray[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    const auto& constArray = Array<const int>(cArray, ArraySize(cArray));

    EXPECT_EQ(constArray.Size(), ArraySize(cArray));
    EXPECT_EQ(constArray.MaxSize(), ArraySize(cArray));

    i = 0;

    for (auto& value : constArray) {
        EXPECT_EQ(value, cArray[i++]);
    }
}
