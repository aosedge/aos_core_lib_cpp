/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <gmock/gmock.h>

#include "aos/common/tools/string.hpp"
#include "aos/common/tools/variant.hpp"

using namespace testing;
using namespace aos;

TEST(VariantTest, GetterTest)
{
    using BigString = StaticString<50>;

    Variant<bool, int, BigString> variant;

    variant.SetValue<int>(20);
    ASSERT_EQ(variant.GetValue<int>(), 20);

    variant.SetValue<bool>(false);
    ASSERT_EQ(variant.GetValue<bool>(), false);

    const auto cTmpString = "Hello world";
    variant.SetValue<BigString>(cTmpString);
    ASSERT_EQ(variant.GetValue<BigString>(), cTmpString);
}

TEST(VariantTest, TestDestructorCalled)
{
    MockFunction<void()> func;

    class Foo {
    public:
        Foo(MockFunction<void()>* func)
            : mFunc(func)
        {
        }
        ~Foo() { mFunc->Call(); }

    private:
        MockFunction<void()>* mFunc;
    };

    EXPECT_CALL(func, Call());
    {
        Variant<bool, Foo, int> variant;

        variant.SetValue<Foo>(&func);
    }
}

TEST(VariantTest, GetBaseTest)
{
    struct Foo {
        virtual StaticString<20> GetName() const = 0;
    };

    struct Child1 : public Foo {
        StaticString<20> GetName() const override { return "Child1"; }
    };

    struct Child2 : public Foo {
        StaticString<20> GetName() const override { return "Child2"; }
    };

    using AnyChild = Variant<Child1, Child2>;
    AnyChild variant;

    variant.SetValue<Child2>();
    variant.SetValue<Child1>();

    EXPECT_EQ(GetBase<Foo>(variant).GetName(), "Child1");
    EXPECT_EQ(GetBase<Foo>(const_cast<const AnyChild&>(variant)).GetName(), "Child1");
}

TEST(VariantTest, CopyVariant)
{
    auto ptr = std::make_shared<std::string>("Hello");

    Variant<std::shared_ptr<std::string>, int> variant1;
    variant1.SetValue<std::shared_ptr<std::string>>(ptr);

    EXPECT_TRUE(ptr.use_count() == 2);

    auto variant2 = variant1;

    EXPECT_TRUE(ptr.use_count() == 3);
}

TEST(VariantTest, CompareVariant)
{
    auto ptr = std::make_shared<std::string>("Hello");

    Variant<std::shared_ptr<std::string>, int> variant1 {ptr};

    auto variant2 = variant1;

    EXPECT_EQ(variant1, variant2);

    variant2.SetValue(10);

    EXPECT_NE(variant1, variant2);
}
