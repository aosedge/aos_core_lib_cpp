/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OPENSSL_PROVIDER_HPP_
#define AOS_OPENSSL_PROVIDER_HPP_

#include <aos/common/config.hpp>
#include <aos/common/crypto/crypto.hpp>

#define AOS_SIGNER             "aossigner"
#define AOS_ALGORITHM          "AOS"
#define PKEY_PARAM_AOS_KEYPAIR AOS_ALGORITHM "PrivateKey"

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

#define AOS_OPENSSL_free [](void* ptr) { OPENSSL_free(ptr); }

struct ossl_provider_st;

namespace aos::crypto::openssl {

class OpenSSLProvider {
public:
    Error Load();
    Error Unload();

private:
    ossl_provider_st* mProvider        = nullptr;
    ossl_provider_st* mDefaultProvider = nullptr;
};

// Takes OID with stripped tag & length, complements it to get a valid ASN1 OID object.
RetWithError<StaticArray<uint8_t, cECDSAParamsOIDSize>> GetFullOID(const Array<uint8_t>& rawOID);

} // namespace aos::crypto::openssl

#endif
