/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OPENSSL_PROVIDER_HPP_
#define AOS_OPENSSL_PROVIDER_HPP_

#include <aos/common/config.hpp>
#include <aos/common/crypto/crypto.hpp>

/**
 * Returns OpenSSL error.
 */
#define OPENSSL_ERROR()                                                                                                \
    []() {                                                                                                             \
        unsigned long errCode = ERR_get_error();                                                                       \
        ERR_clear_error();                                                                                             \
        if (errCode != 0) {                                                                                            \
            return AOS_ERROR_WRAP(Error(errCode, ERR_error_string(errCode, nullptr)));                                 \
        } else {                                                                                                       \
            return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed));                                                          \
        }                                                                                                              \
    }()

namespace aos::crypto::openssl {

/**
 * AOS provider name.
 */
static constexpr auto cAosSigner = "AosSigner";

/**
 * AOS provider filter.
 */
static constexpr auto cAosSignerProvider = "provider=AosSigner";

/**
 * AOS provider algorithm.
 */
static constexpr auto cAosAlgorithm = "Aos";

/**
 * AOS encryption.
 */
static constexpr auto cAosEncryption = "Aos:AosEncryption";

/**
 * AOS private key params.
 */
static constexpr auto cPKeyParamAosKeyPair = "AosPrivateKey";

/**
 * AOS OpenSSL provider.
 */
class OpenSSLProvider {
public:
    /**
     * Loads AOS provider.
     *
     * @param libctx OpenSSL library context.
     * @result Error.
     */
    Error Load(struct ossl_lib_ctx_st* libctx);

    /**
     * Unloads AOS provider.
     *
     * @result Error.
     */
    Error Unload();

private:
    struct ossl_provider_st* mAOSProvider     = nullptr;
    struct ossl_provider_st* mDefaultProvider = nullptr;
};

/**
 * Takes OID with stripped tag & length and returns complete ASN1 OID object.
 *
 * @param rawOID raw ASN1 OID without tag & value.
 * @result RetWithError<StaticArray<uint8_t, cECDSAParamsOIDSize>>.
 */
RetWithError<StaticArray<uint8_t, cECDSAParamsOIDSize>> GetFullOID(const Array<uint8_t>& rawOID);

/**
 * Releases OpenSSL object.
 *
 * @param ptr pointer to the object to be freed.
 */
void AOS_OPENSSL_free(void* ptr);

} // namespace aos::crypto::openssl

#endif
