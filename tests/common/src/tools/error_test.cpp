/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/error.hpp"

using namespace aos;

Error successFunction()
{
    return ErrorEnum::eNone;
}

Error failedFunction()
{
    return ErrorEnum::eFailed;
}

TEST(ErrorTest, Basic)
{
    // Compare errors

    EXPECT_TRUE(Error(ErrorEnum::eFailed).Is(ErrorEnum::eFailed));
    EXPECT_TRUE(Error(ErrorEnum::eFailed).Is(Error(ErrorEnum::eFailed)));
    EXPECT_TRUE(Error(ErrorEnum::eFailed) == ErrorEnum::eFailed);
    EXPECT_TRUE(Error(ErrorEnum::eFailed) != ErrorEnum::eNone);
    EXPECT_TRUE(ErrorEnum::eFailed == Error(ErrorEnum::eFailed));
    EXPECT_TRUE(ErrorEnum::eNone != Error(ErrorEnum::eFailed));

    // Function handling

    EXPECT_TRUE(successFunction().IsNone());
    auto successErr = successFunction();
    EXPECT_FALSE(!successErr.IsNone());
    EXPECT_FALSE(failedFunction().IsNone());
    auto failedErr = failedFunction();
    EXPECT_TRUE(!failedErr.IsNone());

    // Error string

    EXPECT_EQ(strcmp(Error(ErrorEnum::eNone).StrValue(), "none"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eFailed).StrValue(), "failed"), 0);

    // Errno handling

    EXPECT_TRUE(Error(0).IsNone());
    EXPECT_FALSE(Error(EINVAL).IsNone());
    EXPECT_TRUE(Error(ENODEV).Is(ENODEV));
    EXPECT_EQ(strcmp(Error(EAGAIN).StrErrno(), "Resource temporarily unavailable"), 0);

    // Error wrap

    auto funcErr = AOS_ERROR_WRAP(failedFunction());
    EXPECT_EQ(funcErr.LineNumber(), __LINE__ - 1);
    EXPECT_EQ(strcmp(funcErr.StrValue(), "failed"), 0);

    auto enumErr = AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    EXPECT_EQ(enumErr.LineNumber(), __LINE__ - 1);
    EXPECT_EQ(strcmp(enumErr.StrValue(), "not enough memory"), 0);

    auto copyErr(enumErr);
    EXPECT_EQ(copyErr.LineNumber(), enumErr.LineNumber());
    EXPECT_EQ(strcmp(copyErr.StrValue(), enumErr.StrValue()), 0);

    Error assignErr;

    assignErr = copyErr;
    EXPECT_EQ(assignErr.LineNumber(), enumErr.LineNumber());
    EXPECT_EQ(strcmp(assignErr.StrValue(), enumErr.StrValue()), 0);

    auto errnoErr = AOS_ERROR_WRAP(EAGAIN);
    EXPECT_EQ(errnoErr.LineNumber(), __LINE__ - 1);
    EXPECT_EQ(strcmp(errnoErr.StrErrno(), "Resource temporarily unavailable"), 0);
}

TEST(ErrorTest, Messages)
{
    EXPECT_EQ(strcmp(Error(ErrorEnum::eNone).Message(), ""), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eFailed, "Failed Msg").Message(), "Failed Msg"), 0);

    auto err = Error(ErrorEnum::eFailed, "Failed Msg", "file.cpp", 123);

    EXPECT_EQ(strcmp(err.Message(), "Failed Msg"), 0);
    EXPECT_EQ(strcmp(err.FileName(), "file.cpp"), 0);
    EXPECT_EQ(err.LineNumber(), 123);

    err = Error(EAGAIN, "Problem with resource", "file.cpp", 123);

    EXPECT_EQ(strcmp(err.Message(), "Problem with resource"), 0);
    EXPECT_EQ(strcmp(err.FileName(), "file.cpp"), 0);
    EXPECT_EQ(err.LineNumber(), 123);
    EXPECT_EQ(strcmp(err.StrErrno(), "Resource temporarily unavailable"), 0);
}

TEST(ErrorTest, Tie)
{
    Error err = ErrorEnum::eNone;
    bool  val = false;

    Tie(val, err) = RetWithError<bool>(true, ErrorEnum::eFailed);

    EXPECT_EQ(val, true);
    EXPECT_EQ(err, ErrorEnum::eFailed);
}
