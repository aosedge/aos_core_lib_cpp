/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_UUID_HPP_
#define AOS_UUID_HPP_

#include <core/common/config.hpp>

#include "string.hpp"

namespace aos::uuid {

/**
 *  UUID length
 */
constexpr auto cUUIDSize = AOS_CONFIG_TOOLS_UUID_SIZE;

/**
 *  Length of UUID string representation
 */
constexpr auto cUUIDLen = AOS_CONFIG_TOOLS_UUID_LEN;

/**
 * A UUID is a 128 bit (16 byte) Universal Unique IDentifier as defined in RFC 4122.
 */
using UUID = StaticArray<uint8_t, cUUIDSize>;

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

} // namespace aos::uuid

#endif
