/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <aos/iam/nodeinfoprovider.hpp>

using namespace testing;
using namespace aos::iam::nodeinfoprovider;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class NodeInfoProviderTest : public testing::Test { };

/***********************************************************************************************************************
 * Tests
 * **********************************************************************************************************************/

TEST_F(NodeInfoProviderTest, IsMainNodeReturnsFalseOnEmptyAttrs)
{
    aos::NodeInfo nodeInfo;

    EXPECT_FALSE(IsMainNode(nodeInfo)) << "Node has no main node attribute";
}

TEST_F(NodeInfoProviderTest, IsMainNodeReturnsTrue)
{
    aos::NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({"MainNode", ""});

    EXPECT_TRUE(IsMainNode(nodeInfo)) << "Node has main node attribute";
}

TEST_F(NodeInfoProviderTest, IsMainNodeReturnsTrueCaseInsensitive)
{
    aos::NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({"mainNODE", ""});

    EXPECT_TRUE(IsMainNode(nodeInfo)) << "Node has main node attribute";
}
