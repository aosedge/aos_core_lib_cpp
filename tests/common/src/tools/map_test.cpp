/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/common/tools/map.hpp"
#include "aos/common/tools/string.hpp"

using namespace aos;

template <typename T>
Array<T> static ConvertToArray(const std::initializer_list<T>& list)
{
    return Array<T>(list.begin(), list.size());
}

TEST(MapTest, FindByKey)
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

    ASSERT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    for (const auto& [key, value] : source) {
        auto item = map.Find(key);

        ASSERT_TRUE(item.mError.IsNone());
        EXPECT_EQ(item.mValue->mFirst, key);
        EXPECT_EQ(item.mValue->mSecond, value);
    }
}

TEST(MapTest, Erase)
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

    ASSERT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    ASSERT_EQ(map.Size(), source.size());

    auto it = map.Find("0xC");
    ASSERT_TRUE(it.mError.IsNone());

    ASSERT_NE(map.Erase(it.mValue), map.end());
    ASSERT_EQ(map.Size(), source.size() - 1);

    ASSERT_FALSE(map.Contains("0xC"));
}

TEST(MapTest, Contains)
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

    ASSERT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    for (const auto& [key, value] : source) {
        EXPECT_TRUE(map.Contains(key));
    }

    EXPECT_FALSE(map.Contains("not found"));
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

TEST(MapTest, Emplace)
{
    std::initializer_list<Pair<std::string, int>> source = {
        {"0xA", 10},
        {"0xB", 11},
        {"0xC", 12},
    };
    StaticMap<std::string, int, 4> map;

    EXPECT_TRUE(map.Assign(ConvertToArray(source)).IsNone());

    // Emplace new value
    EXPECT_TRUE(map.Emplace("0xF", 15).IsNone());

    EXPECT_TRUE(map.At("0xF").mError.IsNone());
    EXPECT_EQ(map.At("0xF").mValue, 15);

    // Fail emplace with existing value
    EXPECT_FALSE(map.Emplace("0xA", 1).IsNone());

    // Emplace no memory
    EXPECT_FALSE(map.Emplace("0xD", 13).IsNone());
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

TEST(MapTest, Const)
{
    class TestClass { };

    StaticMap<StaticString<16>, TestClass, 4> map;

    auto constCbk
        = [](const Map<StaticString<16>, TestClass>& map) -> RetWithError<const TestClass&> { return map.At("test"); };

    EXPECT_TRUE(map.Emplace("test", TestClass {}).IsNone());
    EXPECT_TRUE(constCbk(map).mError.IsNone());
}
