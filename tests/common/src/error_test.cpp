/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/error.hpp"

using namespace aos;

Error successFunction()
{
    return ErrorEnum::eNone;
}

Error failedFunction()
{
    return ErrorEnum::eFailed;
}

TEST(common, Error)
{
    // Compare errors

    EXPECT_TRUE(Error(ErrorEnum::eFailed).Is(ErrorEnum::eFailed));
    EXPECT_TRUE(Error(ErrorEnum::eFailed).Is(Error(ErrorEnum::eFailed)));

    // Function handling

    EXPECT_TRUE(successFunction().IsNone());
    auto successErr = successFunction();
    EXPECT_FALSE(!successErr.IsNone());
    EXPECT_FALSE(failedFunction().IsNone());
    auto failedErr = failedFunction();
    EXPECT_TRUE(!failedErr.IsNone());

    // Error string

    EXPECT_EQ(strcmp(Error(ErrorEnum::eNone).ToString(), "none"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eFailed).ToString(), "failed"), 0);

    // Errno handling

    EXPECT_TRUE(Error(0).IsNone());
    EXPECT_FALSE(Error(EINVAL).IsNone());
    EXPECT_TRUE(Error(ENODEV).Is(ENODEV));

    EXPECT_EQ(strcmp(Error(EAGAIN).ToString(), "Resource temporarily unavailable"), 0);
}
