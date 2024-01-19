/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/common/tools/optional.hpp"

using namespace aos;

TEST(OptionalTest, Reset)
{
    Optional<int> src = 0;

    ASSERT_TRUE(src.HasValue());

    src.Reset();

    ASSERT_FALSE(src.HasValue());
}

TEST(OptionalTest, SetValue)
{
    Optional<int> src;

    ASSERT_FALSE(src.HasValue());

    src.SetValue(42);

    ASSERT_TRUE(src.HasValue());

    ASSERT_EQ(src.GetValue(), 42);
}

TEST(OptionalTest, CallsDestructor)
{
    using testing::MockFunction;

    MockFunction<void()> callback;

    class Wrapper {
    public:
        explicit Wrapper(MockFunction<void()>* func)
            : mFunc(func)
        {
        }

        ~Wrapper() { mFunc->Call(); }

    private:
        MockFunction<void()>* mFunc;
    };

    EXPECT_CALL(callback, Call()).Times(1);

    {
        Optional<Wrapper> opt;
        opt.EmplaceValue(&callback);
    }
}
