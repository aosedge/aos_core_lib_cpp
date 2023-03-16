/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/buffer.hpp"

using namespace aos;

TEST(common, Buffer)
{
    constexpr auto cBufferSize = 256;

    DynamicBuffer bufferA(cBufferSize);
    EXPECT_EQ(bufferA.Size(), cBufferSize);

    StaticBuffer<cBufferSize> staticBufferA;
    EXPECT_EQ(staticBufferA.Size(), cBufferSize);

    DynamicBuffer bufferB(32);

    strcpy(static_cast<char*>(bufferB.Get()), "test string");

    bufferA = bufferB;

    EXPECT_EQ(strcmp(static_cast<char*>(bufferA.Get()), static_cast<char*>(bufferB.Get())), 0);
}
