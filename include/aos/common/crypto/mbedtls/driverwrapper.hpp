/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DRIVERWRAPPER_HPP_
#define DRIVERWRAPPER_HPP_

#include <psa/crypto.h>

#include "aos/common/crypto.hpp"

/**
 * @brief Add key to the list of builtin keys.
 *
 * @param key Key description.
 *
 * @return Error.
 */
aos::RetWithError<psa_key_id_t> AosPsaAddKey(const aos::crypto::PrivateKeyItf& privKey);

/**
 * @brief Remove key from the list of builtin keys.
 *
 * @param keyId Key ID.
 */
void AosPsaRemoveKey(psa_key_id_t keyID);

#endif
