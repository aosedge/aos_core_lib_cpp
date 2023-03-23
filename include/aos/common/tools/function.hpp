/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_FUNCTION_HPP_
#define AOS_FUNCTION_HPP_

#include <assert.h>

#include "aos/common/tools/buffer.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/new.hpp"

namespace aos {

/**
 * Default function max size.
 */
constexpr auto cDefaultFunctionMaxSize = AOS_CONFIG_FUNCTION_MAX_SIZE;

/**
 * Function instance.
 */
class Function {
public:
    /**
     * Creates function.
     */
    Function() = default;

    /**
     * Creates function from another function.
     *
     * @param function function to create from.
     */
    Function(const Function& function) = default;

    /**
     * Assigns function to another function.
     *
     * @param function function to assign from.
     * @return Function&.
     */
    Function& operator=(const Function& function)
    {
        if (!function.mCallable) {
            return *this;
        }

        assert(mBuffer);

        memcpy(mBuffer, static_cast<void*>(function.mCallable), function.mCallable->Size());

        mCallable = static_cast<CallableItf*>(mBuffer);

        return *this;
    }

    /**
     * Resets holding callable interface.
     */
    void Reset()
    {
        if (mCallable) {
            mCallable->~CallableItf();
            mCallable = nullptr;
        }
    }

    /**
     * Destructs function.
     */
    ~Function() { Reset(); }

    /**
     * Captures functor.
     *
     * @tparam T functor type.
     * @param functor functor.
     * @param arg functor argument.
     * @return Error.
     */
    template <typename T>
    Error Capture(T functor, void* arg = nullptr)
    {
        if (!mBuffer || sizeof(T) > mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        mCallable = new (mBuffer) Capturer<T>(functor, arg);

        return ErrorEnum::eNone;
    }

    /**
     * Calls captured functor.
     *
     * @return Error.
     */
    Error operator()()
    {
        if (!mCallable) {
            return ErrorEnum::eRuntime;
        }

        (*mCallable)();

        return ErrorEnum::eNone;
    }

    /**
     * Checks if function contains callable.
     */
    explicit operator bool() const { return mCallable != nullptr; }

protected:
    void SetBuffer(const Buffer& buffer)
    {
        mBuffer = buffer.Get();
        mMaxSize = buffer.Size();
    }

private:
    class CallableItf {
    public:
        virtual void   operator()() = 0;
        virtual size_t Size() = 0;
        virtual ~CallableItf() = default;
    };

    template <typename T>
    class Capturer : public CallableItf {
    public:
        Capturer(T& functor, void* arg)
            : mFunctor(functor)
            , mArg(arg)
            , mSize(sizeof(Capturer<T>))
        {
        }

        void operator()() override { mFunctor(mArg); }

        size_t Size() override { return mSize; }

    private:
        T      mFunctor;
        void*  mArg;
        size_t mSize;
    };

    void*        mBuffer = nullptr;
    size_t       mMaxSize = 0;
    CallableItf* mCallable = nullptr;
};

/**
 * Static function instance.
 *
 * @tparam cFunctionMaxSize max function size.
 */
template <size_t cFunctionMaxSize = cDefaultFunctionMaxSize>
class StaticFunction : public Function {
public:
    /**
     * Creates static function.
     */
    StaticFunction() { SetBuffer(mBuffer); }

    /**
     * Create static function from another static function.
     *
     * @param function static function to create from.
     */
    StaticFunction(const StaticFunction& function)
    {
        Function::SetBuffer(mBuffer);
        Function::operator=(function);
    }

    /**
     * Assigns one static function to another.
     *
     * @param function static function to assign from.
     * @return StaticFunction&.
     */
    StaticFunction& operator=(const StaticFunction& function)
    {
        Function::operator=(function);

        return *this;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static function from function.
     *
     * @param function function to create from.
     */
    StaticFunction(const Function& function)
    {
        Function::SetBuffer(mBuffer);
        Function::operator=(function);
    }

    /**
     * Assigns function to static function.
     *
     * @param function function to assign from.
     * @return StaticFunction&.
     */
    StaticFunction& operator=(const Function& function)
    {
        Function::operator=(function);

        return *this;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static function from functor.
     *
     * @tparam T functor type.
     * @param functor functor.
     * @param arg functor argument.
     */
    template <typename T>
    StaticFunction(T functor, void* arg = nullptr)
    {
        Function::SetBuffer(mBuffer);
        Function::Capture(functor, arg);
    }

private:
    StaticBuffer<cFunctionMaxSize> mBuffer;
};

} // namespace aos

#endif
