/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mbedtls/asn1write.h>
#include <mbedtls/base64.h>
#include <mbedtls/oid.h>
#include <mbedtls/pem.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>

#include "aos/common/crypto/mbedtls/driverwrapper.hpp"
#include "aos/common/tools/array.hpp"
#include "aos/common/tools/thread.hpp"

#include "../log.hpp"
#include "drivers/aos/psa_crypto_driver_aos.h"

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
    psa_key_id_t mKeyID;
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

static int ExportRSAPublicKeyToDER(
    const aos::crypto::RSAPublicKey& rsaKey, uint8_t* data, size_t dataSize, size_t* dataLength)
{
    mbedtls_mpi n, e;

    mbedtls_mpi_init(&n);
    mbedtls_mpi_init(&e);

    mbedtls_mpi_read_binary(&n, rsaKey.GetN().Get(), rsaKey.GetN().Size());
    mbedtls_mpi_read_binary(&e, rsaKey.GetE().Get(), rsaKey.GetE().Size());

    auto cleanup = [&]() {
        mbedtls_mpi_free(&n);
        mbedtls_mpi_free(&e);
    };

    // Write from the end of the buffer
    unsigned char* c = data + dataSize;

    auto ret = mbedtls_asn1_write_mpi(&c, data, &e);
    if (ret < 0) {
        cleanup();

        return ret;
    }

    size_t len = ret;

    if ((ret = mbedtls_asn1_write_mpi(&c, data, &n)) < 0) {
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

static aos::Pair<psa_ecc_family_t, size_t> FindPsaECGroupByOID(const aos::Array<uint8_t>& oid)
{
    for (int i = MBEDTLS_ECP_DP_NONE; i < MBEDTLS_ECP_DP_MAX; ++i) {
        const char* groupOID;
        size_t      groupOIDSize;

        if (mbedtls_oid_get_oid_by_ec_grp(static_cast<mbedtls_ecp_group_id>(i), &groupOID, &groupOIDSize) == 0) {
            aos::Array<uint8_t> knownOIDBuf(reinterpret_cast<const uint8_t*>(groupOID), groupOIDSize);

            if (oid == knownOIDBuf) {
                switch (i) {
                case MBEDTLS_ECP_DP_SECP192R1:
                    return {PSA_ECC_FAMILY_SECP_R1, 192};
                case MBEDTLS_ECP_DP_SECP224R1:
                    return {PSA_ECC_FAMILY_SECP_R1, 224};
                case MBEDTLS_ECP_DP_SECP256R1:
                    return {PSA_ECC_FAMILY_SECP_R1, 256};
                case MBEDTLS_ECP_DP_SECP384R1:
                    return {PSA_ECC_FAMILY_SECP_R1, 384};
                case MBEDTLS_ECP_DP_SECP521R1:
                    return {PSA_ECC_FAMILY_SECP_R1, 521};
                case MBEDTLS_ECP_DP_SECP192K1:
                    return {PSA_ECC_FAMILY_SECP_K1, 192};
                case MBEDTLS_ECP_DP_SECP224K1:
                    return {PSA_ECC_FAMILY_SECP_K1, 224};
                case MBEDTLS_ECP_DP_SECP256K1:
                    return {PSA_ECC_FAMILY_SECP_K1, 256};
                case MBEDTLS_ECP_DP_CURVE25519:
                    return {PSA_ECC_FAMILY_MONTGOMERY, 255};
                case MBEDTLS_ECP_DP_CURVE448:
                    return {PSA_ECC_FAMILY_MONTGOMERY, 448};
                case MBEDTLS_ECP_DP_BP256R1:
                    return {PSA_ECC_FAMILY_BRAINPOOL_P_R1, 256};
                case MBEDTLS_ECP_DP_BP384R1:
                    return {PSA_ECC_FAMILY_BRAINPOOL_P_R1, 384};
                case MBEDTLS_ECP_DP_BP512R1:
                    return {PSA_ECC_FAMILY_BRAINPOOL_P_R1, 512};

                default:
                    return {PSA_ECC_FAMILY_SECP_R1, 0};
                }
            }
        }
    }

    return {PSA_ECC_FAMILY_SECP_R1, 0};
}

static aos::Pair<aos::Error, mbedtls_ecp_group_id> FindECPGroupByOID(const aos::Array<uint8_t>& oid)
{
    for (int i = MBEDTLS_ECP_DP_NONE; i < MBEDTLS_ECP_DP_MAX; ++i) {
        const char* groupOID;
        size_t      groupOIDSize;

        if (mbedtls_oid_get_oid_by_ec_grp(static_cast<mbedtls_ecp_group_id>(i), &groupOID, &groupOIDSize) == 0) {
            aos::Array<uint8_t> knownOIDBuf(reinterpret_cast<const uint8_t*>(groupOID), groupOIDSize);

            if (oid == knownOIDBuf) {
                return {aos::ErrorEnum::eNone, static_cast<mbedtls_ecp_group_id>(i)};
            }
        }
    }

    return {aos::ErrorEnum::eNotFound, MBEDTLS_ECP_DP_NONE};
}

static int ExportECPublicKeyToDER(
    const aos::crypto::ECDSAPublicKey& ecKey, uint8_t* data, size_t dataSize, size_t* dataLength)
{
    auto curveParameters = FindECPGroupByOID(ecKey.GetECParamsOID());
    if (!curveParameters.mFirst.IsNone()) {
        return -1;
    }

    mbedtls_ecp_group grp;
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point q;
    mbedtls_ecp_point_init(&q);

    auto cleanup = [&]() {
        mbedtls_ecp_group_free(&grp);
        mbedtls_ecp_point_free(&q);
    };

    auto ret = mbedtls_ecp_group_load(&grp, curveParameters.mSecond);
    if (ret != 0) {
        cleanup();

        return ret;
    }

    const aos::Array<uint8_t>& point = ecKey.GetECPoint();

    if ((ret = mbedtls_ecp_point_read_binary(&grp, &q, point.Get(), point.Size())) != 0) {
        cleanup();

        return ret;
    }

    size_t len = 0;

    if ((ret = mbedtls_ecp_point_write_binary(&grp, &q, MBEDTLS_ECP_PF_UNCOMPRESSED, &len, data, dataSize)) != 0) {
        cleanup();

        return ret;
    }

    *dataLength = len;

    cleanup();

    return 0;
}

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::RetWithError<psa_key_id_t> AosPsaAddKey(const aos::crypto::PrivateKeyItf& privKey)
{
    aos::LockGuard lock(sMutex);

    for (psa_key_id_t keyID = MBEDTLS_PSA_KEY_ID_BUILTIN_MIN; keyID <= MBEDTLS_PSA_KEY_ID_BUILTIN_MAX; ++keyID) {
        bool found {};

        for (auto& key : sBuiltinKeys) {
            if (key.mKeyID == keyID) {
                found = true;

                break;
            }
        }

        if (!found) {
            KeyDescription key;

            key.mKeyID    = keyID;
            key.mLifetime = PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
                PSA_KEY_PERSISTENCE_DEFAULT, PSA_CRYPTO_AOS_DRIVER_LOCATION);
            key.mSlotNumber = sBuiltinKeys.Size();
            key.mPrivKey    = &privKey;

            LOG_DBG() << "Add Aos PSA key: keyID = " << key.mKeyID << ", slotNumber = " << key.mSlotNumber;

            return aos::RetWithError<psa_key_id_t>(keyID, sBuiltinKeys.PushBack(key));
        }
    }

    return aos::RetWithError<psa_key_id_t>(MBEDTLS_PSA_KEY_ID_BUILTIN_MAX + 1, aos::ErrorEnum::eOutOfRange);
}

void AosPsaRemoveKey(psa_key_id_t keyID)
{
    aos::UniqueLock lock(sMutex);

    LOG_DBG() << "Remove Aos PSA key: keyID = " << keyID;

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyID == keyID) {
            sBuiltinKeys.Remove(&key);

            lock.Unlock();

            psa_destroy_key(MBEDTLS_SVC_KEY_ID_GET_KEY_ID(keyID));

            return;
        }
    }
}

/***********************************************************************************************************************
 * PSA driver wrappers
 **********************************************************************************************************************/

extern "C" {

psa_status_t mbedtls_psa_platform_get_builtin_key(
    mbedtls_svc_key_id_t keyID, psa_key_lifetime_t* lifetime, psa_drv_slot_number_t* slotNumber)
{
    psa_key_id_t appKeyID = MBEDTLS_SVC_KEY_ID_GET_KEY_ID(keyID);

    LOG_DBG() << "Get platform built-in key: keyID = " << keyID;

    aos::LockGuard lock(sMutex);

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyID == appKeyID) {
            *lifetime   = key.mLifetime;
            *slotNumber = key.mSlotNumber;

            return PSA_SUCCESS;
        }
    }

    LOG_ERR() << "Built-in key not found: keyID = " << keyID;

    return PSA_ERROR_DOES_NOT_EXIST;
}

psa_status_t aos_get_builtin_key(psa_drv_slot_number_t slotNumber, psa_key_attributes_t* attributes, uint8_t* keyBuffer,
    size_t keyBufferSize, size_t* keyBufferLength)
{
    (void)keyBuffer;
    (void)keyBufferLength;

    LOG_DBG() << "Get Aos built-in key: slotNumber = " << slotNumber;

    for (auto& key : sBuiltinKeys) {
        if (key.mSlotNumber == slotNumber) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA:
                psa_set_key_type(attributes, PSA_KEY_TYPE_RSA_KEY_PAIR);
                psa_set_key_algorithm(attributes, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));

                break;

            case aos::crypto::KeyTypeEnum::eECDSA: {
                psa_set_key_algorithm(attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
                auto oid = static_cast<const aos::crypto::ECDSAPublicKey&>(key.mPrivKey->GetPublic()).GetECParamsOID();
                auto curveParameters = FindPsaECGroupByOID(oid);
                if (curveParameters.mSecond == 0) {
                    LOG_ERR() << "EC group not found: slotNumber = " << slotNumber;

                    return PSA_ERROR_NOT_SUPPORTED;
                }

                psa_set_key_type(attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(curveParameters.mFirst));
                psa_set_key_bits(attributes, curveParameters.mSecond);

                break;
            }

            default:
                LOG_ERR() << "Not supported key type: keyType = " << key.mPrivKey->GetPublic().GetKeyType();

                return PSA_ERROR_NOT_SUPPORTED;
            }

            psa_set_key_id(attributes, key.mKeyID);
            psa_set_key_lifetime(attributes, key.mLifetime);
            psa_set_key_usage_flags(attributes, PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_VERIFY_HASH);

            return keyBufferSize != 0 ? PSA_SUCCESS : PSA_ERROR_BUFFER_TOO_SMALL;
        }
    }

    LOG_ERR() << "Built-in key not found: slotNumber = " << slotNumber;

    return PSA_ERROR_DOES_NOT_EXIST;
}

psa_status_t aos_signature_sign_hash(const psa_key_attributes_t* attributes, const uint8_t* key_buffer,
    size_t key_buffer_size, psa_algorithm_t alg, const uint8_t* hash, size_t hash_length, uint8_t* signature,
    size_t signature_size, size_t* signature_length)
{
    (void)key_buffer;
    (void)key_buffer_size;
    (void)alg;

    LOG_DBG() << "Sign hash";

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyID == psa_get_key_id(attributes)) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA:
            case aos::crypto::KeyTypeEnum::eECDSA: {
                aos::crypto::SignOptions options;
                options.mHash = aos::crypto::HashEnum::eSHA256;

                aos::Array<uint8_t> digest(hash, hash_length);
                aos::Array<uint8_t> signatureArray(signature, signature_size);

                auto err = key.mPrivKey->Sign(digest, options, signatureArray);
                if (!err.IsNone()) {
                    LOG_ERR() << "Sign failed: " << err;

                    return PSA_ERROR_GENERIC_ERROR;
                }

                *signature_length = signatureArray.Size();

                return PSA_SUCCESS;
            }

            default:
                LOG_ERR() << "Not supported key type: keyType = " << key.mPrivKey->GetPublic().GetKeyType();

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

    LOG_DBG() << "Export public key";

    for (auto& key : sBuiltinKeys) {
        if (key.mKeyID == psa_get_key_id(attributes)) {
            switch (key.mPrivKey->GetPublic().GetKeyType().GetValue()) {
            case aos::crypto::KeyTypeEnum::eRSA: {
                auto ret
                    = ExportRSAPublicKeyToDER(static_cast<const aos::crypto::RSAPublicKey&>(key.mPrivKey->GetPublic()),
                        data, data_size, data_length);
                if (ret != 0) {
                    LOG_ERR() << "Error exporting RSA public key: " << ret;

                    return PSA_ERROR_GENERIC_ERROR;
                }

                return PSA_SUCCESS;
            }

            case aos::crypto::KeyTypeEnum::eECDSA: {
                auto ret
                    = ExportECPublicKeyToDER(static_cast<const aos::crypto::ECDSAPublicKey&>(key.mPrivKey->GetPublic()),
                        data, data_size, data_length);
                if (ret != 0) {
                    LOG_ERR() << "Error exporting EC public key: " << ret;

                    return PSA_ERROR_GENERIC_ERROR;
                }

                return PSA_SUCCESS;
            }

            default:
                LOG_ERR() << "Not supported key type: keyType = " << key.mPrivKey->GetPublic().GetKeyType();

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

} // extern "C"
