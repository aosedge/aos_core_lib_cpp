/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/queue.hpp"

using namespace aos;

TEST(CommonTest, Queue)
{
    StaticQueue<uint32_t, 30> queue;

    EXPECT_TRUE(queue.IsEmpty());

    // Push to queue

    for (auto j = 0; j < 100; j++) {
        uint32_t i = 0;

        for (; i < queue.MaxSize(); i++) {
            EXPECT_TRUE(queue.Push(i).IsNone());
        }

        EXPECT_EQ(queue.Size(), i);

        // No more space

        EXPECT_FALSE(queue.Push(i).IsNone());

        // Pop from queue

        i = 0;

        for (; i < queue.MaxSize(); i++) {
            auto result = queue.Front();
            EXPECT_TRUE(result.mError.IsNone());

            EXPECT_TRUE(queue.Pop().IsNone());
            EXPECT_EQ(i, result.mValue);
        }
    }

    // check queue clear

    queue.Clear();
    EXPECT_TRUE(queue.IsEmpty());
}
