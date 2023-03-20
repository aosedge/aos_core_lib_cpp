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
        assert(!object || allocator);
    }

    /**
     * Deletes holding object and reset smart pointer.
     */
    void Reset(Allocator* allocator = nullptr, T* object = nullptr)
    {
        if (mAllocator && mObject) {
            operator delete(const_cast<RemoveConstType<T>*>(mObject), mAllocator);
        }

        assert(!object || allocator);

        mObject = object;
        mAllocator = allocator;
    }

    /**
     * Releases smart pointer.
     *
     * @return T* pointer to holding object.
     */
    T* Release()
    {
        auto object = mObject;

        mObject = nullptr;
        mAllocator = nullptr;

        return object;
    }

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
     * Returns holding object.
     *
     * @return T* holding object.
     */
    T* Get() const { return mObject; }

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

protected:
    Allocator* mAllocator {};
    T*         mObject {};
};

/**
 * Unique pointer instance.
 *
 * @tparam T holding object type.
 */
template <typename T>
class UniquePtr : public SmartPtr<T>, private NonCopyable {
public:
    /**
     * Creates unique pointer.
     */
    using SmartPtr<T>::SmartPtr;

    /**
     * Unique pointer move constructor.
     *
     * @param ptr unique pointer to move from.
     */
    UniquePtr(UniquePtr&& ptr)
        : SmartPtr<T>(ptr.mAllocator, ptr.mObject)
    {
        ptr.mObject = nullptr;
        ptr.mAllocator = nullptr;
    }

    /**
     * Unique pointer move assignment.
     *
     * @param ptr unique pointer to assign from.
     */
    void operator=(UniquePtr&& ptr)
    {
        SmartPtr<T>::Reset(ptr.mAllocator, ptr.mObject);

        ptr.mObject = nullptr;
        ptr.mAllocator = nullptr;
    }

    /**
     * Destroys unique pointer.
     */
    ~UniquePtr() { SmartPtr<T>::Reset(); }
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

    return UniquePtr<T>(allocator, new (allocator) T(args...));
}

} // namespace aos

#endif
