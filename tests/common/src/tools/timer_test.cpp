/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/common/tools/timer.hpp"

using namespace aos;
using namespace testing;

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {
std::function<void(void*)> WrapCallback(MockFunction<void(void*)>& cb)
{
    return [&cb](void* arg) { cb.Call(arg); };
}

MATCHER_P(ApproxEqualTime, expected, "")
{
    Duration tolerance = 1 * Time::cMilliseconds;
    auto     diff      = static_cast<int64_t>(arg.UnixNano()) - static_cast<int64_t>(expected.UnixNano());

    std::cout << "arg: " << arg.UnixNano() << ", exp: " << expected.UnixNano() << std::endl;

    return std::abs(diff) < tolerance;
}

} // namespace

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST(TimerTest, RunOneShot)
{
    Timer                     timer {};
    MockFunction<void(void*)> cb;
    Time                      invokeTime {};

    auto       now      = Time::Now();
    const auto cTimeout = 900 * Time::cMilliseconds;

    EXPECT_CALL(cb, Call(_)).WillOnce(InvokeWithoutArgs([&invokeTime]() { invokeTime = Time::Now(); }));

    EXPECT_TRUE(timer.Create(cTimeout, WrapCallback(cb), true).IsNone());
    sleep(1);

    EXPECT_THAT(invokeTime, ApproxEqualTime(Time(now).Add(cTimeout)));

    EXPECT_TRUE(timer.Stop().IsNone());
}

TEST(TimerTest, RunMultiShot)
{
    Timer                     timer {};
    MockFunction<void(void*)> cb;
    Time                      invokeTime {};

    auto       now      = Time::Now();
    const auto cTimeout = 300 * Time::cMilliseconds;

    std::vector<Time>       invokeTimes = {};
    const std::vector<Time> expInvTimes
        = {Time(now).Add(cTimeout), Time(now).Add(cTimeout * 2), Time(now).Add(cTimeout * 3)};

    EXPECT_CALL(cb, Call(_)).Times(3).WillRepeatedly(InvokeWithoutArgs([&invokeTimes]() {
        invokeTimes.push_back(Time::Now());
    }));

    EXPECT_TRUE(timer.Create(cTimeout, WrapCallback(cb), false).IsNone());
    sleep(1);

    EXPECT_THAT(invokeTimes,
        ElementsAre(ApproxEqualTime(expInvTimes[0]), ApproxEqualTime(expInvTimes[1]), ApproxEqualTime(expInvTimes[2])));

    EXPECT_TRUE(timer.Stop().IsNone());
}

TEST(TimerTest, CreateResetStop)
{
    auto       interrupted = 0;
    aos::Timer timer {};

    EXPECT_TRUE(timer.Create(2000 * Time::cMilliseconds, [&interrupted](void*) { interrupted = 1; }).IsNone());

    sleep(1);

    EXPECT_TRUE(timer.Reset([&interrupted](void*) { interrupted = 1; }).IsNone());

    sleep(1);

    EXPECT_TRUE(timer.Stop().IsNone());

    sleep(2);

    EXPECT_EQ(0, interrupted);
}
