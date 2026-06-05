/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CERTHANDLER_ITF_HSM_HPP_
#define AOS_CORE_IAM_CERTHANDLER_ITF_HSM_HPP_

#include <core/common/crypto/itf/crypto.hpp>
#include <core/common/iamclient/itf/certprovider.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/memory.hpp>
#include <core/iam/config.hpp>

namespace aos::iam::certhandler {

/**
 * Max number of IAM certificates per module.
 */
constexpr auto cCertsPerModule = AOS_CONFIG_CERTHANDLER_CERTS_PER_MODULE;

/**
 * Platform dependent secure certificate storage.
 */
class HSMItf {
public:
    /**
     * Owns the module.
     *
     * @param password certificate password.
     * @return Error.
     */
    virtual Error SetOwner(const String& password) = 0;

    /**
     * Removes all module certificates.
     *
     * @return Error.
     */
    virtual Error Clear() = 0;

    /**
     * Generates private key.
     *
     * @param password owner password.
     * @param keyType key type.
     * @return RetWithError<SharedPtr<crypto::PrivateKeyItf>>.
     */
    virtual RetWithError<SharedPtr<crypto::PrivateKeyItf>> CreateKey(const String& password, crypto::KeyType keyType)
        = 0;

    /**
     * Applies certificate chain to a module.
     *
     * @param certChain certificate chain.
     * @param[out] certInfo info about a top certificate in a chain.
     * @param[out] password owner password.
     * @return Error.
     */
    virtual Error ApplyCert(const Array<crypto::x509::Certificate>& certChain, CertInfo& certInfo, String& password)
        = 0;

    /**
     * Removes certificate chain using top level certificate URL and password.
     *
     * @param certURL top level certificate URL.
     * @param password owner password.
     * @return Error.
     */
    virtual Error RemoveCert(const String& certURL, const String& password) = 0;

    /**
     * Removes private key from a module.
     *
     * @param keyURL private key URL.
     * @param password owner password.
     * @return Error.
     */
    virtual Error RemoveKey(const String& keyURL, const String& password) = 0;

    /**
     * Returns valid/invalid certificates.
     *
     * @param[out] invalidCerts invalid certificate URLs.
     * @param[out] invalidKeys invalid key URLs.
     * @param[out] validCerts information about valid certificates.
     * @return Error.
     **/
    virtual Error ValidateCertificates(Array<StaticString<cURLLen>>& invalidCerts,
        Array<StaticString<cURLLen>>& invalidKeys, Array<CertInfo>& validCerts)
        = 0;

    /**
     * Destroys object instance.
     */
    virtual ~HSMItf() = default;
};

} // namespace aos::iam::certhandler

#endif
