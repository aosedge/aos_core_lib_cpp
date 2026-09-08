/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_AOS_COMMON_CRYPTO_CRYPTOUTILS_HPP_
#define AOS_AOS_COMMON_CRYPTO_CRYPTOUTILS_HPP_

#include "itf/hash.hpp"

namespace aos::crypto {

/**
 * URN prefix used by AosCloud for System ID in online certificate SAN.
 */
constexpr auto cSystemIDURNPrefix = "urn:aos:unit:";

/**
 * Calculates file hash.
 *
 * @param path file path.
 * @param algorithm hash algorithm.
 * @param hashProvider hash provider.
 * @param hash output hash.
 * @return Error.
 */
Error CalculateFileHash(const String& path, const Hash& algorithm, HasherItf& hashProvider, Array<uint8_t>& hash);

/**
 * Extracts System ID from a SAN URI starting with urn:aos:unit:.
 *
 * @param uri SAN URI.
 * @param[out] systemID extracted system ID.
 * @return Error.
 */
Error GetSystemIDFromCert(const String& uri, String& systemID);

} // namespace aos::crypto

#endif
