/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_PSA_DRIVER_WRAP_HPP_
#define AOS_PSA_DRIVER_WRAP_HPP_

#include <aos/common/crypto.hpp>

#include "driver.h"

/**
 * @brief Add key to the list of builtin keys
 *
 * @param key Key description
 *
 * @return Error code
 */
aos::RetWithError<psa_key_id_t> AosPsaAddKey(const aos::crypto::PrivateKeyItf& mPrivKey);

/**
 * @brief Remove key from the list of builtin keys
 *
 * @param keyId Key ID
 */
void AosPsaRemoveKey(psa_key_id_t keyId);

#endif
