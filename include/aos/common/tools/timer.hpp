/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TIMER_HPP_
#define AOS_TIMER_HPP_

#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "aos/common/tools/config.hpp"
#include "aos/common/tools/function.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/tools/time.hpp"

namespace aos {

/**
 * Timer instance.
 * @tparam T timer callback type.
 */
class Timer {
public:
    /**
     * Constructs timer instance.
     */
    Timer() = default;

    /**
     * Destructs timer instance.
     */
    ~Timer() { Stop(); }

    /**
     * Creates timer.
     *
     * @param interval timer interval.
     * @return Error code.
     */
    template <typename F>
    Error Create(Duration interval, F functor, bool oneShot = true, void* arg = nullptr)
    {
        LockGuard lock {mMutex};

        if (mTimerID != 0) {
            auto err = Stop();
            if (!err.IsNone()) {
                return err;
            }
        }

        mStop = false;

        auto err = mFunction.Capture(functor, arg);
        if (!err.IsNone()) {
            return err;
        }

        mInterval = interval;
        mOneShot  = oneShot;

        struct sigevent   sev { };
        struct itimerspec its { };

        sev.sigev_notify          = cTimerSigevNotify;
        sev.sigev_value.sival_ptr = this;
        sev.sigev_notify_function = TimerFunction;

        its.it_value.tv_sec  = interval / Time::cSeconds;
        its.it_value.tv_nsec = interval % Time::cSeconds;

        if (!mOneShot) {
            its.it_interval.tv_sec  = interval / Time::cSeconds;
            its.it_interval.tv_nsec = interval % Time::cSeconds;
        }

        auto ret = timer_create(CLOCK_MONOTONIC, &sev, &mTimerID);
        if (ret < 0) {
            return errno;
        }

        ret = timer_settime(mTimerID, 0, &its, nullptr);
        if (ret < 0) {
            return errno;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Stops timer.
     *
     * @return Error code.
     */
    Error Stop()
    {
        LockGuard lock {mMutex};

        mStop = true;

        if (mTimerID != 0) {
            auto ret = timer_delete(mTimerID);
            if (ret < 0) {
                return errno;
            }

            mTimerID = 0;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Resets timer.
     *
     * @return Error code.
     */
    template <typename F>
    Error Reset(F functor, void* arg = nullptr)
    {
        if (mTimerID != 0) {
            auto err = Stop();
            if (!err.IsNone()) {
                return err;
            }

            err = Create(mInterval, functor, mOneShot, arg);
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

private:
    static constexpr int cTimerSigevNotify = AOS_CONFIG_TIMER_SIGEV_NOTIFY;

    static void TimerFunction(union sigval arg)
    {
        auto timer = static_cast<Timer*>(arg.sival_ptr);

        {
            LockGuard lock(timer->mMutex);
            if (timer->mStop) {
                return;
            }
        }

        timer->mFunction();
    }

    timer_t                                 mTimerID {};
    Duration                                mInterval {};
    bool                                    mOneShot {};
    bool                                    mStop {};
    Mutex                                   mMutex;
    StaticFunction<cDefaultFunctionMaxSize> mFunction;
};

} // namespace aos

#endif
