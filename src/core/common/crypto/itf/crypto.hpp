/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_CRYPTO_HPP_
#define AOS_CORE_COMMON_CRYPTO_CRYPTO_HPP_

#include "aes.hpp"
#include "asn1.hpp"
#include "hash.hpp"
#include "rand.hpp"
#include "uuid.hpp"
#include "x509.hpp"

namespace aos::crypto {

/**
 * Crypto provider interface.
 */
class CryptoProviderItf : public x509::ProviderItf,
                          public HasherItf,
                          public RandomItf,
                          public UUIDItf,
                          public AESEncoderDecoderItf,
                          public asn1::ASN1DecoderItf {
public:
    /**
     * Destructor.
     */
    virtual ~CryptoProviderItf() = default;
};

} // namespace aos::crypto

#endif // AOS_CORE_COMMON_CRYPTO_CRYPTO_HPP_
