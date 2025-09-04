/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/cloudprotocol/protocol.hpp>

namespace aos::cloudprotocol {

TEST(CloudprotocolTest, CloudMessage)
{
    const auto header = MessageHeader(1, "systemID");

    auto unitStatus = std::make_unique<UnitStatus>();
    auto message    = std::make_unique<MessageVariant>(*unitStatus);

    auto cloudMessage = std::make_unique<CloudMessage>(header, *message);

    EXPECT_EQ(cloudMessage->mHeader, header);
    EXPECT_EQ(cloudMessage->mData, *message);
}

} // namespace aos::cloudprotocol
