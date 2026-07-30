/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <core/common/tools/heapallocator.hpp>
#include <core/common/tools/memory.hpp>

using namespace aos;

static void OwnUniquePtr(UniquePtr<uint32_t> uPtr)
{
    EXPECT_TRUE(uPtr);
}

static void TakeSharedPtr(SharedPtr<uint32_t> shPtr)
{
    EXPECT_TRUE(shPtr);
}

namespace {
class BaseClass {
public:
    BaseClass() {};
    virtual ~BaseClass() {};
};

class NewClass : public BaseClass {
public:
    NewClass() {};
    virtual ~NewClass() {};
};

}; // namespace

TEST(MemoryTest, UniquePtr)
{
    HeapAllocator allocator;

    // Basic test

    {
        UniquePtr<uint32_t> uPtr = MakeUnique<uint32_t>(&allocator, 0);
        EXPECT_TRUE(uPtr);
        EXPECT_EQ(*uPtr, 0U);
    }

    // Construct with allocator

    {
        auto* raw = static_cast<uint32_t*>(allocator.Allocate(sizeof(uint32_t)));
        ASSERT_NE(raw, nullptr);

        UniquePtr<uint32_t> uPtr(new (raw) uint32_t(), &allocator);
        EXPECT_TRUE(uPtr);
    }

    // Construct with deleter

    {
        auto deleter = [&allocator](uint32_t* ptr) { allocator.Free(ptr); };

        auto* raw = static_cast<uint32_t*>(allocator.Allocate(sizeof(uint32_t)));
        ASSERT_NE(raw, nullptr);

        UniquePtr<uint32_t, decltype(deleter)> uPtr(new (raw) uint32_t(), Move(deleter));
    }

    // Move ownership

    UniquePtr<uint32_t> uPtr;

    EXPECT_FALSE(uPtr);
    EXPECT_TRUE(uPtr == nullptr);
    EXPECT_TRUE(nullptr == uPtr);

    {
        uPtr = MakeUnique<uint32_t>(&allocator);
    }

    EXPECT_TRUE(uPtr);

    OwnUniquePtr(Move(uPtr));

    EXPECT_FALSE(uPtr);

    // Make unique

    auto uPtr2 = MakeUnique<uint32_t>(&allocator);
    EXPECT_TRUE(uPtr2);

    // Check reset

    uPtr2.Reset();
    EXPECT_FALSE(uPtr2);
}

TEST(MemoryTest, SharedPtr)
{
    HeapAllocator allocator;

    // Basic test

    {
        auto* raw = static_cast<uint32_t*>(allocator.Allocate(sizeof(uint32_t)));
        ASSERT_NE(raw, nullptr);

        SharedPtr<uint32_t> shPtr(&allocator, new (raw) uint32_t());
        EXPECT_TRUE(shPtr);
    }

    // Test share

    {
        SharedPtr<uint32_t> shPtr;

        EXPECT_FALSE(shPtr);
        EXPECT_TRUE(shPtr == nullptr);
        EXPECT_TRUE(nullptr == shPtr);

        {
            auto* raw = static_cast<uint32_t*>(allocator.Allocate(sizeof(uint32_t)));
            ASSERT_NE(raw, nullptr);

            shPtr = SharedPtr<uint32_t>(&allocator, new (raw) uint32_t());
        }

        EXPECT_TRUE(shPtr);

        TakeSharedPtr(shPtr);
    }

    // Make shared

    auto shPtr2 = MakeShared<uint32_t>(&allocator);
    EXPECT_TRUE(shPtr2);

    // Check reset

    shPtr2.Reset();
    EXPECT_FALSE(shPtr2);
}

TEST(MemoryTest, UniquePtrDerivedClass)
{
    HeapAllocator allocator;

    UniquePtr<BaseClass> basePtr;

    {
        auto newPtr = MakeUnique<NewClass>(&allocator);
        EXPECT_TRUE(newPtr);

        basePtr = Move(newPtr);
    }

    EXPECT_TRUE(basePtr);
}

TEST(MemoryTest, SharedPtrDerivedClass)
{
    HeapAllocator allocator;

    SharedPtr<BaseClass> basePtr;

    {
        auto newPtr = MakeShared<NewClass>(&allocator);
        EXPECT_TRUE(newPtr);

        basePtr = newPtr;
    }

    EXPECT_TRUE(basePtr);
}

TEST(MemoryTest, DeferRelease)
{
    int                               dummy = 0x42;
    testing::MockFunction<void(int*)> deleter;

    EXPECT_CALL(deleter, Call(&dummy)).Times(1);
    {
        [[maybe_unused]] auto defer = DeferRelease(&dummy, deleter.AsStdFunction());
    }
}

TEST(MemoryTest, DeferReleaseNoOpForNull)
{
    testing::MockFunction<void(int*)> deleter;

    EXPECT_CALL(deleter, Call(testing::_)).Times(0);
    {
        [[maybe_unused]] auto defer = DeferRelease(static_cast<int*>(nullptr), deleter.AsStdFunction());
    }
}

TEST(MemoryTest, UniquePtrDeallocAfterMove)
{
    int                               dummy = 0x42;
    testing::MockFunction<void(int*)> deleter1;
    testing::MockFunction<void(int*)> deleter2;

    EXPECT_CALL(deleter1, Call(&dummy)).Times(1);
    EXPECT_CALL(deleter2, Call(&dummy)).Times(1);

    {
        auto ptr1 = DeferRelease(&dummy, deleter1.AsStdFunction());
        // cppcheck-suppress redundantInitialization
        // cppcheck-suppress unreadVariable
        ptr1 = DeferRelease(&dummy, deleter2.AsStdFunction());
    }
}

TEST(MemoryTest, UniquePtrDeallocAfterMoveDifferentTypes)
{
    struct Base { };
    struct Derived : public Base { };

    Base    base;
    Derived derived;

    testing::MockFunction<void(Base*)> deleter1;
    testing::MockFunction<void(Base*)> deleter2;

    EXPECT_CALL(deleter1, Call(&base)).Times(1);
    EXPECT_CALL(deleter2, Call(&derived)).Times(1);

    {
        auto ptr1 = DeferRelease(&base, deleter1.AsStdFunction());
        // cppcheck-suppress redundantInitialization
        // cppcheck-suppress unreadVariable
        ptr1 = DeferRelease(&derived, deleter2.AsStdFunction());
    }
}

TEST(MemoryTest, SharedPtrDerivedValueClass)
{
    using namespace testing;

    MockFunction<void()> callback;

    class BaseClass { };

    class Child : public BaseClass {
    public:
        explicit Child(MockFunction<void()>* func)
            : mFunc(func)
        {
        }

        ~Child() { mFunc->Call(); }

    private:
        MockFunction<void()>* mFunc;
    };

    HeapAllocator allocator;

    // Check NewClass destructor is called
    EXPECT_CALL(callback, Call()).Times(1);

    {
        SharedPtr<BaseClass> basePtr = MakeShared<Child>(&allocator, &callback);
        EXPECT_TRUE(basePtr);
    }
}
