/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_VARIANT_HPP_
#define AOS_VARIANT_HPP_

namespace aos {

/**
 * Helper structure to get index of a given type in a type list.
 *
 * @param T input type.
 * @param Ts... type list.
 */
template <typename... Ts>
struct GetTypeIndex {
    static constexpr int Value = -1;
};

template <typename T, typename... Ts>
struct GetTypeIndex<T, T, Ts...> {
    static constexpr int Value = 0;
};

template <typename T, typename U, typename... Ts>
struct GetTypeIndex<T, U, Ts...> {
    static constexpr int Value = 1 + GetTypeIndex<T, Ts...>::Value;
};

/**
 * Base static visitor for the variant.
 */
template <typename T>
struct StaticVisitor {
    using Res = T;
};

/**
 * Visitor helper structures.
 */
template <typename... VarArgs>
class VisitorHelper;

template <>
class VisitorHelper<> {
public:
    template <typename Variant, typename Visitor>
    static typename Visitor::Res ApplyVisitor(int typeInd, const Visitor& visitor, Variant& variant)
    {
        (void)typeInd;
        (void)visitor;
        (void)variant;

        assert(false);

        exit(EXIT_FAILURE);
    }
};

template <typename T, typename... VarArgs>
class VisitorHelper<T, VarArgs...> {
public:
    template <typename Variant, typename Visitor>
    static typename Visitor::Res ApplyVisitor(int typeInd, const Visitor& visitor, Variant& variant)
    {
        if (typeInd == 0) {
            auto& val = StaticCast(variant.GetPtr());

            return visitor.Visit(val);
        } else {
            return VisitorHelper<VarArgs...>::ApplyVisitor(typeInd - 1, visitor, variant);
        }
    }

private:
    static const T& StaticCast(const void* ptr) { return *static_cast<const T*>(ptr); }
    static T&       StaticCast(void* ptr) { return *static_cast<T*>(ptr); }
};

/**
 * Class to store different objects int the same memory.
 */
template <typename... VarArgs>
class Variant {
public:
    /**
     * Sets new variant value.
     *
     * @param args... argument list for constructor of a new object.
     */
    template <typename T, typename... Args>
    void SetValue(Args... args)
    {
        DestroyObject();

        new (mBuffer) T(args...);

        mTypeIndex = GetTypeIndex<T, VarArgs...>::Value;
    }

    /**
     * Returns a value based on its type.
     *
     * @tparam T type to be returned.
     */
    template <typename T>
    T& GetValue()
    {
        [[maybe_unused]] static constexpr auto cTypeIndex = GetTypeIndex<T, VarArgs...>::Value;
        assert(mTypeIndex == cTypeIndex);

        return *reinterpret_cast<T*>(mBuffer);
    }

    /**
     * Returns a pointer to memory buffer.
     *
     * @return void*.
     */
    void* GetPtr() { return mBuffer; }

    /**
     * Returns a pointer to memory buffer.
     *
     * @return void*.
     */
    const void* GetPtr() const { return mBuffer; }

    /**
     * Applies static visitor to the current variant object.
     *
     * @param visitor input visitor.
     * @return Visitor::Res.
     */
    template <typename Visitor>
    typename Visitor::Res ApplyVisitor(const Visitor& visitor)
    {
        return VisitorHelper<VarArgs...>::ApplyVisitor(mTypeIndex, visitor, *this);
    }

    /**
     * Applies static visitor to the current variant object(const version).
     *
     * @param visitor input visitor.
     * @return Visitor::Res.
     */
    template <typename Visitor>
    typename Visitor::Res ApplyVisitor(const Visitor& visitor) const
    {
        return VisitorHelper<VarArgs...>::ApplyVisitor(mTypeIndex, visitor, *this);
    }

    /**
     * Destroys object instance.
     */
    ~Variant() { DestroyObject(); }

private:
    static constexpr int cInvalidTypeIndex = -1;

    struct ObjectDestroyer : StaticVisitor<void> {
        template <typename T>
        static void Visit(T& val)
        {
            val.~T();
        }
    };

    void DestroyObject()
    {
        if (mTypeIndex != cInvalidTypeIndex) {
            ApplyVisitor(ObjectDestroyer {});
            mTypeIndex = cInvalidTypeIndex;
        }
    }

    int mTypeIndex = cInvalidTypeIndex;
    alignas(VarArgs...) uint8_t mBuffer[Max(sizeof(VarArgs)...)];
};

/**
 * Helper structure for GetBase implementation.
 */
template <typename Base>
struct GetBaseHelper : StaticVisitor<Base> {
    template <typename T>
    Base& Visit(T& val) const
    {
        return static_cast<Base&>(val);
    }

    template <typename T>
    const Base& Visit(const T& val) const
    {
        return static_cast<const Base&>(val);
    }
};

/**
 * Returns base class for variant types.
 *
 * @tparam Base base class type.
 * @tparam Var variant type.
 * @param variant input variant type.
 * @return Base&
 */
template <typename Base, typename Var>
Base& GetBase(Var& variant)
{
    return variant.ApplyVisitor(GetBaseHelper<Base&> {});
}

/**
 * Returns base class for variant types.
 *
 * @tparam Base base class type.
 * @tparam Var variant type.
 * @param variant input variant type.
 * @return const Base&
 */
template <typename Base, typename Var>
const Base& GetBase(const Var& variant)
{
    return variant.ApplyVisitor(GetBaseHelper<const Base&> {});
}

} // namespace aos

#endif
