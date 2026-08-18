/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_MEMORY_HPP_
#define AOS_CORE_COMMON_TOOLS_MEMORY_HPP_

#include <assert.h>
#include <new>
#include <stddef.h>

#include "noncopyable.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace aos {

/**
 * Allocator interface.
 *
 * Any implementation only has to provide Allocate/Free semantics, so it can be backed by a
 * heap (see HeapAllocator) or by any custom allocation strategy required by a specific target
 * (e.g. a static/embedded arena for safety critical domains).
 */
class AllocatorItf {
public:
    /**
     * Allocates data with specified size.
     *
     * @param size allocate size.
     * @return void* pointer to allocated data, nullptr if allocation failed.
     */
    virtual void* Allocate(size_t size) = 0;

    /**
     * Frees previously allocated data.
     *
     * @param data allocated data to free.
     */
    virtual void Free(void* data) = 0;

    /**
     * Destroys allocator instance.
     */
    virtual ~AllocatorItf() = default;
};

/**
 * Default deleter invokes delete operator for the given pointer.
 */
template <typename T>
class DefaultDeleter : public NonCopyable {
public:
    /**
     * Constructor that takes a pointer to allocator.
     *
     * @param allocator input allocator.
     */
    explicit DefaultDeleter(AllocatorItf* allocator = nullptr)
        : mAllocator(allocator)
    {
    }

    /**
     * Move constructor for a DefaultDeleter of a derived class.
     */
    template <typename P>
    explicit DefaultDeleter(DefaultDeleter<P>&& other)
    {
        *this = Move(other);
    }

    /**
     * Move assignment operator for a DefaultDeleter of a derived class.
     */
    template <typename P>
    DefaultDeleter& operator=(DefaultDeleter<P>&& other)
    {
        mAllocator = other.GetAllocator();

        return *this;
    }

    /**
     * Returns allocator.
     *
     * @return AllocatorItf*.
     */
    AllocatorItf* GetAllocator() const { return mAllocator; }

    /**
     * Destroys object & deallocates memory.
     *
     * @ptr input pointer.
     */
    void operator()(T* ptr)
    {
        if (mAllocator) {
            ptr->~T();
            mAllocator->Free(ptr);
        }
    }

private:
    AllocatorItf* mAllocator;
};

/**
 * Deleter function for objects adopted by a shared pointer.
 */
template <typename T>
inline void SmartPtrDeleter(void* ptr, AllocatorItf* allocator)
{
    if (ptr) {
        static_cast<T*>(ptr)->~T();

        if (allocator) {
            allocator->Free(ptr);
        }
    }
}

/**
 * Unique pointer instance.
 *
 * @tparam T holding object type.
 */
template <typename T, typename Deleter = DefaultDeleter<T>>
class UniquePtr : private NonCopyable {
public:
    /**
     * Default constructor.
     *
     * @param ptr pointer to an object to be destroyed.
     * @param deleter functor destroying the object.
     *
     */
    UniquePtr(T* ptr, Deleter&& deleter)
        : mObject(ptr)
        , mDeleter(Move(deleter))
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Default constructor.
     *
     * @param ptr pointer to an object to be destroyed.
     * @param allocator allocator that object was allocated with.
     *
     */
    UniquePtr(T* ptr = nullptr, AllocatorItf* allocator = nullptr)
        : mObject(ptr)
        , mDeleter(DefaultDeleter<T>(allocator))
    {
    }

    /**
     * Unique pointer move constructor.
     *
     * @param ptr unique pointer to move from.
     */
    UniquePtr(UniquePtr&& ptr)
        : UniquePtr()
    {
        *this = Move(ptr);
    }

    /**
     * Unique pointer move assignment.
     *
     * @param ptr unique pointer to assign from.
     */
    UniquePtr& operator=(UniquePtr&& ptr)
    {
        Reset();

        mDeleter = Move(ptr.GetDeleter());
        mObject  = ptr.Release();

        return *this;
    }

    /**
     * Unique pointer move constructor for derived class.
     *
     * @param ptr unique pointer to move from.
     */
    template <typename P, typename D, typename = EnableIf<IsBaseOf<T, P>::value>>
    // cppcheck-suppress noExplicitConstructor
    UniquePtr(UniquePtr<P, D>&& ptr)
        : UniquePtr()
    {
        *this = Move(ptr);
    }

    /**
     * Unique pointer move assignment for derived class.
     *
     * @param ptr unique pointer to assign from.
     */
    template <typename P, typename D, typename = EnableIf<IsBaseOf<T, P>::value>>
    UniquePtr& operator=(UniquePtr<P, D>&& ptr)
    {
        Reset();

        mDeleter = Move(ptr.GetDeleter());
        mObject  = ptr.Release();

        return *this;
    }

    /**
     * Resets unique pointer.
     *
     * @param object new object.
     */
    void Reset(T* object = nullptr)
    {
        if (mObject) {
            mDeleter(mObject);
            mObject = nullptr;
        }

        mObject = object;
    }

    /**
     * Releases stored pointer and returns it.
     *
     * @return T* stored pointer.
     */
    T* Release()
    {
        auto object = mObject;

        mObject = nullptr;

        return object;
    }

    /**
     * Checks if pointer holds object.
     *
     * @return bool.
     */
    explicit operator bool() const { return mObject != nullptr; }

    /**
     * Compares two unique pointers.
     *
     * @param ptr1 first smart pointer.
     * @param ptr2 second smart pointer.
     * @return bool.
     */
    friend bool operator==(const UniquePtr& ptr1, const UniquePtr& ptr2) { return ptr1.mObject == ptr2.mObject; }

    /**
     * Returns holding object.
     *
     * @return T* holding object.
     */
    T* Get() const { return mObject; }

    /**
     * Returns deleter.
     *
     * @return Deleter&.
     */
    Deleter& GetDeleter() { return mDeleter; }

    /**
     * Returns deleter.
     *
     * @return const Deleter&.
     */
    const Deleter& GetDeleter() const { return mDeleter; }

    /**
     * Provides access to holding object fields.
     *
     * @return T* holding object pointer.
     */
    T* operator->() const { return mObject; }

    /**
     * Dereferences holding object.
     *
     * @return T& holding object value.
     */
    T& operator*() const { return *(mObject); }

    /**
     * Destroys object.
     */
    ~UniquePtr() { Reset(); }

private:
    T*      mObject;
    Deleter mDeleter;
};

/**
 * Defers object destruction till the end of the current scope.
 *
 * @tparam T type of the object to be destroyed.
 * @tparam Deleter type of the deleter.
 * @param ptr pointer to the object to be destroyed.
 * @param deleter functor object to be deferred.
 * @return UniquePtr<T, Deleter>.
 */
template <typename T, typename Deleter>
inline UniquePtr<T, Deleter> DeferRelease(T* ptr, Deleter&& deleter)
{
    return UniquePtr<T, Deleter>(ptr, Move(deleter));
}

/**
 * Base class for shared pointer control blocks.
 *
 * Owns the reference count and knows how to dispose of itself (and whatever it holds) through
 * the same AllocatorItf it was created with. This keeps SharedPtr's ref-counting independent
 * from any allocator-specific bookkeeping, so it works the same way for any AllocatorItf
 * implementation (heap based or static/embedded).
 *
 * Take/Give are mutex protected so that a control block can be safely shared (copied, reset)
 * across multiple threads, e.g. via SharedPtr instances passed between them.
 */
class SharedControlBlock : private NonCopyable {
public:
    /**
     * Creates control block instance.
     *
     * @param allocator allocator the control block itself was allocated from.
     */
    explicit SharedControlBlock(AllocatorItf& allocator)
        : mAllocator(allocator)
    {
    }

    /**
     * Increases shared count.
     *
     * @return size_t shared count value.
     */
    size_t Take()
    {
        LockGuard lock(mMutex);

        return ++mRefCount;
    }

    /**
     * Decreases shared count. Disposes the control block once the count reaches zero.
     *
     * @return size_t shared count value.
     */
    size_t Give()
    {
        UniqueLock<Mutex> lock(mMutex);

        auto count = --mRefCount;

        if (count == 0) {
            // Unlock before disposing as disposal destroys this object (and its mutex).
            lock.Unlock();

            Dispose();
        }

        return count;
    }

    /**
     * Destroys control block instance.
     */
    virtual ~SharedControlBlock() = default;

protected:
    /**
     * Allocator the control block itself was allocated from.
     */
    AllocatorItf& mAllocator;

private:
    virtual void Dispose() = 0;

    Mutex  mMutex;
    size_t mRefCount = 1;
};

/**
 * Control block that owns an object of type T directly (single allocation). Used by MakeShared.
 *
 * @tparam T holding object type.
 */
template <typename T>
class SharedObjectControlBlock : public SharedControlBlock {
public:
    /**
     * Creates control block instance constructing the held object in place.
     *
     * @param allocator allocator the control block is allocated from.
     * @param args holding object constructor parameters.
     */
    template <typename... Args>
    explicit SharedObjectControlBlock(AllocatorItf& allocator, Args&&... args)
        : SharedControlBlock(allocator)
        , mObject(args...)
    {
    }

    /**
     * Returns pointer to the held object.
     *
     * @return T*.
     */
    T* GetObject() { return &mObject; }

private:
    void Dispose() override
    {
        auto& allocator = mAllocator;

        this->~SharedObjectControlBlock();
        allocator.Free(this);
    }

    T mObject;
};

/**
 * Control block that adopts an already constructed, separately allocated object.
 *
 * @tparam T holding object type.
 */
template <typename T>
class SharedAdoptControlBlock : public SharedControlBlock {
public:
    /**
     * Deleter.
     */
    using Deleter = void (*)(void*, AllocatorItf*);

    /**
     * Creates control block instance.
     *
     * @param allocator allocator the control block itself is allocated from.
     * @param object object to adopt.
     * @param deleter functor destroying the adopted object.
     */
    SharedAdoptControlBlock(AllocatorItf& allocator, T* object, Deleter deleter)
        : SharedControlBlock(allocator)
        , mObject(object)
        , mDeleter(deleter)
    {
    }

private:
    void Dispose() override
    {
        auto& allocator = mAllocator;

        if (mDeleter) {
            mDeleter(mObject, &allocator);
        }

        this->~SharedAdoptControlBlock();
        allocator.Free(this);
    }

    T*      mObject;
    Deleter mDeleter;
};

/**
 * Shared pointer instance.
 *
 * @tparam T holding object type.
 */
template <typename T>
class SharedPtr {
public:
    /**
     * Deleter.
     */
    using Deleter = void (*)(void*, AllocatorItf*);

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates shared pointer adopting an already allocated object.
     *
     * @param allocator allocator object was allocated with.
     * @param object object to adopt.
     * @param deleter functor destroying the object.
     */
    SharedPtr(AllocatorItf* allocator = nullptr, T* object = nullptr, Deleter deleter = SmartPtrDeleter<T>)
    {
        Adopt(allocator, object, deleter);
    }

    /**
     * Creates shared pointer from another pointer.
     *
     * @param ptr pointer to create from.
     */
    SharedPtr(const SharedPtr& ptr)
        : mObject(ptr.mObject)
        , mControlBlock(ptr.mControlBlock)
    {
        if (mControlBlock) {
            mControlBlock->Take();
        }
    }

    /**
     * Assigns shared pointer from another shared pointer.
     *
     * @param ptr shared pointer to assign from.
     */
    SharedPtr& operator=(const SharedPtr& ptr)
    {
        if (this == &ptr) {
            return *this;
        }

        Reset();

        mObject       = ptr.mObject;
        mControlBlock = ptr.mControlBlock;

        if (mControlBlock) {
            mControlBlock->Take();
        }

        return *this;
    }

    /**
     * Creates shared pointer from another derived class shared pointer.
     *
     * @param ptr pointer to create from.
     */
    template <typename P>
    // cppcheck-suppress noExplicitConstructor
    SharedPtr(const SharedPtr<P>& ptr)
        : mObject(ptr.mObject)
        , mControlBlock(ptr.mControlBlock)
    {
        if (mControlBlock) {
            mControlBlock->Take();
        }
    }

    /**
     * Assigns shared pointer from another derived class shared pointer.
     *
     * @param ptr shared pointer to assign from.
     */
    template <typename P>
    SharedPtr& operator=(const SharedPtr<P>& ptr)
    {
        Reset();

        mObject       = ptr.mObject;
        mControlBlock = ptr.mControlBlock;

        if (mControlBlock) {
            mControlBlock->Take();
        }

        return *this;
    }

    /**
     * Resets shared pointer.
     *
     * @param allocator new allocator.
     * @param object new object.
     * @param deleter functor destroying the object.
     */
    void Reset(AllocatorItf* allocator = nullptr, T* object = nullptr, Deleter deleter = SmartPtrDeleter<T>)
    {
        if (mControlBlock) {
            mControlBlock->Give();
        }

        mObject       = nullptr;
        mControlBlock = nullptr;

        Adopt(allocator, object, deleter);
    }

    /**
     * Returns holding object.
     *
     * @return T* holding object.
     */
    T* Get() const { return mObject; }

    /**
     * Checks if pointer holds object.
     *
     * @return bool.
     */
    explicit operator bool() const { return mObject != nullptr; }

    /**
     * Compares two shared pointers.
     *
     * @param ptr1 first shared pointer.
     * @param ptr2 second shared pointer.
     * @return bool.
     */
    friend bool operator==(const SharedPtr& ptr1, const SharedPtr& ptr2) { return ptr1.mObject == ptr2.mObject; }

    /**
     * Provides access to holding object fields.
     *
     * @return T* holding object pointer.
     */
    T* operator->() const { return mObject; }

    /**
     * Dereferences holding object.
     *
     * @return T& holding object value.
     */
    T& operator*() const { return *(mObject); }

    /**
     * Destroys shared pointer.
     */
    ~SharedPtr() { Reset(); }

private:
    template <typename>
    friend class SharedPtr;

    template <typename U, typename... Args>
    friend SharedPtr<U> MakeShared(AllocatorItf* allocator, Args&&... args);

    SharedPtr(SharedControlBlock* controlBlock, T* object)
        : mObject(object)
        , mControlBlock(controlBlock)
    {
    }

    void Adopt(AllocatorItf* allocator, T* object, Deleter deleter)
    {
        if (!allocator || !object) {
            return;
        }

        auto data = allocator->Allocate(sizeof(SharedAdoptControlBlock<T>));
        if (!data) {
            if (deleter) {
                deleter(object, allocator);
            }

            return;
        }

        mControlBlock = new (data) SharedAdoptControlBlock<T>(*allocator, object, deleter);
        mObject       = object;
    }

    T*                  mObject {};
    SharedControlBlock* mControlBlock {};
};

/**
 * Constructs unique pointer.
 *
 * @tparam T holding object type.
 * @tparam Args holding object constructor parameters types.
 * @param allocator allocator.
 * @param args holding object constructor parameters.
 * @return UniquePtr<T> constructed unique ptr, empty if allocation failed.
 */
template <typename T, typename... Args>
inline UniquePtr<T> MakeUnique(AllocatorItf* allocator, Args&&... args)
{
    assert(allocator);

    auto data = allocator->Allocate(sizeof(T));
    if (!data) {
        return UniquePtr<T>();
    }

    return UniquePtr<T>(new (data) T(args...), DefaultDeleter<T>(allocator));
}

/**
 * Constructs shared pointer.
 *
 * @tparam T holding object type.
 * @tparam Args holding object constructor parameters types.
 * @param allocator allocator.
 * @param args holding object constructor parameters.
 * @return SharedPtr<T> constructed shared ptr, empty if allocation failed.
 */
template <typename T, typename... Args>
inline SharedPtr<T> MakeShared(AllocatorItf* allocator, Args&&... args)
{
    assert(allocator);

    auto data = allocator->Allocate(sizeof(SharedObjectControlBlock<T>));
    if (!data) {
        return SharedPtr<T>();
    }

    auto* controlBlock = new (data) SharedObjectControlBlock<T>(*allocator, args...);

    return SharedPtr<T>(controlBlock, controlBlock->GetObject());
}

} // namespace aos

#endif
