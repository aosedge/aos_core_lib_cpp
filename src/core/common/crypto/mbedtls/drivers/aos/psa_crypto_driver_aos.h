#ifndef PSA_CRYPTO_DRIVER_AOS_H_
#define PSA_CRYPTO_DRIVER_AOS_H_

#if defined(PSA_CRYPTO_DRIVER_AOS)

#ifndef PSA_CRYPTO_ACCELERATOR_DRIVER_PRESENT
#define PSA_CRYPTO_ACCELERATOR_DRIVER_PRESENT
#endif

#define PSA_CRYPTO_AOS_DRIVER_LOCATION 0x800000

#include <psa/crypto.h>

#ifdef __cplusplus
extern "C" {
#endif

psa_status_t aos_signature_sign_hash(const psa_key_attributes_t* attributes, const uint8_t* key_buffer,
    size_t key_buffer_size, psa_algorithm_t alg, const uint8_t* hash, size_t hash_length, uint8_t* signature,
    size_t signature_size, size_t* signature_length);

psa_status_t aos_export_public_key(const psa_key_attributes_t* attributes, const uint8_t* key_buffer,
    size_t key_buffer_size, uint8_t* data, size_t data_size, size_t* data_length);

psa_status_t aos_get_key_buffer_size(const psa_key_attributes_t* attributes, size_t* key_buffer_size);

psa_status_t aos_get_builtin_key(psa_drv_slot_number_t slotNumber, psa_key_attributes_t* attributes, uint8_t* keyBuffer,
    size_t keyBufferSize, size_t* keyBufferLength);

#ifdef __cplusplus
}
#endif

#endif

#endif
