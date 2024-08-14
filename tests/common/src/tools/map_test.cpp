/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/common/tools/map.hpp"

using namespace aos;

template <typename T>
Array<T> ConvertToArray(const std::initializer_list<T>& list)
{
    return Array<T>(list.begin(), list.size());
}

TEST(MapTest, AssignArray)
{
    std::initializer_list<Pair<std::string, int>> source = {
        {"0xA", 10},
        {"0xB", 11},
        {"0xC", 12},
        {"0xD", 13},
        {"0xE", 14},
        {"0xF", 15},
    };

    StaticMap<std::string, int, 10> map;

    EXPECT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    EXPECT_EQ(map.Size(), 6);
    EXPECT_EQ(map.At("0xA").mValue, 10);
    EXPECT_EQ(map.At("0xB").mValue, 11);
    EXPECT_EQ(map.At("0xC").mValue, 12);
    EXPECT_EQ(map.At("0xD").mValue, 13);
    EXPECT_EQ(map.At("0xE").mValue, 14);
    EXPECT_EQ(map.At("0xF").mValue, 15);
}

TEST(MapTest, AssignArrayWithDuplicates)
{
    std::initializer_list<Pair<std::string, int>> source = {
        {"0xA", 1},
        {"0xB", 11},
        {"0xC", 12},
        {"0xA", 10},
    };

    StaticMap<std::string, int, 3> map;

    EXPECT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    EXPECT_EQ(map.Size(), 3);
    EXPECT_EQ(map.At("0xA").mValue, 10);
    EXPECT_EQ(map.At("0xB").mValue, 11);
    EXPECT_EQ(map.At("0xC").mValue, 12);
}

TEST(MapTest, AssignArrayNoMemory)
{
    std::initializer_list<Pair<std::string, int>> source = {
        {"0xA", 10},
        {"0xB", 11},
        {"0xC", 12},
    };

    StaticMap<std::string, int, 2> map;

    EXPECT_FALSE(map.Assign(ConvertToArray(source)).IsNone());
}

TEST(MapTest, AssignMap)
{
    std::initializer_list<Pair<std::string, int>> source = {
        {"0xA", 10},
        {"0xB", 11},
        {"0xC", 12},
    };

    StaticMap<std::string, int, 3> map1, map2;

    EXPECT_TRUE(map1.Assign(ConvertToArray(source)).IsNone());
    EXPECT_TRUE(map2.Assign(map1).IsNone());
    EXPECT_EQ(map1, map2);
}

TEST(MapTest, Set)
{
    std::initializer_list<Pair<std::string, int>> source = {
        {"0xA", 10},
        {"0xB", 11},
        {"0xC", 12},
    };
    StaticMap<std::string, int, 4> map;

    EXPECT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    // Set new value
    EXPECT_TRUE(map.Set("0xF", 15).IsNone());

    EXPECT_TRUE(map.At("0xF").mError.IsNone());
    EXPECT_EQ(map.At("0xF").mValue, 15);

    // Reset existing
    EXPECT_TRUE(map.Set("0xA", 1).IsNone());

    EXPECT_TRUE(map.At("0xA").mError.IsNone());
    EXPECT_EQ(map.At("0xA").mValue, 1);

    // Set no memory
    EXPECT_FALSE(map.Set("0xD", 13).IsNone());
}

TEST(MapTest, Remove)
{
    std::initializer_list<Pair<std::string, int>> source = {
        {"0xA", 10},
        {"0xB", 11},
        {"0xC", 12},
    };
    StaticMap<std::string, int, 4> map;

    EXPECT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    // Remove existing key
    EXPECT_TRUE(map.Remove("0xA").IsNone());
    EXPECT_FALSE(map.At("0xF").mError.IsNone());

    // Remove missing key: array doesn't return NotFound error. Map follows this behaviour.
    // EXPECT_FALSE(map.Remove("0xD").IsNone());
}

TEST(MapTest, Clear)
{
    std::initializer_list<Pair<std::string, int>> source = {
        {"0xA", 10},
        {"0xB", 11},
        {"0xC", 12},
    };
    StaticMap<std::string, int, 4> map;

    EXPECT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    map.Clear();
    EXPECT_EQ(map.Size(), 0);
}
