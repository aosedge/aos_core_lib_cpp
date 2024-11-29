/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_MEMORY_HPP_
#define AOS_MEMORY_HPP_

#include "aos/common/tools/allocator.hpp"

namespace aos {

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
    DefaultDeleter(Allocator* allocator = nullptr)
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
     * @return Allocator*.
     */
    Allocator* GetAllocator() const { return mAllocator; }

    /**
     * Destroys object & deallocates memory.
     *
     * @ptr input pointer.
     */
    void operator()(T* ptr)
    {
        if (mAllocator) {
            ptr->~T();
            operator delete(ptr, mAllocator);
        }
    }

private:
    Allocator* mAllocator;
};

/**
 * Smart pointer instance.
 *
 * @tparam T holding object type.
 */
template <typename T>
class SmartPtr {
public:
    /**
     * Creates smart pointer.
     *
     * @param allocator allocator.
     * @param object object to make smart pointer.
     */
    SmartPtr(Allocator* allocator = nullptr, T* object = nullptr)
        : mAllocator(allocator)
        , mObject(object)
    {
        assert(!(object && !allocator));
    }

    /**
     * Deletes holding object and release smart pointer.
     *
     * @param allocator new allocator.
     * @param object new object.
     */
    void Reset(Allocator* allocator = nullptr, T* object = nullptr)
    {
        if (mAllocator && mObject) {
            mObject->~T();
            operator delete(const_cast<RemoveConstType<T>*>(mObject), mAllocator);
        }

        assert(!(object && !allocator));

        Release(allocator, object);
    }

    /**
     * Releases smart pointer.
     *
     * @param allocator new allocator.
     * @param object new object.
     * @return T* pointer to holding object.
     */
    T* Release(Allocator* allocator = nullptr, T* object = nullptr)
    {
        auto curObject = mObject;

        mObject    = object;
        mAllocator = allocator;

        return curObject;
    }

    /**
     * Returns holding object.
     *
     * @return T* holding object.
     */
    T* Get() const { return mObject; }

    /**
     * Returns holding allocator.
     *
     * @return Allocator* holding allocator.
     */
    Allocator* GetAllocator() const { return mAllocator; }

    /**
     * Checks if pointer holds object.
     *
     * @return bool.
     */
    explicit operator bool() const { return mObject != nullptr; }

    /**
     * Compares two smart pointers.
     *
     * @param ptr1 first smart pointer.
     * @param ptr2 second smart pointer.
     * @return bool.
     */
    friend bool operator==(const SmartPtr& ptr1, const SmartPtr& ptr2) { return ptr1.mObject == ptr2.mObject; }

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

private:
    Allocator* mAllocator {};
    T*         mObject {};
};

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
    UniquePtr(T* ptr = nullptr, Deleter&& deleter = Deleter())
        : mObject(ptr)
        , mDeleter(Move(deleter))
    {
    }

    /**
     * Unique pointer move constructor.
     *
     * @param ptr unique pointer to move from.
     */
    UniquePtr(UniquePtr&& ptr) { *this = Move(ptr); }

    /**
     * Unique pointer move assignment.
     *
     * @param ptr unique pointer to assign from.
     */
    UniquePtr& operator=(UniquePtr&& ptr)
    {
        mObject  = ptr.mObject;
        mDeleter = Move(ptr.mDeleter);

        ptr.Release();

        return *this;
    }

    /**
     * Unique pointer move constructor for derived class.
     *
     * @param ptr unique pointer to move from.
     */
    template <typename P, typename D, typename = EnableIf<IsBaseOf<T, P>::value>>
    explicit UniquePtr(UniquePtr<P, D>&& ptr)
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
        mObject  = ptr.Release();
        mDeleter = Move(ptr.GetDeleter());

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
 * Shared pointer instance.
 *
 * @tparam T holding object type.
 */
template <typename T>
class SharedPtr : public SmartPtr<T> {
public:
    /**
     * Creates shared pointer.
     */
    SharedPtr(Allocator* allocator = nullptr, T* object = nullptr)
        : SmartPtr<T>(allocator, object)
    {
        if (allocator && object) {
            mAllocation = allocator->FindAllocation(object).mValue;
            SmartPtr<T>::GetAllocator()->TakeAllocation(mAllocation);
        }
    }

    /**
     * Creates shared pointer from another pointer.
     *
     * @param ptr pointer to create from.
     */
    SharedPtr(const SharedPtr& ptr)
        : SmartPtr<T>(ptr.GetAllocator(), ptr.Get())
        , mAllocation(ptr.mAllocation)
    {
        if (SmartPtr<T>::GetAllocator()) {
            SmartPtr<T>::GetAllocator()->TakeAllocation(mAllocation);
        }
    }

    /**
     * Assigns shared pointer from another shared pointer.
     *
     * @param ptr shared pointer to assign from.
     */
    SharedPtr& operator=(const SharedPtr& ptr)
    {
        SmartPtr<T>::Release(ptr.GetAllocator(), ptr.Get());
        mAllocation = ptr.mAllocation;

        if (SmartPtr<T>::GetAllocator()) {
            SmartPtr<T>::GetAllocator()->TakeAllocation(mAllocation);
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
        : SmartPtr<T>(ptr.GetAllocator(), ptr.Get())
        , mAllocation(ptr.mAllocation)
    {
        if (SmartPtr<T>::GetAllocator()) {
            SmartPtr<T>::GetAllocator()->TakeAllocation(mAllocation);
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
        SmartPtr<T>::Release(ptr.GetAllocator(), ptr.Get());
        mAllocation = ptr.mAllocation;

        if (SmartPtr<T>::GetAllocator()) {
            SmartPtr<T>::GetAllocator()->TakeAllocation(mAllocation);
        }

        return *this;
    }

    /**
     * Resets shared pointer.
     *
     * @param allocator new allocator.
     * @param object new object.
     */
    void Reset(Allocator* allocator = nullptr, T* object = nullptr)
    {
        if (SmartPtr<T>::GetAllocator() && SmartPtr<T>::GetAllocator()->GiveAllocation(mAllocation) == 0) {
            SmartPtr<T>::Reset(allocator, object);
        }

        SmartPtr<T>::Release(allocator, object);
        mAllocation = {};

        if (allocator && object) {
            mAllocation = allocator->FindAllocation(object).mValue;
            SmartPtr<T>::GetAllocator()->TakeAllocation(mAllocation);
        }
    }

    /**
     * Destroys unique pointer.
     */
    ~SharedPtr() { Reset(); }

private:
    template <typename>
    friend class SharedPtr;

    List<Allocator::Allocation>::Iterator mAllocation;
};

/**
 * Constructs unique pointer.
 *
 * @tparam T holding object type.
 * @tparam Args holding object constructor parameters types.
 * @param allocator allocator.
 * @param args holding object constructor parameters.
 * @return UniquePtr<T> constructed unique ptr.
 */
template <typename T, typename... Args>
inline UniquePtr<T> MakeUnique(Allocator* allocator, Args&&... args)
{
    assert(allocator);

    return UniquePtr<T>(new (allocator) T(args...), DefaultDeleter<T>(allocator));
}

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
 * Constructs shared pointer.
 *
 * @tparam T holding object type.
 * @tparam Args holding object constructor parameters types.
 * @param allocator allocator.
 * @param args holding object constructor parameters.
 * @return SharedPtr<T> constructed shared ptr.
 */
template <typename T, typename... Args>
inline SharedPtr<T> MakeShared(Allocator* allocator, Args&&... args)
{
    assert(allocator);

    return SharedPtr<T>(allocator, new (allocator) T(args...));
}

} // namespace aos

#endif
