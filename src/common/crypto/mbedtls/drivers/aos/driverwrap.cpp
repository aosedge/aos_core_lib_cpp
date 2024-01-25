/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mbedtls/asn1write.h>
#include <mbedtls/base64.h>
#include <mbedtls/pem.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>

#include "aos/common/crypto/mbedtls/drivers/aos/driverwrap.hpp"
#include "aos/common/tools/array.hpp"
#include "aos/common/tools/thread.hpp"

/***********************************************************************************************************************
 * Structs
 **********************************************************************************************************************/

/**
 * Key description.
 */
struct KeyDescription {
    /**
     * Key ID.
     */
    psa_key_id_t mKeyId;
    /**
     * Key lifetime.
     */
    psa_key_lifetime_t mLifetime;
    /**
     * Slot number.
     */
    psa_drv_slot_number_t mSlotNumber;
    /**
     * Private key.
     */
    const aos::crypto::PrivateKeyItf* mPrivKey;
};

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static aos::StaticArray<KeyDescription, MBEDTLS_PSA_KEY_SLOT_COUNT> sBuiltinKeys;
aos::Mutex                                                          sMutex;

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::RetWithError<psa_key_id_t> AosPsaAddKey(const aos::crypto::PrivateKeyItf& mPrivKey)
{
    aos::LockGuard lock(sMutex);

    for (psa_key_id_t keyId = MBEDTLS_PSA_KEY_ID_BUILTIN_MIN; keyId <= MBEDTLS_PSA_KEY_ID_BUILTIN_MAX; ++keyId) {
        bool found {};

        for (auto& key : sBuiltinKeys) {
            if (key.mKeyId == keyId) {
                found = true;

                break;
            }
        }

        if (!found) {
            KeyDescription key;

            key.mKeyId    = keyId;
            key.mLifetime = PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
                PSA_KEY_PERSISTENCE_DEFAULT, PSA_CRYPTO_AOS_DRIVER_LOCATION);
            key.mSlotNumber = sBuiltinKeys.Size();
            key.mPrivKey    = &mPrivKey;

            return aos::RetWithError<psa_key_id_t>(keyId, sBuiltinKeys.PushBack(key));
        }
    }

    return aos::RetWithError<psa_key_id_t>(MBEDTLS_PSA_KEY_ID_BUILTIN_MAX + 1, aos::ErrorEnum::eOutOfRange);
}

void AosPsaRemoveKey(psa_key_id_t keyId)
{
    aos::LockGuard lock(sMutex);

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyId == keyId) {
            sBuiltinKeys.Remove(&key);

            psa_destroy_key(MBEDTLS_SVC_KEY_ID_GET_KEY_ID(keyId));

            break;
        }
    }
}

/***********************************************************************************************************************
 * PSA driver wrappers
 **********************************************************************************************************************/

extern "C" {
psa_status_t mbedtls_psa_platform_get_builtin_key(
    mbedtls_svc_key_id_t keyId, psa_key_lifetime_t* lifetime, psa_drv_slot_number_t* slotNumber)
{
    psa_key_id_t appKeyId = MBEDTLS_SVC_KEY_ID_GET_KEY_ID(keyId);

    aos::LockGuard lock(sMutex);

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyId == appKeyId) {
            *lifetime   = key.mLifetime;
            *slotNumber = key.mSlotNumber;

            return PSA_SUCCESS;
        }
    }

    return PSA_ERROR_DOES_NOT_EXIST;
}
}

psa_status_t aos_get_builtin_key(psa_drv_slot_number_t slotNumber, psa_key_attributes_t* attributes, uint8_t* keyBuffer,
    size_t keyBufferSize, size_t* keyBufferLength)
{
    (void)keyBuffer;
    (void)keyBufferLength;

    aos::LockGuard lock(sMutex);

    for (auto& key : sBuiltinKeys) {
        if (key.mSlotNumber == slotNumber) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA:
                return PSA_ERROR_NOT_SUPPORTED;

            case aos::crypto::KeyTypeEnum::eECDSA:
                return PSA_ERROR_NOT_SUPPORTED;

            default:
                return PSA_ERROR_NOT_SUPPORTED;
            }

            return keyBufferSize != 0 ? PSA_SUCCESS : PSA_ERROR_BUFFER_TOO_SMALL;
        }
    }

    return PSA_ERROR_DOES_NOT_EXIST;
}

psa_status_t aos_signature_sign_hash(const psa_key_attributes_t* attributes, const uint8_t* key_buffer,
    size_t key_buffer_size, psa_algorithm_t alg, const uint8_t* hash, size_t hash_length, uint8_t* signature,
    size_t signature_size, size_t* signature_length)
{
    (void)key_buffer;
    (void)key_buffer_size;
    (void)alg;

    aos::LockGuard lock(sMutex);

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyId == psa_get_key_id(attributes)) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA: {
                return PSA_ERROR_NOT_SUPPORTED;
            }

            case aos::crypto::KeyTypeEnum::eECDSA:
                return PSA_ERROR_NOT_SUPPORTED;

            default:
                return PSA_ERROR_NOT_SUPPORTED;
            }
        }
    }

    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t aos_export_public_key(const psa_key_attributes_t* attributes, const uint8_t* key_buffer,
    size_t key_buffer_size, uint8_t* data, size_t data_size, size_t* data_length)
{
    (void)key_buffer;
    (void)key_buffer_size;

    aos::LockGuard lock(sMutex);

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyId == psa_get_key_id(attributes)) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA: {
                return PSA_ERROR_NOT_SUPPORTED;
            }

            case aos::crypto::KeyTypeEnum::eECDSA:
                return PSA_ERROR_NOT_SUPPORTED;

            default:
                return PSA_ERROR_NOT_SUPPORTED;
            }
        }
    }

    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t aos_get_key_buffer_size(const psa_key_attributes_t* attributes, size_t* key_buffer_size)
{
    (void)attributes;

    *key_buffer_size = 1;

    return PSA_SUCCESS;
}
