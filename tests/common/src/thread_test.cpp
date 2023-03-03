/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/thread.hpp"

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

TEST(common, Thread)
{
    // Test thread, mutex, lock guard, lambda

    TestCalculator calc;

    Thread<> incThread([&calc](void*) {
        for (auto i = 0; i < cNumIteration; i++) {
            LockGuard lock(calc.GetMutex());
            EXPECT_TRUE(lock.GetError().IsNone());

            calc.Inc();

            usleep(1000);
        }
    });
    Thread<> distThread([&](void*) {
        for (auto i = 0; i < cNumIteration; i++) {
            LockGuard lock(calc.GetMutex());

            calc.SetIncrementer(0);

            usleep(1000);

            calc.SetIncrementer(1);
        }
    });

    EXPECT_TRUE(incThread.Run().IsNone());
    EXPECT_TRUE(distThread.Run().IsNone());

    EXPECT_TRUE(incThread.Join().IsNone());
    EXPECT_TRUE(distThread.Join().IsNone());

    EXPECT_EQ(calc.GetResult(), cNumIteration);

    // Test static function

    Thread<> decThread(calcDec, &calc);

    EXPECT_TRUE(decThread.Run().IsNone());
    EXPECT_TRUE(decThread.Join().IsNone());

    EXPECT_EQ(calc.GetResult(), 0);
}

TEST(common, CondVar)
{
    Mutex               mutex;
    ConditionalVariable condVar(mutex);
    auto                ready = false;
    auto                processed = false;

    Thread<> worker([&](void*) {
        UniqueLock lock(mutex);
        EXPECT_TRUE(lock.GetError().IsNone());

        EXPECT_TRUE(condVar.Wait([&] { return ready; }).IsNone());

        processed = true;

        EXPECT_TRUE(lock.Unlock().IsNone());

        EXPECT_TRUE(condVar.NotifyOne().IsNone());
    });

    EXPECT_TRUE(worker.Run().IsNone());

    {
        LockGuard lock(mutex);
        EXPECT_TRUE(lock.GetError().IsNone());

        ready = true;
    }

    EXPECT_TRUE(condVar.NotifyOne().IsNone());

    {
        LockGuard lock(mutex);
        EXPECT_TRUE(lock.GetError().IsNone());

        EXPECT_TRUE(condVar.Wait([&] { return processed; }).IsNone());
    }

    EXPECT_TRUE(ready);
    EXPECT_TRUE(processed);
    EXPECT_TRUE(worker.Join().IsNone());
}
