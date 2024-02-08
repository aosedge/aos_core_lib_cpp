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
constexpr auto cUUIDLen = AOS_CONFIG_UUID_LEN;

/**
 *  Length of UUID string representation
 */
constexpr auto cUUIDStrLen = AOS_CONFIG_UUID_STR_LEN;

/**
 * A UUID is a 128 bit (16 byte) Universal Unique IDentifier as defined in RFC 4122.
 */
using UUID = StaticArray<uint8_t, cUUIDLen>;

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
 * @return StaticString<cUUIDStrLen>.
 */
StaticString<cUUIDStrLen> UUIDToString(const UUID& uuid);

/**
 * Converts string to UUID.
 *
 * @param src input string.
 * @return RetWithError<UUID>.
 */
RetWithError<UUID> StringToUUID(const StaticString<uuid::cUUIDStrLen>& src);

} // namespace uuid
} // namespace aos

#endif
