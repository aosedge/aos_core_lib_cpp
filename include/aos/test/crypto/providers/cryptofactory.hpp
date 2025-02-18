/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRYPTO_FACTORY_HPP_
#define CRYPTO_FACTORY_HPP_

#ifdef WITH_OPENSSL
#include "opensslfactory.hpp"

#define DEFAULT_CRYPTO_FACTORY OpenSSLCryptoFactory
#endif

#ifdef WITH_MBEDTLS
#include "mbedtlsfactory.hpp"

#undef DEFAULT_CRYPTO_FACTORY
#define DEFAULT_CRYPTO_FACTORY MBedTLSCryptoFactory
#endif

namespace aos::crypto {
using DefaultCryptoFactory = DEFAULT_CRYPTO_FACTORY;
}

#endif
