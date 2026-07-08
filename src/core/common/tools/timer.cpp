/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "timer.hpp"
#include "logger.hpp"

namespace aos {

/***********************************************************************************************************************
 * Static fields
 **********************************************************************************************************************/

StaticArray<Timer*, Timer::cMaxTimersCount> Timer::mRegisteredTimers;
Mutex                                       Timer::mCommonMutex;
ConditionalVariable                         Timer::mCommonCondVar;

Thread<>                                   Timer::mManagementThread;
ThreadPool<Timer::cInvocationThreadsCount> Timer::mInvocationThreads;

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Timer::Stop(StopMode mode)
{
    {
        LockGuard lock {mMutex};

        mStopped = true;
    }

    if (auto err = UnregisterTimer(this, mode); !err.IsNone()) {
        return err;
    }

    if (mode == StopMode::NoWait) {
        return ErrorEnum::eNone;
    }

    UniqueLock lock {mMutex};

    if (auto err = mCondVar.Wait(lock, [this]() { return mActiveCallbacks == 0; }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Timer::AcquireActiveCallback()
{
    LockGuard lock {mMutex};

    mActiveCallbacks++;
}

void Timer::ReleaseActiveCallback()
{
    LockGuard lock {mMutex};

    mActiveCallbacks--;

    if (mActiveCallbacks == 0) {
        mCondVar.NotifyAll();
    }
}

Error Timer::RegisterTimer(Timer* timer)
{
    LockGuard lock {mCommonMutex};

    timer->mWakeupTime = Time::Now().Add(timer->mInterval);

    if (auto err = mRegisteredTimers.PushBack(timer); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mCommonCondVar.NotifyAll(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mRegisteredTimers.Size() == 1) {
        return StartThreads();
    }

    return ErrorEnum::eNone;
}

Error Timer::UnregisterTimer(Timer* timer, StopMode mode)
{
    bool stopThreads = false;

    {
        LockGuard lock {mCommonMutex};

        if (mRegisteredTimers.Remove(timer) == 0) {
            return ErrorEnum::eNone;
        }

        timer->mWakeupTime = Time();

        if (auto err = mCommonCondVar.NotifyAll(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        stopThreads = mRegisteredTimers.IsEmpty();
    }

    if (stopThreads) {
        return StopThreads(mode);
    }

    return ErrorEnum::eNone;
}

Error Timer::StartThreads()
{
    if (auto err = mManagementThread.Run(&Timer::ProcessTimers); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInvocationThreads.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Timer::StopThreads(StopMode mode)
{
    if (auto err = mManagementThread.Join(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInvocationThreads.Shutdown(mode == StopMode::WaitForCallbacks); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Timer::InvokeTimerCallback(Timer* timer)
{
    timer->AcquireActiveCallback();

    if (auto err = mInvocationThreads.AddTask(timer->mFunction); !err.IsNone()) {
        timer->ReleaseActiveCallback();

        LOG_ERR() << "Invoke timer callback failure: err=" << AOS_ERROR_WRAP(err);
    }
}

void Timer::ProcessTimers(void* arg)
{
    (void)arg;

    auto cmpWakeupTime = [](const Timer* left, const Timer* right) {
        if (left->mWakeupTime.IsZero()) {
            return false;
        }

        if (right->mWakeupTime.IsZero()) {
            return true;
        }

        return left->mWakeupTime < right->mWakeupTime;
    };

    while (true) {
        UniqueLock lock {mCommonMutex};

        if (mRegisteredTimers.IsEmpty()) {
            break;
        }

        mRegisteredTimers.Sort(cmpWakeupTime);

        auto now               = Time::Now();
        auto cbInvokeThreshold = Time(now).Add(cTimerResolution);

        for (const auto& timer : mRegisteredTimers) {
            if (timer->mWakeupTime.IsZero()) {
                break;
            }

            if (timer->mWakeupTime > cbInvokeThreshold) {
                break;
            }

            InvokeTimerCallback(timer);

            if (timer->mOneShot) {
                timer->mWakeupTime = Time();
            } else {
                timer->mWakeupTime = Time(now).Add(timer->mInterval);
            }
        }

        auto min = mRegisteredTimers.Min(cmpWakeupTime);
        if (min != mRegisteredTimers.end() && !(*min)->mWakeupTime.IsZero()) {
            mCommonCondVar.Wait(lock, (*min)->mWakeupTime);
        } else {
            mCommonCondVar.Wait(lock);
        }
    }
}

} // namespace aos
