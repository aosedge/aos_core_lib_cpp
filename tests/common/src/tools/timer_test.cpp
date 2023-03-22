/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/timer.hpp"

using namespace aos;

TEST(common, CreateAndStop)
{

    auto       g_interrupted = 0;
    aos::Timer timer {};

    EXPECT_TRUE(timer.Create(900, [&g_interrupted](void*) { g_interrupted = 1; }).IsNone());

    sleep(1);

    EXPECT_TRUE(timer.Stop().IsNone());

    EXPECT_EQ(1, g_interrupted);
}

TEST(common, RaisedOnlyOnce)
{
    auto       g_interrupted = 0;
    aos::Timer timer {};

    EXPECT_TRUE(timer.Create(500, [&g_interrupted](void*) { g_interrupted++; }).IsNone());

    sleep(2);

    EXPECT_TRUE(timer.Stop().IsNone());

    EXPECT_EQ(1, g_interrupted);
}

TEST(common, CreateResetStop)
{
    auto       g_interrupted = 0;
    aos::Timer timer {};

    EXPECT_TRUE(timer.Create(2000, [&g_interrupted](void*) { g_interrupted = 1; }).IsNone());

    sleep(1);

    EXPECT_TRUE(timer.Reset([&g_interrupted](void*) { g_interrupted = 1; }).IsNone());

    sleep(1);

    EXPECT_TRUE(timer.Stop().IsNone());

    sleep(2);

    EXPECT_EQ(0, g_interrupted);
}
