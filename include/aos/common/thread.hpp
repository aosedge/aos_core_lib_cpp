/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_THREAD_HPP_
#define AOS_THREAD_HPP_

#include <pthread.h>

#include "aos/common/config/thread.hpp"
#include "aos/common/error.hpp"
#include "aos/common/noncopyable.hpp"

namespace aos {

constexpr auto cDefaultThreadStackSize = AOS_CONFIG_DEFAULT_THREAD_STACK_SIZE;

/**
 * Aos thread.
 */
template <size_t cStackSize = cDefaultThreadStackSize>
class Thread : private NonCopyable {
public:
    /**
     * Constructs Aos thread and use function pointer as argument.
     *
     * @param routine function to be called in thread.
     * @param arg optional argument that is passed to the thread function.
     */
    explicit Thread(void* (*func)(void*), void* arg = nullptr)
        : mAdapter(func)
        , mArg(arg)
        , mPThread(0)
    {
        mStack[0] = 0;
    }

    /**
     * Constructs Aos thread instance and use lambda as argument.
     *
     * @param routine function to be called in thread.
     * @param arg optional argument that is passed to the thread function.
     */
    template <typename T>
    explicit Thread(T routine)
        : mArg(nullptr)
        , mPThread(0)
    {
        static auto sRoutine = routine;

        mStack[0] = 0;

        mAdapter = [](void*) -> void* {
            sRoutine();

            return nullptr;
        };
    }

    /**
     * Runs thread function.
     *
     * @return Error.
     */
    Error Run()
    {
        pthread_attr_t attr;

        auto ret = pthread_attr_init(&attr);
        if (ret != 0) {
            return ret;
        }

        ret = pthread_attr_setstack(&attr, mStack, cStackSize);
        if (ret != 0) {
            return ret;
        }

        return pthread_create(&mPThread, &attr, mAdapter, mArg);
    }

    /**
     * Waits thread function is finished.
     *
     * @return Error.
     */
    Error Join() { return pthread_join(mPThread, nullptr); }

private:
    void* (*mAdapter)(void*);
    void*     mArg;
    uint8_t   mStack[cStackSize + sizeof(int) - cStackSize % sizeof(int)];
    pthread_t mPThread;
};

/**
 * Aos mutex.
 */
class Mutex : private NonCopyable {
public:
    /**
     * Constructs Aos mutex.
     */
    Mutex() { pthread_mutex_init(&mPMutex, nullptr); }

    /**
     * Destroys Aos mutex.
     */
    ~Mutex() { pthread_mutex_destroy(&mPMutex); }

    /**
     * Locks Aos mutex.
     *
     * @return Error.
     */
    Error Lock() { return pthread_mutex_lock(&mPMutex); }

    /**
     * Unlocks Aos mutex.
     *
     * @return Error.
     */
    Error Unlock() { return pthread_mutex_unlock(&mPMutex); }

    /**
     * Converts mutex to pthread_mutex_t pointer.
     *
     * @return pthread_mutex_t pointer.
     */

    explicit operator pthread_mutex_t*() { return &mPMutex; }

private:
    pthread_mutex_t mPMutex;
};

/**
 * Aos lock guard.
 */
class LockGuard : private NonCopyable {
public:
    /**
     * Creates lock guard instance.
     *
     * @param mutex mutex used to guard.
     */
    explicit LockGuard(Mutex& mutex)
        : mMutex(mutex)
    {
        mError = mMutex.Lock();
    }

    /**
     * Destroys lock guard instance.
     */
    ~LockGuard() { mMutex.Unlock(); }

    /**
     * Returns current lock guard error.
     *
     * @return Error.
     */
    Error GetError() { return mError; }

private:
    Mutex& mMutex;
    Error  mError;
};

/**
 * Aos unique lock.
 */
class UniqueLock : private NonCopyable {
public:
    /**
     * Creates unique lock instance.
     *
     * @param mutex mutex used to guard.
     */
    explicit UniqueLock(Mutex& mutex)
        : mMutex(mutex)
        , mIsLocked(false)
    {
        mError = Lock();
    }

    /**
     * Destroys unique lock instance.
     */
    ~UniqueLock()
    {
        if (mIsLocked) {
            Unlock();
        }
    }

    /**
     * Locks unique lock instance.
     */
    Error Lock()
    {
        mError = mMutex.Lock();
        if (!mError.IsNone()) {
            return mError;
        }

        mIsLocked = true;

        return mError;
    }

    /**
     * Unlocks unique lock instance.
     */
    Error Unlock()
    {
        mError = mMutex.Unlock();
        if (!mError.IsNone()) {
            return mError;
        }

        mIsLocked = false;

        return mError;
    }

    /**
     * Returns current unique lock error.
     *
     * @return Error.
     */
    Error GetError() { return mError; }

private:
    Mutex& mMutex;
    bool   mIsLocked;
    Error  mError;
};

/**
 * Aos conditional variable.
 */
class ConditionalVariable : private NonCopyable {
public:
    /**
     * Creates conditional variable.
     *
     * @param mutex conditional variable mutex.
     */
    explicit ConditionalVariable(Mutex& mutex)
        : mMutex(mutex)
    {
        mError = pthread_cond_init(&mCondVar, nullptr);
    }

    /**
     * Destroys conditional variable.
     */
    ~ConditionalVariable() { pthread_cond_destroy(&mCondVar); }

    /**
     * Blocks the current thread until the condition variable is awakened.
     *
     * @return Error.
     */
    Error Wait() { return mError = pthread_cond_wait(&mCondVar, static_cast<pthread_mutex_t*>(mMutex)); }

    /**
     * Blocks the current thread until the condition variable is awakened and predicate condition is met.
     *
     * @return Error.
     */
    template <typename T>
    Error Wait(T waitCondition)
    {
        while (!waitCondition()) {
            auto err = Wait();
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Notifies one waiting thread.
     *
     * @return Error.
     */
    Error NotifyOne() { return mError = pthread_cond_signal(&mCondVar); }

    /**
     * Notifies all waiting thread.
     *
     * @return Error.
     */
    Error NotifyAll() { return mError = pthread_cond_broadcast(&mCondVar); }

    /**
     * Returns current conditional variable error.
     *
     * @return Error.
     */
    Error GetError() { return mError; }

private:
    Mutex&         mMutex;
    pthread_cond_t mCondVar;
    Error          mError;
};

} // namespace aos

#endif
