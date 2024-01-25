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

static int ExportRsaPublicKeyToDer(
    const aos::crypto::RSAPublicKey& rsaKey, uint8_t* data, size_t dataSize, size_t* dataLength)
{
    mbedtls_mpi N, E;

    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&E);

    mbedtls_mpi_read_binary(&N, rsaKey.GetN().Get(), rsaKey.GetN().Size());
    mbedtls_mpi_read_binary(&E, rsaKey.GetE().Get(), rsaKey.GetE().Size());

    auto cleanup = [&]() {
        mbedtls_mpi_free(&N);
        mbedtls_mpi_free(&E);
    };

    // Write from the end of the buffer
    unsigned char* c = data + dataSize;

    auto ret = mbedtls_asn1_write_mpi(&c, data, &E);
    if (ret < 0) {
        cleanup();

        return ret;
    }

    size_t len = ret;

    if ((ret = mbedtls_asn1_write_mpi(&c, data, &N)) < 0) {
        cleanup();

        return ret;
    }

    len += ret;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&c, data, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&c, data, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    memmove(data, c, len);
    *dataLength = len;

    cleanup();

    return 0;
}

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

    for (auto& key : sBuiltinKeys) {
        if (key.mSlotNumber == slotNumber) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA:
                psa_set_key_type(attributes, PSA_KEY_TYPE_RSA_KEY_PAIR);
                psa_set_key_algorithm(attributes, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));

                break;

            case aos::crypto::KeyTypeEnum::eECDSA:
                return PSA_ERROR_NOT_SUPPORTED;

            default:
                return PSA_ERROR_NOT_SUPPORTED;
            }

            psa_set_key_id(attributes, key.mKeyId);
            psa_set_key_lifetime(attributes, key.mLifetime);
            psa_set_key_usage_flags(attributes, PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_VERIFY_HASH);

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

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyId == psa_get_key_id(attributes)) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA: {
                aos::crypto::SignOptions options;
                options.mHash = aos::crypto::HashEnum::eSHA256;

                aos::Array<uint8_t> digest(hash, hash_length);
                aos::Array<uint8_t> signatureArray(signature, signature_size);

                auto err = key.mPrivKey->Sign(digest, options, signatureArray);
                if (!err.IsNone()) {
                    return PSA_ERROR_NOT_SUPPORTED;
                }

                *signature_length = signatureArray.Size();

                return PSA_SUCCESS;
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

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyId == psa_get_key_id(attributes)) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA: {
                auto ret
                    = ExportRsaPublicKeyToDer(static_cast<const aos::crypto::RSAPublicKey&>(key.mPrivKey->GetPublic()),
                        data, data_size, data_length);
                if (ret != 0) {
                    return PSA_ERROR_NOT_SUPPORTED;
                }

                return PSA_SUCCESS;
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
