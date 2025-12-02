/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/types/alerts.hpp>
#include <core/common/types/blobs.hpp>
#include <core/common/types/certificates.hpp>
#include <core/common/types/desiredstatus.hpp>
#include <core/common/types/envvars.hpp>
#include <core/common/types/instance.hpp>
#include <core/common/types/log.hpp>
#include <core/common/types/monitoring.hpp>
#include <core/common/types/network.hpp>
#include <core/common/types/permissions.hpp>
#include <core/common/types/provisioning.hpp>
#include <core/common/types/state.hpp>
#include <core/common/types/unitconfig.hpp>
#include <core/common/types/unitstatus.hpp>

using namespace aos;

TEST(CommonTest, Types)
{
    // InstanceIdent comparision
    EXPECT_TRUE((InstanceIdent {"service1", "subject1", 2, UpdateItemTypeEnum::eService})
        == (InstanceIdent {"service1", "subject1", 2, UpdateItemTypeEnum::eService}));
    EXPECT_FALSE((InstanceIdent {"service1", "subject1", 2, UpdateItemTypeEnum::eService})
        != (InstanceIdent {"service1", "subject1", 2, UpdateItemTypeEnum::eService}));

    //  comparision
    EXPECT_TRUE((InstanceInfo {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "image1", "runc", {}, 2, 3, 4,
                    "state", "storage", {}, {}, {}})
        == (InstanceInfo {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "image1", "runc", {}, 2, 3, 4,
            "state", "storage", {}, {}, {}}));
    EXPECT_FALSE((InstanceInfo {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "image1", "runc", {}, 2, 3,
                     4, "state", "storage", {}, {}, {}})
        != (InstanceInfo {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "image1", "runc", {}, 2, 3, 4,
            "state", "storage", {}, {}, {}}));
    // InstanceStatus comparision
    EXPECT_TRUE((InstanceStatus {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "node0", "runc", "image0",
                    {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "3.0.0"})
        == (InstanceStatus {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "node0", "runc", "image0", {},
            {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "3.0.0"}));
    EXPECT_FALSE((InstanceStatus {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "node0", "runc", "image0",
                     {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "3.0.0"})
        != (InstanceStatus {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "node0", "runc", "image0", {},
            {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "3.0.0"}));
}
