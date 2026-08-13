/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_AOS_COMMON_CRYPTO_CRYPTOUTILS_HPP_
#define AOS_AOS_COMMON_CRYPTO_CRYPTOUTILS_HPP_

#include "itf/hash.hpp"
#include "itf/x509.hpp"

namespace aos::crypto {

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
 * Validates that a certificate can be used as a CA (root or intermediate issuer).
 *
 * @param cert certificate to validate.
 * @return Error.
 */
Error ValidateCACert(const x509::Certificate& cert);

} // namespace aos::crypto

#endif
