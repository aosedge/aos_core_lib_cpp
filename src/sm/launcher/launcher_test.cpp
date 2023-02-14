/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "launcher.hpp"

using aos::Error;
using aos::sm::launcher::Launcher;

TEST(launcher, RunInstances)
{
    Launcher launcher;

    EXPECT_TRUE(launcher.RunInstances().IsNone());
}
