/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/iam/certhandler.hpp"

using aos::Error;
using aos::iam::certhandler::CertHandler;

TEST(CertHandlerTest, CreateKey)
{
    CertHandler handler;

    EXPECT_TRUE(handler.CreateKey().IsNone());
}
