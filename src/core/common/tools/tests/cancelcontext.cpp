/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <future>

#include <gmock/gmock.h>

#include <core/common/tools/cancelcontext.hpp>

using namespace aos;
using namespace testing;

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST(CancelContextTest, Cancel)
{
    CancelContext ctx;

    EXPECT_FALSE(ctx.IsCanceled());
    EXPECT_TRUE(ctx.Err().IsNone());

    ASSERT_TRUE(ctx.Cancel().IsNone());

    EXPECT_TRUE(ctx.IsCanceled());
    EXPECT_TRUE(ctx.Err().Is(ErrorEnum::eCanceled));

    // Second cancel does nothing.
    EXPECT_TRUE(ctx.Cancel().IsNone());
    EXPECT_TRUE(ctx.IsCanceled());
}

TEST(CancelContextTest, WaitTimeout)
{
    CancelContext ctx;

    const auto start = std::chrono::steady_clock::now();

    EXPECT_TRUE(ctx.Wait(100 * Time::cMilliseconds).Is(ErrorEnum::eTimeout));
    EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(100));
}

TEST(CancelContextTest, WaitCanceled)
{
    CancelContext ctx;

    ASSERT_TRUE(ctx.Cancel().IsNone());

    // Wait on the canceled context returns immediately.
    EXPECT_TRUE(ctx.Wait().Is(ErrorEnum::eCanceled));
    EXPECT_TRUE(ctx.Wait(10 * Time::cSeconds).Is(ErrorEnum::eCanceled));
}

TEST(CancelContextTest, CancelWakesUpWaiters)
{
    CancelContext ctx;

    auto waitFuture        = std::async(std::launch::async, [&ctx]() { return ctx.Wait(); });
    auto waitTimeoutFuture = std::async(std::launch::async, [&ctx]() { return ctx.Wait(10 * Time::cSeconds); });

    EXPECT_EQ(waitFuture.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);

    const auto start = std::chrono::steady_clock::now();

    ASSERT_TRUE(ctx.Cancel().IsNone());

    EXPECT_TRUE(waitFuture.get().Is(ErrorEnum::eCanceled));
    EXPECT_TRUE(waitTimeoutFuture.get().Is(ErrorEnum::eCanceled));
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
}

TEST(CancelContextTest, Reset)
{
    CancelContext ctx;

    ASSERT_TRUE(ctx.Cancel().IsNone());

    ctx.Reset();

    EXPECT_FALSE(ctx.IsCanceled());
    EXPECT_TRUE(ctx.Err().IsNone());
    EXPECT_TRUE(ctx.Wait(10 * Time::cMilliseconds).Is(ErrorEnum::eTimeout));

    // The context can be canceled again.
    ASSERT_TRUE(ctx.Cancel().IsNone());

    EXPECT_TRUE(ctx.IsCanceled());
}
