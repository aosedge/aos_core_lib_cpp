/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRYPTOPROVIDER_HPP_
#define CRYPTOPROVIDER_HPP_

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>
#include <psa/crypto_types.h>

#include "aos/common/crypto.hpp"

namespace aos {
namespace crypto {

/**
 * MbedTLSCryptoProvider provider.
 */
class MbedTLSCryptoProvider : public aos::crypto::x509::ProviderItf {
public:
    /**
     * Creates a new certificate based on a template.
     *
     * @param templ a pattern for a new certificate.
     * @param parent a parent certificate in a certificate chain.
     * @param pubKey public key.
     * @param privKey private key.
     * @param[out] resultCert result certificate in PEM format.
     * @result Error.
     */
    aos::Error CreateCertificate(const aos::crypto::x509::Certificate& templ,
        const aos::crypto::x509::Certificate& parent, const aos::crypto::PrivateKeyItf& privKey,
        aos::Array<uint8_t>& pemCert) override;

    /**
     * Reads certificates from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @param[out] resultCerts result certificate chain.
     * @result Error.
     */
    aos::Error PEMToX509Certs(
        const aos::Array<uint8_t>& pemBlob, aos::Array<aos::crypto::x509::Certificate>& resultCerts) override;

    /**
     * Reads certificate from a DER blob.
     *
     * @param derBlob raw certificate in a DER format.
     * @param[out] resultCert result certificate.
     * @result Error.
     */
    aos::Error DERToX509Cert(const aos::Array<uint8_t>& derBlob, aos::crypto::x509::Certificate& resultCert) override;

    /**
     * Creates a new certificate request, based on a template.
     *
     * @param templ template for a new certificate request.
     * @param privKey private key.
     * @param[out] pemCSR result CSR in PEM format.
     * @result Error.
     */
    aos::Error CreateCSR(const aos::crypto::x509::CSR& templ, const aos::crypto::PrivateKeyItf& privKey,
        aos::Array<uint8_t>& pemCSR) override;

    /**
     * Reads private key from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @result RetWithError<SharedPtr<PrivateKeyItf>>.
     */
    aos::RetWithError<aos::SharedPtr<aos::crypto::PrivateKeyItf>> PEMToX509PrivKey(
        const aos::Array<uint8_t>& pemBlob) override;

    /**
     * Constructs x509 distinguished name(DN) from the argument list.
     *
     * @param comName common name.
     * @param[out] result result DN.
     * @result Error.
     */
    aos::Error ASN1EncodeDN(const aos::String& commonName, aos::Array<uint8_t>& result) override;

    /**
     * Returns text representation of x509 distinguished name(DN).
     *
     * @param dn source binary representation of DN.
     * @param[out] result DN text representation.
     * @result Error.
     */
    aos::Error ASN1DecodeDN(const aos::Array<uint8_t>& dn, aos::String& result) override;

    /**
     * Encodes array of object identifiers into ASN1 value.
     *
     * @param src array of object identifiers.
     * @param asn1Value result ASN1 value.
     */
    aos::Error ASN1EncodeObjectIds(
        const aos::Array<aos::crypto::asn1::ObjectIdentifier>& src, aos::Array<uint8_t>& asn1Value) override;

    /**
     * Encodes big integer in ASN1 format.
     *
     * @param number big integer.
     * @param[out] asn1Value result ASN1 value.
     * @result Error.
     */
    aos::Error ASN1EncodeBigInt(const aos::Array<uint8_t>& number, aos::Array<uint8_t>& asn1Value) override;

    /**
     * Creates ASN1 sequence from already encoded DER items.
     *
     * @param items DER encoded items.
     * @param[out] asn1Value result ASN1 value.
     * @result Error.
     */
    aos::Error ASN1EncodeDERSequence(
        const aos::Array<aos::Array<uint8_t>>& items, aos::Array<uint8_t>& asn1Value) override;

private:
    aos::Error ParseX509Certs(mbedtls_x509_crt* currentCrt, aos::crypto::x509::Certificate& cert);
    aos::Error GetX509CertExtensions(aos::crypto::x509::Certificate& cert, mbedtls_x509_crt* crt);
    void       GetX509CertData(aos::crypto::x509::Certificate& cert, mbedtls_x509_crt* crt);
};

} // namespace crypto
} // namespace aos

#endif
