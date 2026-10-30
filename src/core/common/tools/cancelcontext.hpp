/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_CANCELCONTEXT_HPP_
#define AOS_CORE_COMMON_TOOLS_CANCELCONTEXT_HPP_

#include "error.hpp"
#include "noncopyable.hpp"
#include "thread.hpp"
#include "time.hpp"

namespace aos {

/**
 * Cancel context. It is passed to long operations, so that they can be canceled, similar to the Go context. The
 * operation checks the context periodically or waits on it instead of sleeping.
 */
class CancelContext : private NonCopyable {
public:
    /**
     * Creates cancel context.
     */
    CancelContext() = default;

    /**
     * Cancels the context and wakes up the waiters. Calling Cancel on the canceled context does nothing.
     *
     * @return Error.
     */
    Error Cancel()
    {
        LockGuard lock {mMutex};

        if (mCanceled) {
            return ErrorEnum::eNone;
        }

        mCanceled = true;

        if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
            return err;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns whether the context is canceled.
     *
     * @return bool.
     */
    bool IsCanceled() const
    {
        LockGuard lock {mMutex};

        return mCanceled;
    }

    /**
     * Returns context error.
     *
     * @return Error: eCanceled if the context is canceled, eNone otherwise.
     */
    Error Err() const
    {
        LockGuard lock {mMutex};

        if (mCanceled) {
            return Error(ErrorEnum::eCanceled, "context canceled");
        }

        return ErrorEnum::eNone;
    }

    /**
     * Waits until the context is canceled.
     *
     * @return Error: eCanceled if the context is canceled.
     */
    Error Wait()
    {
        UniqueLock lock {mMutex};

        if (auto err = mCondVar.Wait(lock, [this]() { return mCanceled; }); !err.IsNone()) {
            return err;
        }

        return Error(ErrorEnum::eCanceled, "context canceled");
    }

    /**
     * Waits until the context is canceled or the timeout expires.
     *
     * @param timeout wait timeout.
     * @return Error: eCanceled if the context is canceled, eTimeout if the timeout expired.
     */
    Error Wait(Duration timeout)
    {
        UniqueLock lock {mMutex};

        if (auto err = mCondVar.Wait(lock, timeout, [this]() { return mCanceled; }); !err.IsNone()) {
            return err;
        }

        return Error(ErrorEnum::eCanceled, "context canceled");
    }

    /**
     * Resets the canceled context, so it can be used again. Must be called when the context is not used by any
     * operation.
     */
    void Reset()
    {
        LockGuard lock {mMutex};

        mCanceled = false;
    }

private:
    mutable Mutex       mMutex;
    ConditionalVariable mCondVar;
    bool                mCanceled {};
};

} // namespace aos

#endif
