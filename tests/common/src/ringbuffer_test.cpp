/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/ringbuffer.hpp"

using namespace aos;

TEST(common, LinearRingBuffer)
{
    LinearRingBuffer<30> ringBuffer;

    EXPECT_TRUE(ringBuffer.IsEmpty());

    for (auto j = 0; j < 100; j++) {
        // Write to buffer

        uint32_t i = 0;

        for (; i < ringBuffer.MaxSize() / sizeof(i); i++) {
            EXPECT_TRUE(ringBuffer.Push(&i).IsNone());
        }

        EXPECT_EQ(ringBuffer.Size(), i * sizeof(i));

        // No more space

        EXPECT_FALSE(ringBuffer.Push(&i).IsNone());

        // Read from buffer

        i = 0;

        for (; i < ringBuffer.MaxSize() / sizeof(i); i++) {
            uint32_t value;

            EXPECT_TRUE(ringBuffer.Pop(&value).IsNone());
            EXPECT_EQ(i, value);
        }
    }

    // check buffer clear

    ringBuffer.Clear();
    EXPECT_TRUE(ringBuffer.IsEmpty());
}
