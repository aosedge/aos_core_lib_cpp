/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tools/thread.hpp>

using namespace aos;

constexpr auto cNumIteration = 100;

class TestCalculator {
public:
    TestCalculator()
        : mValue(0)
        , mInc(1)
    {
    }

    void   Inc() { mValue = mValue + mInc; }
    void   SetIncrementer(int i) { mInc = i; }
    int    GetResult() const { return mValue; }
    Mutex& GetMutex() { return mMutex; }

private:
    int   mValue;
    int   mInc;
    Mutex mMutex;
};

static void* calcDec(void* arg)
{
    auto calc = static_cast<TestCalculator*>(arg);

    calc->SetIncrementer(-1);

    for (auto i = 0; i < cNumIteration; i++) {
        calc->Inc();
    }

    return nullptr;
}

TEST(ThreadTest, Basic)
{
    // Test thread, mutex, lock guard, lambda

    TestCalculator calc;

    Thread<> incThread;
    Thread<> distThread;

    EXPECT_TRUE(incThread
                    .Run([&calc](void*) {
                        for (auto i = 0; i < cNumIteration; i++) {
                            LockGuard lock(calc.GetMutex());
                            EXPECT_TRUE(lock.GetError().IsNone());

                            calc.Inc();

                            usleep(1000);
                        }
                    })
                    .IsNone());
    EXPECT_TRUE(distThread
                    .Run([&](void*) {
                        for (auto i = 0; i < cNumIteration; i++) {
                            LockGuard lock(calc.GetMutex());

                            calc.SetIncrementer(0);

                            usleep(1000);

                            calc.SetIncrementer(1);
                        }
                    })
                    .IsNone());

    EXPECT_TRUE(incThread.Join().IsNone());
    EXPECT_TRUE(distThread.Join().IsNone());

    EXPECT_EQ(calc.GetResult(), cNumIteration);

    // Test static function

    Thread<> decThread;

    EXPECT_TRUE(decThread.Run(calcDec, &calc).IsNone());
    EXPECT_TRUE(decThread.Join().IsNone());

    EXPECT_EQ(calc.GetResult(), 0);
}

TEST(ThreadTest, CondVar)
{
    Mutex               mutex;
    ConditionalVariable condVar;
    auto                ready     = false;
    auto                processed = false;

    Thread<> worker;

    EXPECT_TRUE(worker
                    .Run([&](void*) {
                        UniqueLock lock(mutex);
                        EXPECT_TRUE(lock.GetError().IsNone());

                        EXPECT_TRUE(condVar.Wait(lock, [&] { return ready; }).IsNone());

                        processed = true;

                        EXPECT_TRUE(lock.Unlock().IsNone());

                        EXPECT_TRUE(condVar.NotifyOne().IsNone());
                    })
                    .IsNone());

    {
        LockGuard lock(mutex);
        EXPECT_TRUE(lock.GetError().IsNone());

        ready = true;
    }

    EXPECT_TRUE(condVar.NotifyOne().IsNone());

    {
        UniqueLock lock(mutex);
        EXPECT_TRUE(lock.GetError().IsNone());

        EXPECT_TRUE(condVar.Wait(lock, [&] { return processed; }).IsNone());
    }

    EXPECT_TRUE(ready);
    EXPECT_TRUE(processed);
    EXPECT_TRUE(worker.Join().IsNone());
}

TEST(ThreadTest, CondVarTimeout)
{
    Mutex               mutex;
    ConditionalVariable condVar;
    Thread<>            worker;
    bool                ready = false;

    // Check normal

    EXPECT_TRUE(worker
                    .Run([&](void*) {
                        UniqueLock lock(mutex);
                        EXPECT_TRUE(lock.GetError().IsNone());

                        EXPECT_TRUE(condVar.Wait(lock, Time::cSeconds).IsNone());
                    })
                    .IsNone());
    usleep(500000);
    EXPECT_TRUE(condVar.NotifyOne().IsNone());
    EXPECT_TRUE(worker.Join().IsNone());

    // Check timeout

    EXPECT_TRUE(worker
                    .Run([&](void*) {
                        UniqueLock lock(mutex);
                        EXPECT_TRUE(lock.GetError().IsNone());

                        EXPECT_EQ(condVar.Wait(lock, Time::cMilliseconds * 100), aos::ErrorEnum::eTimeout);
                    })
                    .IsNone());

    EXPECT_TRUE(worker.Join().IsNone());

    // Check normal with predicate

    EXPECT_TRUE(worker
                    .Run([&](void*) {
                        UniqueLock lock(mutex);
                        EXPECT_TRUE(lock.GetError().IsNone());

                        EXPECT_TRUE(condVar.Wait(lock, Time::cSeconds, [&] { return ready; }).IsNone());
                        ready = false;
                    })
                    .IsNone());
    {
        LockGuard lock(mutex);
        EXPECT_TRUE(lock.GetError().IsNone());

        ready = true;
    }

    EXPECT_TRUE(condVar.NotifyOne().IsNone());
    EXPECT_TRUE(worker.Join().IsNone());

    // Check timeout with predicate

    EXPECT_TRUE(worker
                    .Run([&](void*) {
                        UniqueLock lock(mutex);
                        EXPECT_TRUE(lock.GetError().IsNone());

                        EXPECT_EQ(condVar.Wait(lock, Time::cMilliseconds * 100, [&] { return ready; }),
                            aos::ErrorEnum::eTimeout);
                    })
                    .IsNone());

    EXPECT_TRUE(worker.Join().IsNone());
}

TEST(ThreadTest, ThreadPool)
{
    ThreadPool<3, 32 * 32 * 3> threadPool;
    Mutex                      mutex;
    auto                       value1 = 0, value2 = 0, value3 = 0;
    auto                       i = 0;

    for (; i < 32; i++) {
        EXPECT_TRUE(threadPool
                        .AddTask([&value1, &mutex](void*) {
                            LockGuard lock(mutex);
                            value1++;
                        })
                        .IsNone());
        EXPECT_TRUE(threadPool
                        .AddTask([&value2, &mutex](void*) {
                            LockGuard lock(mutex);
                            value2++;
                        })
                        .IsNone());
        EXPECT_TRUE(threadPool
                        .AddTask([&value3, &mutex](void*) {
                            LockGuard lock(mutex);
                            value3++;
                        })
                        .IsNone());
    }

    EXPECT_TRUE(threadPool.Run().IsNone());
    EXPECT_TRUE(threadPool.Wait().IsNone());

    EXPECT_EQ(value1, i);
    EXPECT_EQ(value2, i);
    EXPECT_EQ(value3, i);

    EXPECT_TRUE(threadPool.Shutdown().IsNone());
}
