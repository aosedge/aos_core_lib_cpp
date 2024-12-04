/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_MAP_HPP_
#define AOS_MAP_HPP_

#include "aos/common/tools/array.hpp"
#include "aos/common/tools/utils.hpp"

namespace aos {

/**
 * Simple map implementation based on a non sorted array.
 *
 * @tparam Key type of keys.
 * @tparam Value type of values.
 */

template <typename Key, typename Value>
class Map : public AlgorithmItf<Pair<Key, Value>, Pair<Key, Value>*, const Pair<Key, Value>*> {
public:
    using ValueType     = Pair<Key, Value>;
    using Iterator      = ValueType*;
    using ConstIterator = const ValueType*;

    /**
     * Finds element in map by key.
     *
     * @param key key to find.
     * @return RetWithError<Iterator>.
     */
    RetWithError<Iterator> Find(const Key& key)
    {
        for (auto it = begin(); it != end(); ++it) {
            if (it->mFirst == key) {
                return it;
            }
        }

        return {end(), ErrorEnum::eNotFound};
    }

    /**
     * Returns a reference to the value with specified key. And an error if a key is absent.
     *
     * @param key key to find.
     * @return RetWithError<Value&>.
     */
    RetWithError<Value&> At(const Key& key)
    {
        auto item = mItems.FindIf([&key](const ValueType& item) { return item.mFirst == key; });

        return {item.mValue->mSecond, item.mError};
    }

    /**
     * Returns reference to the value with specified key. And an error if a key is absent.
     *
     * @param key.
     * @return RetWithError<const Value&>.
     */
    RetWithError<const Value&> At(const Key& key) const
    {
        auto item = mItems.FindIf([&key](const ValueType& item) { return item.mFirst == key; });

        return {item.mValue->mSecond, item.mError};
    }

    /**
     * Replaces map with elements from the array.
     *
     * @param array source array.
     * @return Error.
     */
    Error Assign(const Array<ValueType>& array)
    {
        mItems.Clear();

        for (const auto& [key, value] : array) {
            auto err = Set(key, value);
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Replaces map elements with a copy of the elements from other map.
     *
     * @param map source map.
     * @return Error.
     */
    Error Assign(const Map<Key, Value>& map)
    {
        if (mItems.MaxSize() < map.mItems.Size()) {
            return ErrorEnum::eNoMemory;
        }

        mItems = map.mItems;

        return ErrorEnum::eNone;
    }

    /**
     * Inserts or replaces a value with a specified key.
     *
     * @param key key to insert.
     * @param value value to insert.
     * @return Error.
     */
    Error Set(const Key& key, const Value& value)
    {
        auto cur = At(key);
        if (cur.mError.IsNone()) {
            // cppcheck-suppress unreadVariable
            cur.mValue = value;

            return ErrorEnum::eNone;
        }

        return mItems.EmplaceBack(key, value);
    }

    /**
     * Inserts a new value into the map.
     *
     * @param args list of arguments to construct a new element.
     * @return Error.
     */
    template <typename... Args>
    Error Emplace(Args&&... args)
    {
        const auto kv = ValueType(args...);

        auto cur = At(kv.mFirst);
        if (!cur.mError.IsNone()) {
            return mItems.EmplaceBack(kv);
        }

        return ErrorEnum::eInvalidArgument;
    }

    /**
     * Removes element with the specified key from the map.
     *
     * @param key key to remove.
     * @return Error.
     */
    Error Remove(const Key& key)
    {
        const auto matchKey = [&key](const ValueType& item) { return item.mFirst == key; };

        return mItems.RemoveIf(matchKey) ? ErrorEnum::eNone : ErrorEnum::eNotFound;
    }

    /**
     * Checks if the map contains an element with the specified key.
     *
     * @param key key to check.
     * @return bool.
     */
    bool Contains(const Key& key) const { return At(key).mError.IsNone(); }

    /**
     * Removes all elements from the map.
     */
    void Clear() { mItems.Clear(); }

    /**
     * Returns current number of elements in the map.
     *
     * @return size_t.
     */
    size_t Size() const override { return mItems.Size(); }

    /**
     * Returns maximum allowed number of elements in the map.
     *
     * @return size_t.
     */
    size_t MaxSize() const override { return mItems.MaxSize(); }

    /**
     * Compares contents of two maps.
     *
     * @param other map to be compared with.
     * @return bool.
     */
    bool operator==(const Map<Key, Value>& other) const
    {
        if (Size() != other.Size()) {
            return false;
        }

        for (const auto& item : other) {
            if (!mItems.Find(item).mError.IsNone()) {
                return false;
            }
        }

        return true;
    }

    /**
     * Compares contents of two maps.
     *
     * @param other map to be compared with.
     * @return bool.
     */
    bool operator!=(const Map<Key, Value>& other) const { return !(*this == other); }

    /**
     * Erases items range from map.
     *
     * @param first first item to erase.
     * @param first last item to erase.
     * @return next after deleted item iterator.
     */
    Iterator Erase(ConstIterator first, ConstIterator last) override { return mItems.Erase(first, last); }

    /**
     * Erases item from map.
     *
     * @param it item to erase.
     * @return next after deleted item iterator.
     */
    Iterator Erase(ConstIterator it) override { return mItems.Erase(it); }

    /**
     * Iterators to the beginning / end of the map
     */
    Iterator      begin() override { return mItems.begin(); }
    Iterator      end() override { return mItems.end(); }
    ConstIterator begin() const override { return mItems.begin(); }
    ConstIterator end() const override { return mItems.end(); }

protected:
    explicit Map(Array<ValueType>& array)
        : mItems(array)
    {
    }

private:
    Array<ValueType>& mItems;
};

/**
 * Map with static capacity.
 *
 * @tparam Key type of keys.
 * @tparam Value type of values.
 * @tparam cMaxSize max size.
 */
template <typename Key, typename Value, size_t cMaxSize>
class StaticMap : public Map<Key, Value> {
public:
    StaticMap()
        : Map<Key, Value>(mArray)
    {
    }

private:
    StaticArray<Pair<Key, Value>, cMaxSize> mArray;
};

} // namespace aos

#endif
