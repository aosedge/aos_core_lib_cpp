/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CRYPTO_PROVIDER_HPP_
#define AOS_CRYPTO_PROVIDER_HPP_

#ifdef WITH_OPENSSL
#include "openssl/cryptoprovider.hpp"

#define DEFAULT_CRYPTO_PROVIDER OpenSSLCryptoProvider
#endif

#ifdef WITH_MBEDTLS
#include "mbedtls/cryptoprovider.hpp"

#undef DEFAULT_CRYPTO_PROVIDER
#define DEFAULT_CRYPTO_PROVIDER MbedTLSCryptoProvider
#endif

namespace aos::crypto {
using DefaultCryptoProvider = DEFAULT_CRYPTO_PROVIDER;
}

#endif
