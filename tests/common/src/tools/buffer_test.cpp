/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/buffer.hpp"

using namespace aos;

TEST(BufferTest, Basic)
{
    constexpr auto cBufferSize = 256;

    StaticBuffer<cBufferSize> staticBufferA;
    EXPECT_EQ(staticBufferA.Size(), cBufferSize);

    StaticBuffer<cBufferSize> staticBufferB;
    EXPECT_EQ(staticBufferB.Size(), cBufferSize);

    strcpy(static_cast<char*>(staticBufferB.Get()), "test string");

    staticBufferA = staticBufferB;

    EXPECT_EQ(strcmp(static_cast<char*>(staticBufferA.Get()), static_cast<char*>(staticBufferB.Get())), 0);

    StaticBuffer<512> bufferC(staticBufferA);
}
