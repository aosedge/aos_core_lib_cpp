/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/string.hpp"

using namespace aos;

TEST(CommonTest, String)
{
    StaticString<32> str;

    EXPECT_TRUE(str.IsEmpty());
    EXPECT_EQ(str.Size(), 0);

    char cStr[20];

    strcpy(cStr, "test C string");

    str = cStr;

    EXPECT_FALSE(str.IsEmpty());
    EXPECT_EQ(str.Size(), strlen(cStr));
    EXPECT_EQ(strcmp(str.CStr(), cStr), 0);

    const String constStr(cStr);

    EXPECT_EQ(constStr.Size(), strlen(cStr));
    EXPECT_EQ(strcmp(constStr.CStr(), cStr), 0);

    StaticString<16> anotherStr("another string");

    str = anotherStr;

    // Compare operators

    EXPECT_TRUE(str == anotherStr);
    EXPECT_TRUE(anotherStr == str);

    EXPECT_TRUE(str != constStr);
    EXPECT_TRUE(constStr != str);

    str.Clear();

    EXPECT_TRUE(str != constStr);
    EXPECT_TRUE(constStr != str);

    // Test append

    str.Append("test1");
    str += "test2";

    EXPECT_EQ(str, "test1test2");

    // Convert int

    StaticString<2> strInt;

    EXPECT_EQ(strInt.Convert(42), "42");

    // Convert error

    StaticString<32> strErr;

    EXPECT_EQ(strErr.Convert(Error(ErrorEnum::eFailed)), "failed");
    EXPECT_EQ(strErr.Convert(Error(ErrorEnum::eRuntime, "file1", 123)), "runtime error (file1:123)");

    // Copy static string to static string

    StaticString<64> dst;
    StaticString<32> src("test string");

    dst = src;

    EXPECT_EQ(dst, src);
}

TEST(CommonTest, StringArray)
{
    struct TestStruct {
        StaticString<32> str1;
        StaticString<32> str2;
    };

    StaticArray<TestStruct, 8> strArray;

    strArray.Resize(1);

    strArray[0].str1 = "test1";
    strArray[0].str2 = "test2";

    EXPECT_EQ(strArray[0].str1, "test1");
    EXPECT_EQ(strArray[0].str2, "test2");
}

TEST(CommonTest, SplitString)
{
    StaticArray<StaticString<4>, 4> splitArray;

    String str = "aa bb cc dd";

    EXPECT_TRUE(str.Split(splitArray).IsNone());

    StaticString<4> resultList[] = {"aa", "bb", "cc", "dd"};

    Array<StaticString<4>> resultArray(resultList, ArraySize(resultList));

    EXPECT_EQ(splitArray, resultArray);
}
