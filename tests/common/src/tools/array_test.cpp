/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/array.hpp"

using namespace aos;

TEST(ArrayTest, Basic)
{
    constexpr auto cNumItems = 32;

    // Array from buffer

    StaticBuffer<sizeof(int) * cNumItems> buffer;
    Array<int>                            bufferArray(buffer);

    EXPECT_EQ(bufferArray.Size(), 0);
    EXPECT_EQ(bufferArray.MaxSize(), cNumItems);

    // Static array

    StaticArray<int, cNumItems> staticArray;

    EXPECT_TRUE(staticArray.Resize(3).IsNone());

    EXPECT_EQ(staticArray.Size(), 3);
    EXPECT_EQ(staticArray.MaxSize(), cNumItems);

    // Fill array by push

    bufferArray.Clear();

    for (size_t i = 0; i < cNumItems; i++) {
        EXPECT_TRUE(bufferArray.PushBack(i).IsNone());
    }

    EXPECT_EQ(bufferArray.Size(), cNumItems);

    // Fill array by index

    EXPECT_TRUE(staticArray.Resize(cNumItems).IsNone());

    for (size_t i = 0; i < staticArray.Size(); i++) {
        staticArray[i] = i;
    }

    // Check At

    for (size_t i = 0; i < bufferArray.Size(); i++) {
        auto ret1 = bufferArray.At(i);
        EXPECT_TRUE(ret1.mError.IsNone());

        auto ret2 = staticArray.At(i);
        EXPECT_TRUE(ret2.mError.IsNone());

        EXPECT_EQ(ret1.mValue, ret2.mValue);
    }

    // Range base loop

    auto i = 0;

    for (const auto& value : bufferArray) {
        EXPECT_EQ(value, staticArray[i++]);
    }

    // Check pop operation

    for (i = cNumItems - 1; i >= 0; i--) {
        auto ret1 = bufferArray.Back();
        EXPECT_TRUE(bufferArray.PopBack().IsNone());

        auto ret2 = staticArray.Back();
        EXPECT_TRUE(staticArray.PopBack().IsNone());

        EXPECT_EQ(ret1.mValue, ret2.mValue);
    }

    // Test const array

    static const int cArray[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    const auto constArray = Array<const int>(cArray, ArraySize(cArray));

    EXPECT_EQ(constArray.Size(), ArraySize(cArray));
    EXPECT_EQ(constArray.MaxSize(), ArraySize(cArray));

    i = 0;

    for (auto& value : constArray) {
        EXPECT_EQ(value, cArray[i++]);
    }
}

TEST(ArrayTest, Insert)
{
    StaticArray<int, 32> array;

    // Insert at the end

    int ins1[] = {8, 8, 8, 8, 8};

    array.Insert(array.end(), ins1, ins1 + ArraySize(ins1));
    EXPECT_EQ(array.Size(), ArraySize(ins1));
    EXPECT_EQ(memcmp(array.begin(), ins1, ArraySize(ins1)), 0);

    // Insert in the middle

    int ins2[] = {3, 3, 3};

    array.Insert(&array[2], ins2, ins2 + ArraySize(ins2));

    int ins3[] = {5, 5, 5, 5, 5};
    array.Insert(&array[6], ins3, ins3 + ArraySize(ins3));

    int result[] = {8, 8, 3, 3, 3, 8, 5, 5, 5, 5, 5, 8, 8};

    EXPECT_EQ(array.Size(), ArraySize(result));
    EXPECT_EQ(memcmp(array.begin(), result, ArraySize(result)), 0);
}

TEST(ArrayTest, Find)
{
    int inputArray[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    Array<int> array(inputArray, ArraySize(inputArray));

    // Found

    auto result = array.Find(4);
    EXPECT_TRUE(result.mError.IsNone());
    EXPECT_EQ(*result.mValue, 4);

    // Not found

    result = array.Find(13);
    EXPECT_TRUE(result.mError.Is(ErrorEnum::eNotFound));
    EXPECT_EQ(result.mValue, array.end());

    // Found matched

    result = array.FindIf([](const int& value) { return value == 8 ? true : false; });
    EXPECT_TRUE(result.mError.IsNone());
    EXPECT_EQ(*result.mValue, 8);
}

TEST(ArrayTest, Erase)
{
    int inputArray[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    Array<int> array(inputArray, ArraySize(inputArray));

    // Erase last element

    {
        auto result = array.Erase(&array[0]);
        EXPECT_EQ(result, &array[0]);

        int resultArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        EXPECT_EQ(array.Size(), ArraySize(resultArray));
        EXPECT_EQ(memcmp(array.begin(), resultArray, ArraySize(resultArray)), 0);
    }

    // Erase one element

    {
        auto result = array.Erase(&array[4]);
        EXPECT_EQ(result, &array[4]);

        int resultArray[] = {1, 2, 3, 5, 6, 7, 8, 9};
        EXPECT_EQ(array.Size(), ArraySize(resultArray));
        EXPECT_EQ(memcmp(array.begin(), resultArray, ArraySize(resultArray)), 0);
    }

    // Erase last element

    {
        auto result = array.Erase(&array[7]);
        EXPECT_EQ(result, array.end());

        int resultArray[] = {1, 2, 3, 5, 6, 7, 8};
        EXPECT_EQ(array.Size(), ArraySize(resultArray));
        EXPECT_EQ(memcmp(array.begin(), resultArray, ArraySize(resultArray)), 0);
    }
}

TEST(ArrayTest, Remove)
{
    int inputArray[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    Array<int> array(inputArray, ArraySize(inputArray));

    // Remove first element

    {
        auto result = array.Remove(0);
        EXPECT_EQ(result, 1);

        int resultArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        EXPECT_EQ(array.Size(), ArraySize(resultArray));
        EXPECT_EQ(memcmp(array.begin(), resultArray, ArraySize(resultArray)), 0);
    }

    // Remove one element

    {
        auto result = array.Remove(4);
        EXPECT_EQ(result, 1);

        int resultArray[] = {1, 2, 3, 5, 6, 7, 8, 9};
        EXPECT_EQ(array.Size(), ArraySize(resultArray));
        EXPECT_EQ(memcmp(array.begin(), resultArray, ArraySize(resultArray)), 0);
    }

    // Remove last element

    {
        auto result = array.Remove(9);
        EXPECT_EQ(result, 1);

        int resultArray[] = {1, 2, 3, 5, 6, 7, 8};
        EXPECT_EQ(array.Size(), ArraySize(resultArray));
        EXPECT_EQ(memcmp(array.begin(), resultArray, ArraySize(resultArray)), 0);
    }

    // Remove matching condition

    {
        auto result = array.RemoveIf([](const int& value) { return value == 6 ? true : false; });
        EXPECT_EQ(result, 1);

        int resultArray[] = {1, 2, 3, 5, 7, 8};
        EXPECT_EQ(array.Size(), ArraySize(resultArray));
        EXPECT_EQ(memcmp(array.begin(), resultArray, ArraySize(resultArray)), 0);
    }
}

TEST(ArrayTest, Struct)
{
    struct TestStruct {
        StaticArray<uint32_t, 32> arr1;
        StaticArray<uint32_t, 32> arr2;
    };

    StaticArray<TestStruct, 8> array;

    array.Resize(1);

    uint32_t test1[] = {0, 1, 2, 3, 4};
    uint32_t test2[] = {5, 6, 7, 8, 9};

    array[0].arr1 = Array<uint32_t>(test1, ArraySize(test1));
    array[0].arr2 = Array<uint32_t>(test1, ArraySize(test2));

    EXPECT_EQ(array[0].arr1, Array<uint32_t>(test1, ArraySize(test1)));
    EXPECT_EQ(array[0].arr2, Array<uint32_t>(test1, ArraySize(test1)));
}

TEST(ArrayTest, Sort)
{
    int intValues[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

    auto array = Array<int>(intValues, ArraySize(intValues));

    array.Sort();

    for (size_t i = 0; i < ArraySize(intValues); i++) {
        EXPECT_EQ(array[i], i);
    }

    array.Sort([](int a, int b) { return a > b; });

    for (size_t i = 0; i < ArraySize(intValues); i++) {
        EXPECT_EQ(array[i], ArraySize(intValues) - i - 1);
    }
}
