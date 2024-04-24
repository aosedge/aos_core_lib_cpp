/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "aos/common/tools/uuid.hpp"

namespace aos {
namespace uuid {

TEST(UUIDTest, CreateUUID)
{
    static constexpr auto cTestUUIDsCount = 1000;

    std::vector<UUID> uuids;

    for (int i = 0; i < cTestUUIDsCount; i++) {
        auto tmp = CreateUUID();

        ASSERT_EQ(tmp.Size(), tmp.MaxSize());

        uuids.push_back(tmp);
    }

    UUID prevUUID;
    std::sort(uuids.begin(), uuids.end());

    for (const auto& cur : uuids) {
        ASSERT_NE(prevUUID, cur);
        prevUUID = cur;
    }
}

TEST(UUIDTest, UUIDToString)
{
    uint8_t uuidBlob[uuid::cUUIDSize]
        = {0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x9A, 0xAB, 0xBC, 0xCD, 0xDE, 0xEF, 0xFF};
    UUID source = Array<uint8_t>(uuidBlob, uuid::cUUIDSize);

    EXPECT_EQ(UUIDToString(source), "01122334-4556-6778-899a-abbccddeefff");
}

TEST(UUIDTest, StringToUUID)
{
    Error   err = ErrorEnum::eNone;
    UUID    destination;
    uint8_t expected[uuid::cUUIDSize]
        = {0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x9A, 0xAB, 0xBC, 0xCD, 0xDE, 0xEF, 0xFF};

    Tie(destination, err) = StringToUUID("01122334-4556-6778-899A-abbccddeefff");
    ASSERT_TRUE(err.IsNone());

    EXPECT_EQ(destination, Array<uint8_t>(expected, uuid::cUUIDSize));
}

TEST(UUIDTest, TreatEmptyUUIDValid)
{
    // empty UUID produces 0... string
    EXPECT_EQ(UUIDToString(UUID {}), "00000000-0000-0000-0000-000000000000");

    UUID  result;
    Error err = ErrorEnum::eNone;

    // empty string produces 0... UUID
    Tie(result, err) = StringToUUID("");
    ASSERT_TRUE(err.IsNone());

    uint8_t expected[uuid::cUUIDSize] = {};

    EXPECT_EQ(result, Array<uint8_t>(expected, uuid::cUUIDSize));
}

} // namespace uuid
} // namespace aos
