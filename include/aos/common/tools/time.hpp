/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TIME_HPP_
#define AOS_TIME_HPP_

#include <time.h>

#include "aos/common/tools/array.hpp"
#include "aos/common/tools/log.hpp"
#include "aos/common/tools/string.hpp"

namespace aos {

/**
 * Base type for a time duration in nanoseconds. Can also be negative to set a point back in time.
 */
using Duration = int64_t;

/**
 * Returns time duration in years.
 *
 * @param num number of years.
 * @result Duration.
 */
constexpr Duration Years(int64_t num)
{
    constexpr Duration year = 31556925974700000;

    return year * num;
}

/**
 * An object specifying a time instant.
 */
class Time {
public:
    /**
     * Constructs object instance
     */
    Time()
        : mTime()
    {
    }

    /**
     * Returns current local time.
     *
     * @result Time.
     */
    static Time Now()
    {
        timespec time;

        auto ret = clock_gettime(CLOCK_REALTIME, &time);
        assert(ret == 0);

        return Time(time);
    }

    /**
     * Returns local time corresponding to Unix time.
     *
     * @param sec seconds since January 1, 1970 UTC.
     * @param nsec nano seconds part.
     * @result Ti
     */
    static Time Unix(int64_t sec, int64_t nsec) { return Time({sec, nsec}); }

    /**
     * Checks whether Time object is default initialized.
     *
     * @result bool.
     */
    bool IsZero() const { return *this == Time(); }

    /**
     * Returns a copy of the current object increased by a specified time duration.
     *
     * @param duration duration to be added to the current time instant.
     * @result Time.
     */
    Time Add(Duration duration) const
    {
        auto time = mTime;

        int64_t nsec = time.tv_nsec + duration;

        time.tv_sec += nsec / cNanoRatio;
        time.tv_nsec = nsec % cNanoRatio;

        // sign of the remainder implementation defined => correct if negative
        if (time.tv_nsec < 0) {
            time.tv_nsec += cNanoRatio;
            time.tv_sec--;
        }

        return Time(time);
    }

    /**
     * Returns an integer representing a time in nanoseconds elapsed since January 1, 1970 UTC.
     *
     * @result result time in nanoseconds.
     */
    uint64_t UnixNano() const
    {
        uint64_t nsec = mTime.tv_nsec + mTime.tv_sec * cNanoRatio;

        return nsec;
    }

    /**
     * Checks whether a current time is less than a specified one.
     *
     * @param obj time object to compare with.
     * @result bool.
     */
    bool operator<(const Time& obj) const
    {
        return mTime.tv_sec < obj.mTime.tv_sec
            || (mTime.tv_sec == obj.mTime.tv_sec && mTime.tv_nsec < obj.mTime.tv_nsec);
    }

    /**
     * Checks whether a current time and a specified object represent the same time instant.
     *
     * @param obj time object to compare with.
     * @result bool.
     */
    bool operator==(const Time& obj) const
    {
        return mTime.tv_sec == obj.mTime.tv_sec && mTime.tv_nsec == obj.mTime.tv_nsec;
    }

    /**
     * Checks whether a current time and a specified object don't represent the same time instant.
     *
     * @param obj time object to compare with.
     * @result bool.
     */
    bool operator!=(const Time& obj) const { return !operator==(obj); }

    /**
     * Prints time into log.
     *
     * @param log log object to print time into.
     * @param obj time object to be printed.
     * @result source stream object.
     */
    friend Log& operator<<(Log& log, const Time& obj)
    {
        static constexpr auto cTimeStrLen = 32;

        tm                        buf;
        StaticString<cTimeStrLen> utcTimeStr;

        auto time = gmtime_r(&obj.mTime.tv_sec, &buf);
        assert(time != nullptr);

        utcTimeStr.Resize(utcTimeStr.MaxSize());

        size_t size = strftime(utcTimeStr.Get(), utcTimeStr.Size(), "%FT%TZ", time);
        assert(size != 0);

        utcTimeStr.Resize(size - 1);

        log << utcTimeStr;

        return log;
    }

private:
    static constexpr int64_t cNanoRatio = 1000 * 1000 * 1000;

    explicit Time(timespec time)
        : mTime(time)
    {
    }

    timespec mTime;
};

} // namespace aos

#endif
