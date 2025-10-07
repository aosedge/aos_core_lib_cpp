/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/types/alerts.hpp>
#include <core/common/types/certificates.hpp>
#include <core/common/types/desiredstatus.hpp>
#include <core/common/types/envvars.hpp>
#include <core/common/types/log.hpp>
#include <core/common/types/monitoring.hpp>
#include <core/common/types/obsolete.hpp>
#include <core/common/types/provisioning.hpp>
#include <core/common/types/state.hpp>
#include <core/common/types/unitconfig.hpp>

using namespace aos;

TEST(CommonTest, Types)
{
    // InstanceIdent comparision
    EXPECT_TRUE((InstanceIdent {"service1", "subject1", 2}) == (InstanceIdent {"service1", "subject1", 2}));
    EXPECT_FALSE((InstanceIdent {"service1", "subject1", 2}) != (InstanceIdent {"service1", "subject1", 2}));

    //  comparision
    EXPECT_TRUE((InstanceInfo {{"service1", "subject1", 2}, "runc", 3, 4, "state", "storage", {}})
        == (InstanceInfo {{"service1", "subject1", 2}, "runc", 3, 4, "state", "storage", {}}));
    EXPECT_FALSE((InstanceInfo {{"service1", "subject1", 2}, "runc", 3, 4, "state", "storage", {}})
        != (InstanceInfo {{"service1", "subject1", 2}, "runc", 3, 4, "state", "storage", {}}));

    // InstanceStatus comparision
    EXPECT_TRUE((InstanceStatus {{"service1", "subject1", 2}, "3.0.0", "node0", "runc", {}, InstanceStateEnum::eActive,
                    ErrorEnum::eNone})
        == (InstanceStatus {
            {"service1", "subject1", 2}, "3.0.0", "node0", "runc", {}, InstanceStateEnum::eActive, ErrorEnum::eNone}));
    EXPECT_FALSE((InstanceStatus {{"service1", "subject1", 2}, "3.0.0", "node0", "runc", {}, InstanceStateEnum::eActive,
                     ErrorEnum::eNone})
        != (InstanceStatus {
            {"service1", "subject1", 2}, "3.0.0", "node0", "runc", {}, InstanceStateEnum::eActive, ErrorEnum::eNone}));
}
