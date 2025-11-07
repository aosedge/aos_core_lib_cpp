/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/iam/nodeinfoprovider/nodeinfoprovider.hpp>

using namespace testing;

namespace aos::iam::nodeinfoprovider {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class NodeInfoProviderTest : public testing::Test { };

/***********************************************************************************************************************
 * Tests
 * **********************************************************************************************************************/

TEST_F(NodeInfoProviderTest, IsMainNodeReturnsFalseOnEmptyAttrs)
{
    NodeInfo nodeInfo;

    EXPECT_FALSE(IsMainNode(nodeInfo)) << "Node has no main node attribute";
}

TEST_F(NodeInfoProviderTest, IsMainNodeReturnsTrue)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({"MainNode", ""});

    EXPECT_TRUE(IsMainNode(nodeInfo)) << "Node has main node attribute";
}

TEST_F(NodeInfoProviderTest, IsMainNodeReturnsTrueCaseInsensitive)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({"mainNODE", ""});

    EXPECT_TRUE(IsMainNode(nodeInfo)) << "Node has main node attribute";
}

TEST_F(NodeInfoProviderTest, ContainsComponent)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({cAttrAosComponents, "cm,sm"});

    EXPECT_TRUE(ContainsComponent(nodeInfo, CoreComponentEnum::eCM)) << "Node has component CM";
    EXPECT_TRUE(ContainsComponent(nodeInfo, CoreComponentEnum::eSM)) << "Node has component SM";
    EXPECT_FALSE(ContainsComponent(nodeInfo, CoreComponentEnum::eIAM)) << "Node has no component IAM";
}

} // namespace aos::iam::nodeinfoprovider
