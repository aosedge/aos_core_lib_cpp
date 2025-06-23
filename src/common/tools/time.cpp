/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(__ZEPHYR__)
#include <zephyr/sys/timeutil.h>
#endif

#include "aos/common/tools/time.hpp"

namespace aos {

namespace {

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

constexpr auto cUTCFormat = "%Y-%m-%dT%H:%M:%SZ";

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

const char* ConsumeChars(const char* s, char* dest, size_t cnt)
{
    if (strnlen(s, cnt) < cnt) {
        return nullptr;
    }

    memcpy(dest, s, cnt);
    dest[cnt] = '\0';

    return s + cnt;
}

const char* ConsumeChar(const char* s, char ch)
{
    if (*s != ch) {
        return nullptr;
    }

    return ++s;
}

const char* ConsumeDate(const char* s, struct tm* tm_time)
{
    char year[4 + 1];
    char month[2 + 1];
    char day[2 + 1];

    s = ConsumeChars(s, year, 4);
    if (!s) {
        return nullptr;
    }

    s = ConsumeChar(s, '-');
    if (!s) {
        return nullptr;
    }

    s = ConsumeChars(s, month, 2);
    if (!s) {
        return nullptr;
    }

    s = ConsumeChar(s, '-');
    if (!s) {
        return nullptr;
    }

    s = ConsumeChars(s, day, 2);
    if (!s) {
        return nullptr;
    }

    tm_time->tm_year = atoi(year) - 1900;
    tm_time->tm_mon  = atoi(month) - 1;
    tm_time->tm_mday = atoi(day);

    return s;
}

const char* ConsumeTime(const char* s, struct tm* tm_time)
{
    char hour[2 + 1];
    char minute[2 + 1];
    char second[2 + 1];

    s = ConsumeChars(s, hour, 2);
    if (!s) {
        return nullptr;
    }

    s = ConsumeChar(s, ':');
    if (!s) {
        return nullptr;
    }

    s = ConsumeChars(s, minute, 2);
    if (!s) {
        return nullptr;
    }

    s = ConsumeChar(s, ':');
    if (!s) {
        return nullptr;
    }

    s = ConsumeChars(s, second, 2);
    if (!s) {
        return nullptr;
    }

    tm_time->tm_hour = atoi(hour);
    tm_time->tm_min  = atoi(minute);
    tm_time->tm_sec  = atoi(second);

    return s;
}

Error ParseDateTime(const char* s, const char* format, struct tm* tm_time)
{
    if (strcmp(format, cUTCFormat) != 0) {
        return Error(ErrorEnum::eInvalidArgument, "unsupported format");
    }

    s = ConsumeDate(s, tm_time);
    if (!s) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "failed to parse date"));
    }

    s = ConsumeChar(s, 'T');
    if (!s) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "failed to parse date"));
    }

    s = ConsumeTime(s, tm_time);
    if (!s) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "failed to parse time"));
    }

    return ErrorEnum::eNone;
}

} // namespace

/***********************************************************************************************************************
 * Duration
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

StaticString<cTimeStrLen> Duration::ToISO8601String() const
{
    if (mDuration == 0) {
        return "PT0S";
    }

    StaticString<cTimeStrLen> result = (mDuration < 0) ? "-P" : "P";

    auto total = llabs(mDuration);
    char buffer[16];

    if (auto years = total / Time::cYear.Nanoseconds(); years > 0) {
        snprintf(buffer, sizeof(buffer), "%lldY", years);

        result.Append(buffer);

        total %= Time::cYear.Nanoseconds();
    }

    if (auto months = total / Time::cMonth.Nanoseconds(); months > 0) {
        snprintf(buffer, sizeof(buffer), "%lldM", months);

        result.Append(buffer);
        total %= Time::cMonth.Nanoseconds();
    }

    if (auto weeks = total / Time::cWeek.Nanoseconds(); weeks > 0) {
        snprintf(buffer, sizeof(buffer), "%lldW", weeks);

        result.Append(buffer);

        total %= Time::cWeek.Nanoseconds();
    }

    if (auto days = total / Time::cDay.Nanoseconds(); days > 0) {
        snprintf(buffer, sizeof(buffer), "%lldD", days);

        result.Append(buffer);

        total %= Time::cDay.Nanoseconds();
    }

    const auto hours = total / Time::cHours.Nanoseconds();
    total %= Time::cHours.Nanoseconds();

    const auto minutes = total / Time::cMinutes.Nanoseconds();
    total %= Time::cMinutes.Nanoseconds();

    auto seconds = total / Time::cSeconds.Nanoseconds();
    total %= Time::cSeconds.Nanoseconds();

    if (hours || minutes || seconds || total) {
        result.Append("T");

        if (hours) {
            snprintf(buffer, sizeof(buffer), "%lldH", hours);
            result.Append(buffer);
        }

        if (minutes) {
            snprintf(buffer, sizeof(buffer), "%lldM", minutes);
            result.Append(buffer);
        }

        if (total == 0 && seconds > 0) {
            snprintf(buffer, sizeof(buffer), "%lldS", seconds);
            result.Append(buffer);
        }

        if (total > 0) {
            const auto rest = static_cast<double>(total) / Time::cSeconds.Nanoseconds() + static_cast<double>(seconds);

            snprintf(buffer, sizeof(buffer), "%0.9lfS", rest);
            result.Append(buffer);
        }
    }

    return result;
}

/***********************************************************************************************************************
 * Time
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

RetWithError<Time> Time::UTC(const String& utcTimeStr)
{
    struct tm timeInfo = {};

    if (auto err = ParseDateTime(utcTimeStr.CStr(), cUTCFormat, &timeInfo); !err.IsNone()) {
        return {{}, err};
    }

#if defined(__ZEPHYR__)
    auto seconds = timeutil_timegm(&timeInfo);
#else
    auto seconds = timegm(&timeInfo);
#endif

    return Time::Unix(seconds);
}

RetWithError<StaticString<cTimeStrLen>> Time::ToUTCString() const
{
    tm                        buf;
    StaticString<cTimeStrLen> utcTimeStr;

    auto unixTime = UnixTime();

    auto time = gmtime_r(&unixTime.tv_sec, &buf);

    utcTimeStr.Resize(utcTimeStr.MaxSize());

    size_t size = strftime(utcTimeStr.Get(), utcTimeStr.Size(), "%FT%TZ", time);
    if (size == 0) {
        return {{}, Error(ErrorEnum::eFailed, "failed to format time")};
    }

    if (auto err = utcTimeStr.Resize(size); !err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    return utcTimeStr;
}

} // namespace aos
