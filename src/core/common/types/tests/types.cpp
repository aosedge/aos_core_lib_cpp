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
    // InstanceIdent comparison
    EXPECT_TRUE((InstanceIdent {"service1", "subject1", 2, UpdateItemTypeEnum::eService})
        == (InstanceIdent {"service1", "subject1", 2, UpdateItemTypeEnum::eService}));
    EXPECT_FALSE((InstanceIdent {"service1", "subject1", 2, UpdateItemTypeEnum::eService})
        != (InstanceIdent {"service1", "subject1", 2, UpdateItemTypeEnum::eService}));

    // InstanceInfo comparison
    EXPECT_TRUE((InstanceInfo {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "1.0.0", "image1", "runc", "",
                    {}, 2, 3, 4, "state", "storage", {}, {}})
        == (InstanceInfo {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "1.0.0", "image1", "runc", "", {},
            2, 3, 4, "state", "storage", {}, {}}));
    EXPECT_FALSE((InstanceInfo {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "1.0.0", "image1", "runc",
                     "", {}, 2, 3, 4, "state", "storage", {}, {}})
        != (InstanceInfo {{"service1", "subject1", 2, UpdateItemTypeEnum::eService}, "1.0.0", "image1", "runc", "", {},
            2, 3, 4, "state", "storage", {}, {}}));

    // InstanceStatus comparison
    EXPECT_TRUE((InstanceStatus {"service1", "subject1", 2, UpdateItemTypeEnum::eService, false, "node0", "runc",
                    "image0", {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "3.0.0"})
        == (InstanceStatus {"service1", "subject1", 2, UpdateItemTypeEnum::eService, false, "node0", "runc", "image0",
            {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "3.0.0"}));
    EXPECT_FALSE((InstanceStatus {"service1", "subject1", 2, UpdateItemTypeEnum::eService, false, "node0", "runc",
                     "image0", {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "3.0.0"})
        != (InstanceStatus {"service1", "subject1", 2, UpdateItemTypeEnum::eService, false, "node0", "runc", "image0",
            {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, "3.0.0"}));
}

TEST(CommonTest, NodeRootCertificatesComparison)
{
    NodeRootCertificates rootCert1;
    NodeRootCertificates rootCert2;

    rootCert1.mNodeID = "node0";
    ASSERT_TRUE(rootCert1.mSHA256Fingerprints.EmplaceBack("fingerprint0").IsNone());

    rootCert2 = rootCert1;

    EXPECT_EQ(rootCert1, rootCert2);

    rootCert2.mNodeID = "node1";

    EXPECT_NE(rootCert1, rootCert2);
}

TEST(CommonTest, UnitRootCertificatesComparison)
{
    UnitRootCertificates rootCert1;
    UnitRootCertificates rootCert2;

    rootCert1.mCorrelationID = "correlation0";
    rootCert1.mIsPartial     = true;
    ASSERT_TRUE(rootCert1.mNodeCertificates.EmplaceBack().IsNone());
    rootCert1.mNodeCertificates.Back().mNodeID = "node0";

    rootCert2 = rootCert1;

    EXPECT_EQ(rootCert1, rootCert2);

    rootCert2.mIsPartial = false;

    EXPECT_NE(rootCert1, rootCert2);
}

TEST(CommonTest, DesiredNodeRootCertificatesComparison)
{
    auto rootCert1 = std::make_unique<DesiredNodeRootCertificates>();
    auto rootCert2 = std::make_unique<DesiredNodeRootCertificates>();

    rootCert1->mNodeID = "node0";
    ASSERT_TRUE(rootCert1->mCertificates.EmplaceBack("pem0").IsNone());

    *rootCert2 = *rootCert1;

    EXPECT_EQ(*rootCert1, *rootCert2);

    rootCert2->mCertificates[0] = "pem1";

    EXPECT_NE(*rootCert1, *rootCert2);
}

TEST(CommonTest, DesiredUnitRootCertificatesComparison)
{
    auto rootCert1 = std::make_unique<DesiredUnitRootCertificates>();
    auto rootCert2 = std::make_unique<DesiredUnitRootCertificates>();

    rootCert1->mCorrelationID = "correlation0";
    ASSERT_TRUE(rootCert1->mNodeCertificates.EmplaceBack().IsNone());
    rootCert1->mNodeCertificates.Back().mNodeID = "node0";

    *rootCert2 = *rootCert1;

    EXPECT_EQ(*rootCert1, *rootCert2);

    rootCert2->mCorrelationID = "correlation1";

    EXPECT_NE(*rootCert1, *rootCert2);
}

TEST(CommonTest, IsMainNodeReturnsFalseOnEmptyAttrs)
{
    NodeInfo nodeInfo;

    EXPECT_FALSE(nodeInfo.IsMainNode()) << "Node has no main node attribute";
}

TEST(CommonTest, IsMainNodeReturnsTrue)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({"MainNode", ""});

    EXPECT_TRUE(nodeInfo.IsMainNode()) << "Node has main node attribute";
}

TEST(CommonTest, IsMainNodeReturnsTrueCaseInsensitive)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({"mainNODE", ""});

    EXPECT_TRUE(nodeInfo.IsMainNode()) << "Node has main node attribute";
}

TEST(CommonTest, NodeContainsComponent)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({cAttrAosComponents, "cm,sm"});

    EXPECT_TRUE(nodeInfo.ContainsComponent(CoreComponentEnum::eCM)) << "Node has component CM";
    EXPECT_TRUE(nodeInfo.ContainsComponent(CoreComponentEnum::eSM)) << "Node has component SM";
    EXPECT_FALSE(nodeInfo.ContainsComponent(CoreComponentEnum::eIAM)) << "Node has no component IAM";
}
