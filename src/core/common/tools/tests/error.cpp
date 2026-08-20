/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tools/error.hpp>

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

TEST(ErrorTest, ConstructFromEnumAndErrno)
{
    // Enum is preserved as given, independently of the errno value.

    auto err = Error(ErrorEnum::eNotFound, ENODEV);

    EXPECT_EQ(err.Value(), ErrorEnum::eNotFound);
    EXPECT_EQ(err.Errno(), ENODEV);
    EXPECT_EQ(strcmp(err.StrValue(), "not found"), 0);
    EXPECT_EQ(strcmp(err.StrErrno(), strerror(ENODEV)), 0);

    // Negative errno values are normalized to positive.

    auto negErr = Error(ErrorEnum::eFailed, -EAGAIN);

    EXPECT_EQ(negErr.Errno(), EAGAIN);

    // Message, file name and line number are propagated as with the other constructors.

    auto detailedErr = Error(ErrorEnum::eRuntime, ENODEV, "gRPC call failed", "file.cpp", 123);

    EXPECT_EQ(detailedErr.Value(), ErrorEnum::eRuntime);
    EXPECT_EQ(detailedErr.Errno(), ENODEV);
    EXPECT_EQ(strcmp(detailedErr.Message(), "gRPC call failed"), 0);
    EXPECT_EQ(strcmp(detailedErr.FileName(), "file.cpp"), 0);
    EXPECT_EQ(detailedErr.LineNumber(), 123);

    // With a non-zero errno set, Is() compares by errno rather than by enum, matching Error(int) behavior.

    EXPECT_TRUE(Error(ErrorEnum::eNotFound, ENODEV).Is(Error(ErrorEnum::eFailed, ENODEV)));
    EXPECT_FALSE(Error(ErrorEnum::eNotFound, ENODEV).Is(Error(ErrorEnum::eNotFound, EAGAIN)));

    // A zero errno keeps enum-based comparison, distinguishing it from Error(int) which maps errno 0 to eNone.

    EXPECT_TRUE(Error(ErrorEnum::eNotFound, 0).Is(ErrorEnum::eNotFound));
    EXPECT_FALSE(Error(ErrorEnum::eNotFound, 0).IsNone());
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
