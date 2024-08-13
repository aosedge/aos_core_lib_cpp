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
    RetWithError<Value&> At(const Key& key);

    /**
     * Returns reference to the value with specified key. And an error if a key is absent.
     *
     * @param key
     * @return RetWithError<const Value&>
     */
    RetWithError<const Value&> At(const Key& key) const;

    /**
     * Replaces map with elements from the array.
     *
     * @param array source array
     * @return Error
     */
    Error Assign(const Array<Pair<Key, Value>>& array);

    /**
     * Replaces map elements with a copy of the elements from other map.
     *
     * @param map source map
     * @return Error
     */
    Error Assign(const Map<Key, Value>& map);

    /**
     * Inserts or replaces a value with a specified key.
     *
     * @param key
     * @param value
     * @return Error
     */
    Error Set(const Key& key, const Value& value);

    /**
     * Removes element with the specified key from the map.
     *
     * @param key
     * @return Error
     */
    Error Remove(const Key& key);

    /**
     * Removes all elements from the map.
     */
    void Clear();

    /**
     * Returns current number of elements in the map.
     *
     * @return size_t.
     */
    size_t Size() const;

    /**
     * Returns maximum allowed number of elements in the map.
     *
     * @return size_t.
     */
    size_t MaxSize() const;

    /**
     * Compares contents of two maps.
     *
     * @param other map to be compared with.
     * @return bool
     */
    bool operator==(const Map<Key, Value>& other) const;

    /**
     * Compares contents of two maps.
     *
     * @param other map to be compared with.
     * @return bool
     */
    bool operator!=(const Map<Key, Value>& other) const;

    /**
     * Iterators to the beginning / end of the map
     */
    Pair<Key, Value>*       begin();
    Pair<Key, Value>*       end();
    const Pair<Key, Value>* begin() const;
    const Pair<Key, Value>* end() const;

protected:
    Map(Array<Pair<Key, Value>>& array);
};

} // namespace aos

#endif
