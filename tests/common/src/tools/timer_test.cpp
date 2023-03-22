/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/timer.hpp"

using namespace aos;

TEST(common, TimerCreateAndStop)
{

    auto       interrupted = 0;
    aos::Timer timer {};

    EXPECT_TRUE(timer.Create(900, [&interrupted](void*) { interrupted = 1; }).IsNone());

    sleep(1);

    EXPECT_TRUE(timer.Stop().IsNone());

    EXPECT_EQ(1, interrupted);
}

TEST(common, TimerRaisedOnlyOnce)
{
    auto       interrupted = 0;
    aos::Timer timer {};

    EXPECT_TRUE(timer.Create(500, [&interrupted](void*) { interrupted++; }).IsNone());

    sleep(2);

    EXPECT_TRUE(timer.Stop().IsNone());

    EXPECT_EQ(1, interrupted);
}

TEST(common, TimerCreateResetStop)
{
    auto       interrupted = 0;
    aos::Timer timer {};

    EXPECT_TRUE(timer.Create(2000, [&interrupted](void*) { interrupted = 1; }).IsNone());

    sleep(1);

    EXPECT_TRUE(timer.Reset([&interrupted](void*) { interrupted = 1; }).IsNone());

    sleep(1);

    EXPECT_TRUE(timer.Stop().IsNone());

    sleep(2);

    EXPECT_EQ(0, interrupted);
}
