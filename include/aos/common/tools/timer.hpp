/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TIMER_HPP_
#define AOS_TIMER_HPP_

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
     * @param callback callback.
     * @param oneShot specifies whether timer should be called exactly once.
     * @param arg callback argument.
     * @return Error code.
     */
    template <typename F>
    Error Create(Duration interval, F callback, bool oneShot = true, void* arg = nullptr)
    {
        if (interval <= cTimerResolution) {
            return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
        }

        Stop();

        LockGuard lock {mMutex};

        if (auto err = mFunction.Capture(callback, arg); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        mInterval = interval;
        mOneShot  = oneShot;

        return RegisterTimer(this);
    }

    /**
     * Stops timer.
     *
     * @return Error code.
     */
    Error Stop() { return UnregisterTimer(this); }

    /**
     * Resets timer.
     *
     * @return Error code.
     */
    template <typename F>
    Error Reset(F functor, void* arg = nullptr)
    {
        if (auto err = Create(mInterval, functor, mOneShot, arg); !err.IsNone()) {
            return err;
        }

        return ErrorEnum::eNone;
    }

private:
    Duration                                mInterval {};
    bool                                    mOneShot {};
    StaticFunction<cDefaultFunctionMaxSize> mFunction;
    Time                                    mWakeupTime;
    Mutex                                   mMutex;

    // Set two threads for callbacks: in case if any executes for a long time, another will hedge.
    static constexpr auto     cInvocationThreadsCount = 2;
    static constexpr auto     cMaxTimersCount         = AOS_CONFIG_TIMERS_MAX_COUNT;
    static constexpr Duration cTimerResolution        = Time::cMicroseconds * 500;

    static Error RegisterTimer(Timer* timer);
    static Error UnregisterTimer(Timer* timer);

    static Error StartThreads();
    static Error StopThreads();

    static void ProcessTimers(void* arg);
    static void UpdateWakeupTime(const Time& now, Timer* timer);
    static void InvokeTimerCallback(Timer* timer);

    static StaticArray<Timer*, cMaxTimersCount> mRegisteredTimers;
    static Mutex                                mCommonMutex;
    static ConditionalVariable                  mCommonCondVar;

    static Thread<cDefaultFunctionMaxSize, cDefaultThreadStackSize> mManagementThread;
    static ThreadPool<cInvocationThreadsCount>                      mInvocationThreads;
};

} // namespace aos

#endif
