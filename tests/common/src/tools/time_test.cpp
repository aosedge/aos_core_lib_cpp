/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/common/tools/time.hpp"
#include "aos/test/log.hpp"

using namespace testing;
using namespace aos;

class TimeTest : public Test {
private:
    void SetUp() override { test::InitLog(); }
};

TEST_F(TimeTest, DurationToISO8601String)
{
    struct TestCase {
        Duration    duration;
        const char* expected;
    } testCases[] = {
        {Time::cDay * -6, "-P6D"},
        {-6 * Time::cDay, "-P6D"},
        {Time::cWeek, "P1W"},
        {Time::cWeek * 2, "P2W"},
        {Time::cWeek / 7, "P1D"},
        {7 / Time::cWeek, "P1D"},
        {Time::cWeek - Time::cDay, "P6D"},
        {Time::cMonth, "P1M"},
        {Time::cYear, "P1Y"},
        {Time::cYear + Time::cMonth + Time::cWeek + Time::cDay + Time::cHours, "P1Y1M1W1DT1H"},
        {Duration(0), "PT0S"},
        {Duration(1), "PT0.000000001S"},
        {Time::cMinutes + Time::cSeconds, "PT1M1S"},
        {Time::cMinutes + Time::cMicroseconds * 32, "PT1M0.000032000S"},
    };

    for (const auto& testCase : testCases) {
        LOG_DBG() << "Duration: " << testCase.duration;

        EXPECT_STREQ(testCase.duration.ToISO8601String().CStr(), testCase.expected);
    }
}

TEST_F(TimeTest, DurationParts)
{
    constexpr auto    cDays    = 8;
    constexpr auto    cHours   = cDays * 24 + 1;
    constexpr auto    cMinutes = cHours * 60 + 1;
    constexpr auto    cSeconds = cMinutes * 60 + 1;
    constexpr int64_t cMillis  = cSeconds * 1000 + 1;
    constexpr int64_t cMicros  = cMillis * 1000 + 1;
    constexpr int64_t cNanos   = cMicros * 1000 + 1;

    constexpr Duration duration = cNanos;

    // Test duration contains 1 milli, micro and nano second
    // that makes the seconds, minutes, hours to have fractional part
    constexpr float epsilon = 0.02;

    EXPECT_NEAR(duration.Hours(), cHours, epsilon);
    EXPECT_NEAR(duration.Minutes(), cMinutes, epsilon);
    EXPECT_NEAR(duration.Seconds(), cSeconds, epsilon);
    EXPECT_EQ(duration.Milliseconds(), cMillis);
    EXPECT_EQ(duration.Microseconds(), cMicros);
    EXPECT_EQ(duration.Nanoseconds(), cNanos);
}

TEST_F(TimeTest, Add4Years)
{
    Time now             = Time::Now();
    Time fourYearsLater  = now.Add(Years(4));
    Time fourYearsBefore = now.Add(Years(-4));

    LOG_INF() << "Time now: " << now;
    LOG_INF() << "Four years later: " << fourYearsLater;

    EXPECT_EQ(now.UnixNano() + Years(4).Nanoseconds(), fourYearsLater.UnixNano());
    EXPECT_EQ(now.UnixNano() + Years(-4).Nanoseconds(), fourYearsBefore.UnixNano());
}

TEST_F(TimeTest, Less)
{
    auto now = Time::Now();

    const Duration year       = Years(1);
    const Duration oneNanosec = 1;

    EXPECT_TRUE(now < now.Add(year));
    EXPECT_TRUE(now < now.Add(oneNanosec));

    EXPECT_FALSE(now.Add(oneNanosec) < now);
    EXPECT_FALSE(now < now);
}

TEST_F(TimeTest, More)
{
    auto now = Time::Now();

    const Duration year       = Years(1);
    const Duration oneNanosec = 1;

    EXPECT_TRUE(now.Add(year) > now);
    EXPECT_TRUE(now.Add(oneNanosec) > now);

    EXPECT_FALSE(now > now.Add(oneNanosec));
    EXPECT_FALSE(now > now);
}

TEST_F(TimeTest, GetDateTime)
{
    auto t = Time::Unix(1706702400);

    int day, month, year, hour, min, sec;

    EXPECT_TRUE(t.GetDate(&day, &month, &year).IsNone());
    EXPECT_TRUE(t.GetTime(&hour, &min, &sec).IsNone());

    EXPECT_EQ(day, 31);
    EXPECT_EQ(month, 1);
    EXPECT_EQ(year, 2024);
    EXPECT_EQ(hour, 12);
    EXPECT_EQ(min, 00);
    EXPECT_EQ(sec, 00);
}
