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
#include "aos/common/tools/time.hpp"

#if AOS_CONFIG_THREAD_STACK_GUARD_SIZE != 0
#include <sys/mman.h>
#endif

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
 * Configures thread stack guard size.
 */
constexpr auto cThreadStackGuardSize = AOS_CONFIG_THREAD_STACK_GUARD_SIZE;

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
    /**
     * Constructs Aos thread instance and use lambda as argument.
     */
    Thread() = default;

    /**
     * Destructor.
     */
    ~Thread()
    {
#if AOS_CONFIG_THREAD_STACK_GUARD_SIZE != 0
        auto ret = mprotect(mStack, AlignedSize(cThreadStackGuardSize, cThreadStackAlign), PROT_READ | PROT_WRITE);
        assert(ret == 0);
#endif
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

#if AOS_CONFIG_THREAD_STACK_USAGE
        memset(&mStack[AlignedSize(cThreadStackGuardSize, cThreadStackAlign)], 0xAA,
            AlignedSize(cStackSize, cThreadStackAlign));
#endif

#if AOS_CONFIG_THREAD_STACK_GUARD_SIZE != 0
        if (auto ret = mprotect(mStack, AlignedSize(cThreadStackGuardSize, cThreadStackAlign), PROT_READ); ret != 0) {
            return ret;
        }
#endif

        pthread_attr_t attr;

        auto ret = pthread_attr_init(&attr);
        if (ret != 0) {
            return ret;
        }

        ret = pthread_attr_setstack(&attr, &mStack[AlignedSize(cThreadStackGuardSize, cThreadStackAlign)],
            AlignedSize(cStackSize, cThreadStackAlign));
        if (ret != 0) {
            return ret;
        }

        ret = pthread_create(&mPThread, &attr, ThreadFunction, this);

        mJoinable = ret == 0;

        return ret;
    }

    /**
     * Waits thread function is finished.
     *
     * @return Error.
     */
    Error Join()
    {
        if (mJoinable) {
            mJoinable = false;

            return pthread_join(mPThread, nullptr);
        }

        return ErrorEnum::eNone;
    }

#if AOS_CONFIG_THREAD_STACK_USAGE
    size_t GetStackUsage()
    {
        size_t freeSize = 0;

        for (size_t i = 0; i < AlignedSize(cStackSize, cThreadStackAlign); i++) {
            if (mStack[AlignedSize(cThreadStackGuardSize, cThreadStackAlign) + i] != 0xAA) {
                break;
            }

            freeSize++;
        }

        return AlignedSize(cStackSize, cThreadStackAlign) - freeSize;
    }
#endif

private:
    alignas(cThreadStackAlign) uint8_t
        mStack[AlignedSize(cThreadStackGuardSize, cThreadStackAlign) + AlignedSize(cStackSize, cThreadStackAlign)];
    StaticFunction<cFunctionMaxSize> mFunction;
    pthread_t                        mPThread  = {};
    bool                             mJoinable = false;

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
        if (mError = mMutex.Lock(); !mError.IsNone()) {
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
        if (mError = mMutex.Unlock(); !mError.IsNone()) {
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
    Error GetError() const { return mError; }

    /**
     * Returns reference to holding mutex.
     *
     * @return Mutex&.
     */
    Mutex& GetMutex() const { return mMutex; }

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
     */
    ConditionalVariable() { mError = pthread_cond_init(&mCondVar, nullptr); }

    /**
     * Destroys conditional variable.
     */
    ~ConditionalVariable() { pthread_cond_destroy(&mCondVar); }

    /**
     * Blocks the current thread until the condition variable is awakened.
     *
     * @param lock unique lock.
     * @return Error.
     */
    Error Wait(UniqueLock& lock)
    {
        return mError = pthread_cond_wait(&mCondVar, static_cast<pthread_mutex_t*>(lock.GetMutex()));
    }

    /**
     * Blocks the current thread until the condition variable is awakened or absolute time passed.
     *
     * @param lock unique lock.
     * @param absTime absolute time.
     * @return Error.
     */
    Error Wait(UniqueLock& lock, Time absTime)
    {
        auto unixTime = absTime.UnixTime();

        auto ret = pthread_cond_timedwait(&mCondVar, static_cast<pthread_mutex_t*>(lock.GetMutex()), &unixTime);

        if (ret == ETIMEDOUT) {
            return mError = ErrorEnum::eTimeout;
        }

        return mError = ret;
    }

    /**
     * Blocks the current thread until the condition variable is awakened or time duration passed.
     *
     * @param lock unique lock.
     * @param absTime absolute time.
     * @return Error.
     */
    Error Wait(UniqueLock& lock, Duration duration) { return Wait(lock, Time::Now(cClockID).Add(duration)); }

    /**
     * Blocks the current thread until the condition variable is awakened and predicate condition is met.
     *
     * @param lock unique lock.
     * @param waitCondition wait condition predicate.
     * @return Error.
     */
    template <typename T>
    Error Wait(UniqueLock& lock, T waitCondition)
    {
        while (!waitCondition()) {
            auto err = Wait(lock);
            if (!err.IsNone()) {
                return mError = err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Blocks the current thread until the condition variable is awakened and predicate condition is met or absolute
     * time passed.
     *
     * @param lock unique lock.
     * @param absTime absolute time.
     * @param waitCondition wait condition predicate.
     * @return Error.
     */
    template <typename T>
    Error Wait(UniqueLock& lock, Time absTime, T waitCondition)
    {
        while (!waitCondition()) {
            auto err = Wait(lock, absTime);
            if (!err.IsNone()) {
                return mError = err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Blocks the current thread until the condition variable is awakened and predicate condition is met or time
     * duration passed.
     *
     * @param lock unique lock.
     * @param absTime absolute time.
     * @param waitCondition wait condition predicate.
     * @return Error.
     */
    template <typename T>
    Error Wait(UniqueLock& lock, Duration duration, T waitCondition)
    {
        return Wait(lock, Time::Now(cClockID).Add(duration), waitCondition);
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
    static constexpr auto cClockID = AOS_CONFIG_THREAD_CLOCK_ID;

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
    ThreadPool() { }

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

                    auto err = mTaskCondVar.Wait(lock, [this]() { return mShutdown || !mQueue.IsEmpty(); });
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
        UniqueLock lock(mMutex);

        auto err = mWaitCondVar.Wait(lock, [this]() { return mPendingTaskCount == 0; });
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

#if AOS_CONFIG_THREAD_STACK_USAGE
    size_t GetStackUsage()
    {
        size_t usedSize = 0;

        for (auto& thread : mThreads) {
            auto size = thread.GetStackUsage();

            if (size > usedSize) {
                usedSize = size;
            }
        }

        return usedSize;
    }
#endif

private:
    Thread<cMaxTaskSize, cThreadStackSize>                mThreads[cNumThreads];
    Mutex                                                 mMutex;
    ConditionalVariable                                   mTaskCondVar;
    ConditionalVariable                                   mWaitCondVar;
    StaticQueue<StaticFunction<cMaxTaskSize>, cQueueSize> mQueue;
    bool                                                  mShutdown         = false;
    int                                                   mPendingTaskCount = 0;
};

} // namespace aos

#endif
