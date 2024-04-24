/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_UUID_HPP_
#define AOS_UUID_HPP_

#include "aos/common/config.hpp"
#include "aos/common/tools/string.hpp"

namespace aos {
namespace uuid {

/**
 *  UUID length
 */
constexpr auto cUUIDSize = AOS_CONFIG_UUID_SIZE;

/**
 *  Length of UUID string representation
 */
constexpr auto cUUIDLen = AOS_CONFIG_UUID_LEN;

/**
 * A UUID is a 128 bit (16 byte) Universal Unique IDentifier as defined in RFC 4122.
 */
using UUID = StaticArray<uint8_t, cUUIDSize>;

/**
 * Creates unique UUID.
 *
 * @return UUID.
 */
UUID CreateUUID();

/**
 * Converts UUID to string.
 *
 * @param uuid uuid.
 * @return StaticString<cUUIDLen>.
 */
StaticString<cUUIDLen> UUIDToString(const UUID& uuid);

/**
 * Converts string to UUID.
 *
 * @param src input string.
 * @return RetWithError<UUID>.
 */
RetWithError<UUID> StringToUUID(const StaticString<uuid::cUUIDLen>& src);

} // namespace uuid
} // namespace aos

#endif
