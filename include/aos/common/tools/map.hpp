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
class Map {
public:
    /**
     * Returns a reference to the value with specified key. And an error if a key is absent.
     *
     * @param key
     * @return RetWithError<Value&>
     */
    RetWithError<Value&> At(const Key& key)
    {
        auto item = mItems.Find([&key](const Pair<Key, Value>& item) { return item.mFirst == key; });

        if (!item.mError.IsNone()) {
            return {*static_cast<Value*>(nullptr), item.mError};
        }

        return {item.mValue->mSecond, item.mError};
    }

    /**
     * Returns reference to the value with specified key. And an error if a key is absent.
     *
     * @param key
     * @return RetWithError<const Value&>
     */
    RetWithError<const Value&> At(const Key& key) const { return const_cast<Map<Key, Value>&>(*this).At(key); }

    /**
     * Replaces map with elements from the array.
     *
     * @param array source array
     * @return Error
     */
    Error Assign(const Array<Pair<Key, Value>>& array)
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
     * @param map source map
     * @return Error
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
     * @param key
     * @param value
     * @return Error
     */
    Error Set(const Key& key, const Value& value)
    {
        auto cur = At(key);
        if (cur.mError.IsNone()) {
            cur.mValue = value;

            return ErrorEnum::eNone;
        }

        return mItems.EmplaceBack(key, value);
    }

    /**
     * Removes element with the specified key from the map.
     *
     * @param key
     * @return Error
     */
    Error Remove(const Key& key)
    {
        const auto matchKey = [&key](const Pair<Key, Value>& item) { return item.mFirst == key; };

        return mItems.Remove(matchKey).mError;
    }

    /**
     * Removes all elements from the map.
     */
    void Clear() { mItems.Clear(); }

    /**
     * Returns current number of elements in the map.
     *
     * @return size_t.
     */
    size_t Size() const { return mItems.Size(); }

    /**
     * Returns maximum allowed number of elements in the map.
     *
     * @return size_t.
     */
    size_t MaxSize() const { return mItems.MaxSize(); }

    /**
     * Compares contents of two maps.
     *
     * @param other map to be compared with.
     * @return bool
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
     * @return bool
     */
    bool operator!=(const Map<Key, Value>& other) const { return !(*this == other); }

    /**
     * Iterators to the beginning / end of the map
     */
    Pair<Key, Value>*       begin() { return mItems.begin(); }
    Pair<Key, Value>*       end() { return mItems.end(); }
    const Pair<Key, Value>* begin() const { return mItems.begin(); }
    const Pair<Key, Value>* end() const { return mItems.end(); }

protected:
    Map(Array<Pair<Key, Value>>& array)
        : mItems(array)
    {
    }

private:
    Array<Pair<Key, Value>>& mItems;
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
