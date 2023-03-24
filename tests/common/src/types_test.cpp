/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/types.hpp"

using namespace aos;

TEST(common, Types)
{
    // InstanceIdent comparision
    EXPECT_TRUE((InstanceIdent {"service1", "subject1", 2}) == (InstanceIdent {"service1", "subject1", 2}));
    EXPECT_FALSE((InstanceIdent {"service1", "subject1", 2}) != (InstanceIdent {"service1", "subject1", 2}));

    // InstanceInfo comparision
    EXPECT_TRUE((InstanceInfo {{"service1", "subject1", 2}, 3, 4, "state", "storage"})
        == (InstanceInfo {{"service1", "subject1", 2}, 3, 4, "state", "storage"}));
    EXPECT_FALSE((InstanceInfo {{"service1", "subject1", 2}, 3, 4, "state", "storage"})
        != (InstanceInfo {{"service1", "subject1", 2}, 3, 4, "state", "storage"}));

    // InstanceStatus comparision
    EXPECT_TRUE((InstanceStatus {{"service1", "subject1", 2}, 3, InstanceRunStateEnum::eActive, ErrorEnum::eNone})
        == (InstanceStatus {{"service1", "subject1", 2}, 3, InstanceRunStateEnum::eActive, ErrorEnum::eNone}));
    EXPECT_FALSE((InstanceStatus {{"service1", "subject1", 2}, 3, InstanceRunStateEnum::eActive, ErrorEnum::eNone})
        != (InstanceStatus {{"service1", "subject1", 2}, 3, InstanceRunStateEnum::eActive, ErrorEnum::eNone}));
}
