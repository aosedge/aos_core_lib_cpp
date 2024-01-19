/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/enum.hpp"

using namespace aos;

class TestType {
public:
    enum class Enum { eTestDefault, eTestType1, eTestType2, eTestTypeSize };

    static const Array<const char* const> GetStrings()
    {
        static const char* const cTestTypeStrings[] = {"default", "type1", "type2"};

        return Array<const char* const>(cTestTypeStrings, ArraySize(cTestTypeStrings));
    };
};

using TestInstance = EnumStringer<TestType>;
using TestEnum     = TestType::Enum;

TEST(EnumTest, EnumStringer)
{
    // Check copying
    TestInstance e1;
    EXPECT_TRUE(e1.GetValue() == TestEnum::eTestDefault);

    TestInstance e2(TestEnum::eTestType1);
    e1 = e2;
    EXPECT_TRUE(e1 == TestEnum::eTestType1);

    e1 = TestEnum::eTestType2;
    EXPECT_TRUE(e1 == TestEnum::eTestType2);

    // Check comparisons

    EXPECT_TRUE(TestEnum(TestEnum::eTestType1) == TestEnum(TestEnum::eTestType1));
    EXPECT_TRUE(TestEnum(TestType::Enum::eTestType1) != TestEnum(TestEnum::eTestType2));
    EXPECT_TRUE(TestEnum(TestType::Enum::eTestType1) == TestEnum::eTestType1);
    EXPECT_TRUE(TestEnum(TestType::Enum::eTestType1) != TestEnum::eTestType2);
    EXPECT_TRUE(TestEnum::eTestType1 == TestEnum(TestEnum::eTestType1));
    EXPECT_TRUE(TestEnum::eTestType2 != TestEnum(TestEnum::eTestType1));

    EXPECT_EQ(TestInstance().ToString().CStr(), "default");
    EXPECT_EQ(TestInstance(TestEnum::eTestType1).ToString().CStr(), "type1");
    EXPECT_EQ(TestInstance(static_cast<TestEnum>(-1)).ToString().CStr(), "unknown");
}

TEST(EnumTest, EnumFromString)
{
    TestInstance e;

    auto err = e.FromString("type1");
    EXPECT_TRUE(err.IsNone());
    EXPECT_TRUE(e == TestEnum::eTestType1);

    err = e.FromString("type2");
    EXPECT_TRUE(err.IsNone());
    EXPECT_TRUE(e == TestEnum::eTestType2);

    err = e.FromString("unknown");
    EXPECT_FALSE(err.IsNone());
    EXPECT_TRUE(e == TestEnum::eTestTypeSize);
}
