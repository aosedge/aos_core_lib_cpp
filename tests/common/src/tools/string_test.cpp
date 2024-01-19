/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/string.hpp"

using namespace aos;

TEST(StringTest, Basic)
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

    StaticString<4> convertStr;

    // Convert to int

    convertStr = "56";

    auto intResult = convertStr.ToInt();

    EXPECT_TRUE(intResult.mError.IsNone());
    EXPECT_EQ(intResult.mValue, 56);

    // Convert to uint64

    convertStr = "435";

    auto uint64Result = convertStr.ToUint64();

    EXPECT_TRUE(uint64Result.mError.IsNone());
    EXPECT_EQ(uint64Result.mValue, 435);

    // Convert to int64

    convertStr = "-123";

    auto int64Result = convertStr.ToInt64();

    EXPECT_TRUE(int64Result.mError.IsNone());
    EXPECT_EQ(int64Result.mValue, -123);

    // Convert from int

    EXPECT_TRUE(convertStr.Convert(42).IsNone());
    EXPECT_EQ(convertStr, "42");

    // Convert from uint64

    uint64_t v1 = 323;

    EXPECT_TRUE(convertStr.Convert(v1).IsNone());
    EXPECT_EQ(convertStr, "323");

    // Convert from int64

    int64_t v2 = -323;

    EXPECT_TRUE(convertStr.Convert(v2).IsNone());
    EXPECT_EQ(convertStr, "-323");

    // Convert from error

    StaticString<32> strErr;

    EXPECT_TRUE(strErr.Convert(Error(ErrorEnum::eFailed)).IsNone());
    EXPECT_EQ(strErr, "failed");

    EXPECT_TRUE(strErr.Convert(Error(ErrorEnum::eRuntime, "file1", 123)).IsNone());
    EXPECT_EQ(strErr, "runtime error (file1:123)");

    // Copy static string to static string

    StaticString<64> dst;
    StaticString<32> src("test string");

    dst = src;

    EXPECT_EQ(dst, src);
}

TEST(StringTest, StringArray)
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

TEST(StringTest, Split)
{
    StaticArray<StaticString<4>, 4> splitArray;

    String str = "aa bb cc dd";

    EXPECT_TRUE(str.Split(splitArray).IsNone());

    StaticString<4> resultList[] = {"aa", "bb", "cc", "dd"};

    Array<StaticString<4>> resultArray(resultList, ArraySize(resultList));

    EXPECT_EQ(splitArray, resultArray);
}

TEST(StringTest, HexToByteArray)
{
    const String hex = "abcDEF0123456789";

    StaticArray<uint8_t, 8> result;
    uint8_t                 expected[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89};

    ASSERT_TRUE(hex.HexToByteArray(result).IsNone());
    EXPECT_EQ(result, Array<uint8_t>(expected, sizeof(expected)));
}

TEST(StringTest, HexToByteArrayOddSize)
{
    const String hex = "01234";

    StaticArray<uint8_t, 8> result;
    uint8_t                 expected[] = {0x01, 0x23, 0x40};

    ASSERT_TRUE(hex.HexToByteArray(result).IsNone());
    EXPECT_EQ(result, Array<uint8_t>(expected, sizeof(expected)));
}

TEST(StringTest, HexToByteArrayNoMemory)
{
    const String hex = "01234";

    StaticArray<uint8_t, 2> result;

    ASSERT_EQ(hex.HexToByteArray(result), ErrorEnum::eNoMemory);
}

TEST(StringTest, ByteArrayToHex)
{
    const char expected[] = "ABCDEF0123456789";

    const uint8_t sourceArr[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89};
    const auto    source      = Array<uint8_t>(sourceArr, sizeof(sourceArr));

    StaticString<16> dst;

    ASSERT_TRUE(dst.ByteArrayToHex(source).IsNone());
    ASSERT_EQ(dst, expected);
}

TEST(StringTest, Format)
{
    StaticString<20> str;

    ASSERT_TRUE(str.Format("%s: %d", "id", 10).IsNone());
    ASSERT_EQ(str, "id: 10");
}

TEST(StringTest, Search)
{
    StaticString<40> str = "pkcs11:object=10;id=40";

    StaticString<20> object;
    StaticString<20> id;

    const char* regex = ".*object=([0-9]+).*id=([0-9]+)";

    ASSERT_TRUE(str.Search<1>(regex, object).IsNone());
    EXPECT_EQ(object, "10");

    ASSERT_TRUE(str.Search<2>(regex, id).IsNone());
    EXPECT_EQ(id, "40");

    ASSERT_EQ(str.Search<3>(regex, id), ErrorEnum::eNotFound);

    StaticString<1> smallId;
    ASSERT_EQ(str.Search<2>(regex, smallId), ErrorEnum::eNoMemory);
}

TEST(StringTest, Remove)
{
    const auto expected = "Thank you, sir";

    StaticString<100> str1 = "Thank you for removing me, sir";
    ASSERT_TRUE(str1.Remove(str1.begin() + 9, str1.begin() + 25).IsNone());
    ASSERT_EQ(str1, expected);

    StaticString<100> str2 = "Please remove me. Thank you, sir";
    ASSERT_TRUE(str2.Remove(str2.begin(), str2.begin() + 18).IsNone());
    ASSERT_EQ(str2, expected);

    StaticString<100> str3 = "Thank you, sir(for removing me)";
    ASSERT_TRUE(str3.Remove(str3.begin() + 14, str3.end()).IsNone());
    ASSERT_EQ(str3, expected);
}
