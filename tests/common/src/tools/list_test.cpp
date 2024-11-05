/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/list.hpp"

using namespace aos;

TEST(ListTest, Basic)
{
    constexpr auto cNumItems = 32;

    // Static list

    StaticList<int, cNumItems> staticList;

    EXPECT_EQ(staticList.Size(), 0);
    EXPECT_EQ(staticList.MaxSize(), cNumItems);

    // Fill list by push back

    for (size_t i = 0; i < cNumItems; i++) {
        EXPECT_TRUE(staticList.PushBack(i).IsNone());
    }

    EXPECT_EQ(staticList.Size(), cNumItems);
    EXPECT_TRUE(staticList.IsFull());

    // Add item to the end

    EXPECT_TRUE(staticList.PushBack(100).Is(ErrorEnum::eNoMemory));

    int expectedValue = 0;

    // Iterate over list
    for (auto& value : staticList) {
        EXPECT_EQ(value, expectedValue);

        expectedValue++;
    }

    // Clear list

    staticList.Clear();
    EXPECT_EQ(staticList.Size(), 0);

    // Fill list by push front

    for (size_t i = 0; i < cNumItems; i++) {
        EXPECT_TRUE(staticList.PushFront(i).IsNone());
    }

    EXPECT_EQ(staticList.Size(), cNumItems);
    EXPECT_TRUE(staticList.IsFull());

    auto constList(staticList);

    expectedValue = cNumItems - 1;

    for (const auto& value : constList) {
        EXPECT_EQ(value, expectedValue);

        expectedValue--;
    }

    // Check assign operator

    StaticList<int, cNumItems> assignList;

    assignList = constList;

    EXPECT_EQ(staticList.Size(), cNumItems);
    EXPECT_TRUE(staticList.IsFull());

    expectedValue = cNumItems - 1;

    for (const auto& value : assignList) {
        EXPECT_EQ(value, expectedValue);

        expectedValue--;
    }

    // Remove all odd items

    for (auto it = staticList.begin(); it != staticList.end();) {
        if (*it % 2) {
            it = staticList.Remove(it).mValue;
        } else {
            it++;
        }
    }

    expectedValue = cNumItems - 2;

    for (const auto& value : staticList) {
        EXPECT_EQ(value, expectedValue);

        expectedValue -= 2;
    }

    EXPECT_EQ(staticList.Size(), cNumItems / 2);

    // Check emplace

    auto [pos, err] = staticList.Find(10);

    EXPECT_TRUE(err.IsNone());

    EXPECT_TRUE(staticList.Emplace(pos, 100).IsNone());

    for (auto it = staticList.begin(); it != staticList.end(); --it) {
        if (*it == 10) {
            EXPECT_EQ(*(++it), 100);
        }
    }
}
