/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRYPTOFACTORY_HPP_
#define CRYPTOFACTORY_HPP_

#if defined(WITH_MBEDTLS)
#include "mbedtlsfactory.hpp"

namespace aos::crypto {
using DefaultCryptoFactory = MBedTLSCryptoFactory;
}
#elif defined(WITH_OPENSSL)
#include "opensslfactory.hpp"

namespace aos::crypto {
using DefaultCryptoFactory = OpenSSLCryptoFactory;
}
#else
#error "No crypto provider defined. Define WITH_OPENSSL or WITH_MBEDTLS."
#endif

#endif // CRYPTOFACTORY_HPP_
