/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/types/types.hpp>

using namespace aos;

TEST(CommonTest, Types)
{
    // InstanceIdent comparision
    EXPECT_TRUE(
        (InstanceIdentObsolete {"service1", "subject1", 2}) == (InstanceIdentObsolete {"service1", "subject1", 2}));
    EXPECT_FALSE(
        (InstanceIdentObsolete {"service1", "subject1", 2}) != (InstanceIdentObsolete {"service1", "subject1", 2}));

    // InstanceInfo comparision
    EXPECT_TRUE((InstanceInfoObsolete {{"service1", "subject1", 2}, 3, 4, "state", "storage", {}})
        == (InstanceInfoObsolete {{"service1", "subject1", 2}, 3, 4, "state", "storage", {}}));
    EXPECT_FALSE((InstanceInfoObsolete {{"service1", "subject1", 2}, 3, 4, "state", "storage", {}})
        != (InstanceInfoObsolete {{"service1", "subject1", 2}, 3, 4, "state", "storage", {}}));

    // InstanceStatus comparision
    EXPECT_TRUE((InstanceStatusObsolete {
                    {"service1", "subject1", 2}, "3.0.0", "", InstanceRunStateEnum::eActive, "", ErrorEnum::eNone})
        == (InstanceStatusObsolete {
            {"service1", "subject1", 2}, "3.0.0", "", InstanceRunStateEnum::eActive, "", ErrorEnum::eNone}));
    EXPECT_FALSE((InstanceStatusObsolete {
                     {"service1", "subject1", 2}, "3.0.0", "", InstanceRunStateEnum::eActive, "", ErrorEnum::eNone})
        != (InstanceStatusObsolete {
            {"service1", "subject1", 2}, "3.0.0", "", InstanceRunStateEnum::eActive, "", ErrorEnum::eNone}));
}
