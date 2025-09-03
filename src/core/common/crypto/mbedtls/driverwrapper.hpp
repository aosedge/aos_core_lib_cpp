/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_MBEDTLS_DRIVERWRAPPER_HPP_
#define AOS_CORE_COMMON_CRYPTO_MBEDTLS_DRIVERWRAPPER_HPP_

#include <psa/crypto.h>

#include "../crypto.hpp"

/**
 * Key description.
 */
struct KeyInfo {
    /**
     * Key ID.
     */
    psa_key_id_t mKeyID;
    /**
     * Message digest type.
     */
    mbedtls_md_type_t mMDType;
};

/**
 * Adds key to the list of builtin keys.
 *
 * @param key Key description.
 * @return Key ID and error code.
 */
aos::RetWithError<KeyInfo> AosPsaAddKey(const aos::crypto::PrivateKeyItf& privKey);

/**
 * Removes key from the list of builtin keys.
 *
 * @param keyId Key ID.
 */
void AosPsaRemoveKey(psa_key_id_t keyID);

#endif
