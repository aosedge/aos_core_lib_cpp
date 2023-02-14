// SPDX-License-Identifier: Apache-2.0
//
// Copyright (C) 2023 Renesas Electronics Corporation.
// Copyright (C) 2023 EPAM Systems, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include "stringer.hpp"

class TestType {
public:
    enum class Enum { eTestDefault, eTestType1, eTestType2, eTestTypeSize };

    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const cTestTypeStrings[static_cast<size_t>(Enum::eTestTypeSize)]
            = {"default", "type1", "type2"};

        return Pair<const char* const*, size_t>(cTestTypeStrings, static_cast<size_t>(Enum::eTestTypeSize));
    };
};

typedef EnumStringer<TestType> TestInstance;
typedef TestType::Enum         TestEnum;

TEST(common, EnumStringer)
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

    EXPECT_EQ(strcmp(TestInstance().ToString(), "default"), 0);
    EXPECT_EQ(strcmp(TestInstance(TestEnum::eTestType1).ToString(), "type1"), 0);
    EXPECT_EQ(strcmp(TestInstance(static_cast<TestEnum>(-1)).ToString(), "unknown"), 0);
}
