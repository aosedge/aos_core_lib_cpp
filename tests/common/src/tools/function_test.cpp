/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/function.hpp"

using namespace aos;

TEST(FunctionTest, Basic)
{
    uint32_t value = 0;

    // Default function

    StaticFunction<32> func1;

    EXPECT_FALSE(func1().IsNone());

    // Assign functor

    func1 = [&value](void*) { value = 1; };

    func1();

    EXPECT_EQ(value, 1);

    // Capture

    func1.Capture([&value](void*) { value = 2; });

    func1();

    EXPECT_EQ(value, 2);

    // Copy constructor

    StaticFunction<32> func2([&value](void*) { value = 3; });

    func2();

    EXPECT_EQ(value, 3);

    // Assign operator

    func2 = func1;

    func2();

    EXPECT_EQ(value, 2);
}
