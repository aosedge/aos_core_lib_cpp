/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/common/tools/time.hpp"
#include "log.hpp"

using namespace testing;
using namespace aos;

class TimeTest : public Test {
private:
    void SetUp() override
    {
        Log::SetCallback([](const char* module, LogLevel level, const String& message) {
            static std::mutex sLogMutex;

            std::lock_guard<std::mutex> lock(sLogMutex);

            std::cout << level.ToString().CStr() << " | " << module << " | " << message.CStr() << std::endl;
        });
    }
};

TEST_F(TimeTest, Add4Years)
{
    Time now             = Time::Now();
    Time fourYearsLater  = now.Add(Years(4));
    Time fourYearsBefore = now.Add(Years(-4));

    LOG_INF() << "Time now: " << now;
    LOG_INF() << "Four years later: " << fourYearsLater;

    EXPECT_EQ(now.UnixNano() + Years(4), fourYearsLater.UnixNano());
    EXPECT_EQ(now.UnixNano() + Years(-4), fourYearsBefore.UnixNano());
}

TEST_F(TimeTest, Compare)
{
    auto now = Time::Now();

    const Duration year       = Years(1);
    const Duration oneNanosec = 1;

    EXPECT_TRUE(now < now.Add(year));
    EXPECT_TRUE(now < now.Add(oneNanosec));

    EXPECT_FALSE(now.Add(oneNanosec) < now);
    EXPECT_FALSE(now < now);
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

TEST_F(TimeTest, ToString)
{
    auto t = Time::Unix(1706702400);

    Error                     err;
    StaticString<cTimeStrLen> str;

    Tie(str, err) = t.ToString();

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(str, "20240131120000");
}
