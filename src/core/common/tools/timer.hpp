/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_TIMER_HPP_
#define AOS_CORE_COMMON_TOOLS_TIMER_HPP_

#include "config.hpp"
#include "function.hpp"
#include "thread.hpp"
#include "time.hpp"

namespace aos {

/**
 * Timer instance.
 * @tparam T timer callback type.
 */
class Timer {
public:
    /**
     * Timer stop mode.
     */
    enum class StopMode {
        NoWait, ///< Stop timer without waiting for running callbacks.
        WaitForCallbacks, ///< Wait for running callbacks and stop timer.
    };

    /**
     * Constructs timer instance.
     */
    Timer() = default;

    /**
     * Destructs timer instance.
     */
    ~Timer() { Stop(StopMode::WaitForCallbacks); }

    /**
     * Starts timer.
     *
     * @param interval timer interval.
     * @param callback callback.
     * @param oneShot specifies whether timer should be called exactly once.
     * @param arg callback argument.
     * @return Error code.
     */
    template <typename F>
    Error Start(Duration interval, F callback, bool oneShot = true, void* arg = nullptr)
    {
        if (interval <= cTimerResolution) {
            return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
        }

        if (auto err = Stop(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        const auto wrappedCallback = [this, callback](void* arg) {
            bool oneshot = false;
            bool stopped = false;

            {
                LockGuard lock {mMutex};

                stopped = mStopped;
                oneshot = mOneShot;
            }

            if (stopped) {
                ReleaseActiveCallback();

                return;
            }

            if (oneshot) {
                Stop();
            }

            callback(arg);

            ReleaseActiveCallback();
        };

        LockGuard lock {mMutex};

        mStopped = false;

        if (auto err = mFunction.Capture(wrappedCallback, arg); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        mInterval = interval;
        mOneShot  = oneShot;

        return RegisterTimer(this);
    }

    /**
     * Stops timer.
     *
     * @param mode specifies whether to wait for currently running callbacks.
     * @return Error code.
     */
    Error Stop(StopMode mode = StopMode::NoWait);

    /**
     * Restarts timer.
     *
     * @return Error code.
     */
    Error Restart()
    {
        if (auto err = Stop(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        {
            LockGuard lock {mMutex};

            if (!mFunction) {
                return ErrorEnum::eNone;
            }

            mStopped = false;
        }

        return RegisterTimer(this);
    }

private:
    void AcquireActiveCallback();
    void ReleaseActiveCallback();

    Duration                                mInterval {};
    bool                                    mOneShot {};
    bool                                    mStopped {};
    size_t                                  mActiveCallbacks {};
    StaticFunction<cDefaultFunctionMaxSize> mFunction;
    Time                                    mWakeupTime;
    Mutex                                   mMutex;
    ConditionalVariable                     mCondVar;

    // Set two threads for callbacks: in case if any executes for a long time, another will hedge.
    static constexpr auto     cInvocationThreadsCount = 2;
    static constexpr auto     cMaxTimersCount         = AOS_CONFIG_TOOLS_TIMERS_MAX_COUNT;
    static constexpr Duration cTimerResolution        = Time::cMicroseconds * 500;

    static Error RegisterTimer(Timer* timer);
    static Error UnregisterTimer(Timer* timer, StopMode mode);

    static Error StartThreads();
    static Error StopThreads(StopMode mode);

    static void ProcessTimers(void* arg);
    static void UpdateWakeupTime(const Time& now, Timer* timer);
    static void InvokeTimerCallback(Timer* timer);

    static StaticArray<Timer*, cMaxTimersCount> mRegisteredTimers;
    static Mutex                                mCommonMutex;
    static ConditionalVariable                  mCommonCondVar;

    static Thread<>                            mManagementThread;
    static ThreadPool<cInvocationThreadsCount> mInvocationThreads;
};

} // namespace aos

#endif
