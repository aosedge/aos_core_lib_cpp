/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_THREAD_HPP_
#define AOS_THREAD_HPP_

#include <assert.h>
#include <cstdint>
#include <pthread.h>

#include "aos/common/tools/config.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/function.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/queue.hpp"

namespace aos {

/**
 * Default tread stack size.
 */
constexpr auto cDefaultThreadStackSize = AOS_CONFIG_THREAD_DEFAULT_STACK_SIZE;

/**
 * Configures thread stack alignment.
 */
constexpr auto cThreadStackAlign = AOS_CONFIG_THREAD_STACK_ALIGN;

/**
 * Default thread pool queue size.
 */
constexpr auto cDefaultThreadPoolQueueSize = AOS_CONFIG_THREAD_POOL_DEFAULT_QUEUE_SIZE;

/**
 * Aos thread.
 */
template <size_t cFunctionMaxSize = cDefaultFunctionMaxSize, size_t cStackSize = cDefaultThreadStackSize>
class Thread : private NonCopyable {
public:
    // cppcheck-suppress uninitMemberVar
    /**
     * Constructs Aos thread instance and use lambda as argument.
     */
    Thread()
        : mPThread()
    {
    }

    /**
     * Runs thread function.
     *
     * @param functor function to be called in thread.
     * @param arg optional argument that is passed to the thread function.
     * @return Error.
     */
    template <typename T>
    Error Run(T functor, void* arg = nullptr)
    {
        auto err = mFunction.Capture(functor, arg);
        if (!err.IsNone()) {
            return err;
        }

        pthread_attr_t attr;

        auto ret = pthread_attr_init(&attr);
        if (ret != 0) {
            return ret;
        }

        ret = pthread_attr_setstack(&attr, mStack, ArraySize(mStack));
        if (ret != 0) {
            return ret;
        }

        return pthread_create(&mPThread, &attr, ThreadFunction, this);
    }

    /**
     * Waits thread function is finished.
     *
     * @return Error.
     */
    Error Join() { return pthread_join(mPThread, nullptr); }

private:
    alignas(cThreadStackAlign) uint8_t mStack[AlignedSize(cStackSize, cThreadStackAlign)];
    StaticFunction<cFunctionMaxSize> mFunction;
    pthread_t                        mPThread;

    static void* ThreadFunction(void* arg)
    {
        static_cast<Thread*>(arg)->mFunction();
        static_cast<Thread*>(arg)->mFunction.Reset();

        return nullptr;
    }
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
        if (!(mError = mMutex.Lock()).IsNone()) {
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
        if (!(mError = mMutex.Unlock()).IsNone()) {
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

/**
 * Aos thread pool.
 *
 * @tparam cNumThreads number of threads used to perform tasks.
 * @tparam cThreadStackSize stack size of threads used to perform tasks.
 * @tparam cQueueSize tasks queue size.
 * @tparam cMaxTaskSize max task size.
 */
template <size_t cNumThreads = 1, size_t cQueueSize = cDefaultThreadPoolQueueSize,
    size_t cMaxTaskSize = cDefaultFunctionMaxSize, size_t cThreadStackSize = cDefaultThreadStackSize>
class ThreadPool : private NonCopyable {
public:
    /**
     * Creates thread pool instance.
     */
    ThreadPool()
        : mTaskCondVar(mMutex)
        , mWaitCondVar(mMutex)
        , mShutdown(false)
        , mPendingTaskCount(0)
    {
    }

    /**
     * Adds task to task queue.
     *
     * @tparam T task type.
     * @param functor task functor.
     * @param arg argument passed to task functor when executed.
     * @return Error.
     */
    template <typename T>
    Error AddTask(T functor, void* arg = nullptr)
    {
        LockGuard lock(mMutex);

        auto err = mQueue.Push(Function());
        if (!err.IsNone()) {
            return err;
        }

        mPendingTaskCount++;

        err = mTaskCondVar.NotifyOne();
        if (!err.IsNone()) {
            return err;
        }

        err = mQueue.Back().mValue.Capture(functor, arg);
        if (!err.IsNone()) {
            return err;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Runs thread pool.
     *
     * @return Error.
     */
    Error Run()
    {
        LockGuard lock(mMutex);

        mShutdown = false;

        for (auto& thread : mThreads) {
            auto err = thread.Run([this](void*) {
                StaticFunction<cMaxTaskSize> task;

                while (true) {
                    UniqueLock lock(mMutex);

                    auto err = mTaskCondVar.Wait([this]() { return mShutdown || !mQueue.IsEmpty(); });
                    assert(err.IsNone());

                    if (mShutdown) {
                        return;
                    }

                    auto result = mQueue.Front();
                    assert(result.mError.IsNone());

                    task = result.mValue;

                    err = mQueue.Pop();
                    assert(err.IsNone());

                    lock.Unlock();

                    if (task) {
                        task();
                        task.Reset();
                    }

                    err = mTaskCondVar.NotifyOne();
                    assert(err.IsNone());

                    lock.Lock();

                    mPendingTaskCount--;

                    lock.Unlock();

                    err = mWaitCondVar.NotifyAll();
                    assert(err.IsNone());
                }
            });
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Waits for all current tasks are finished.
     */
    Error Wait()
    {
        LockGuard lock(mMutex);

        auto err = mWaitCondVar.Wait([this]() { return mPendingTaskCount == 0; });
        if (!err.IsNone()) {
            return err;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Shutdowns all pool threads.
     */
    Error Shutdown()
    {
        UniqueLock lock(mMutex);

        mShutdown = true;
        mQueue.Clear();

        lock.Unlock();

        auto err = mTaskCondVar.NotifyAll();
        if (!err.IsNone()) {
            return err;
        }

        for (auto& thread : mThreads) {
            auto joinErr = thread.Join();
            if (!joinErr.IsNone() && err.IsNone()) {
                err = joinErr;
            }
        }

        return err;
    }

private:
    Thread<cMaxTaskSize, cThreadStackSize>                mThreads[cNumThreads];
    Mutex                                                 mMutex;
    ConditionalVariable                                   mTaskCondVar;
    ConditionalVariable                                   mWaitCondVar;
    StaticQueue<StaticFunction<cMaxTaskSize>, cQueueSize> mQueue;
    bool                                                  mShutdown;
    int                                                   mPendingTaskCount;
};

} // namespace aos

#endif
