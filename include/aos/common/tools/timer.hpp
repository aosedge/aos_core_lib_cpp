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
     * @param intervalMs Timer interval in milliseconds.
     *
     * @return Error code.
     */
    template <typename F>
    Error Create(unsigned int intervalMs, F functor, bool oneShot = true, void* arg = nullptr)
    {
        aos::LockGuard lock(mMutex);

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

        mIntervalMs = intervalMs;
        mOneShot = oneShot;

        struct sigevent   sev { };
        struct itimerspec its { };

        sev.sigev_notify = cTimerSigevNotify;
        sev.sigev_value.sival_ptr = this;
        sev.sigev_notify_function = TimerFunction;

        its.it_value.tv_sec = intervalMs / 1000;
        its.it_value.tv_nsec = (intervalMs % 1000) * 1000000;

        if (!mOneShot) {
            its.it_interval.tv_sec = intervalMs / 1000;
            its.it_interval.tv_nsec = (intervalMs % 1000) * 1000000;
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
        aos::LockGuard lock(mMutex);

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

            err = Create(mIntervalMs, functor, mOneShot, arg);
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
            aos::LockGuard lock(timer->mMutex);
            if (timer->mStop) {
                return;
            }
        }

        timer->mFunction();
    }

    timer_t                                 mTimerID {};
    unsigned int                            mIntervalMs {};
    bool                                    mOneShot {};
    bool                                    mStop {};
    Mutex                                   mMutex;
    StaticFunction<cDefaultFunctionMaxSize> mFunction;
};

} // namespace aos

#endif
