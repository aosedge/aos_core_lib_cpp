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
class MbedTLSCryptoProvider : public x509::ProviderItf {
public:
    /**
     * Initializes the object.
     *
     * @result Error.
     */
    Error Init();

    /**
     * Creates a new certificate based on a template.
     *
     * @param templ a pattern for a new certificate.
     * @param parent a parent certificate in a certificate chain.
     * @param pubKey public key.
     * @param privKey private key.
     * @param[out] pemCert result certificate in PEM format.
     * @result Error.
     */
    Error CreateCertificate(const x509::Certificate& templ, const x509::Certificate& parent,
        const PrivateKeyItf& privKey, String& pemCert) override;

    /**
     * Reads certificates from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @param[out] resultCerts result certificate chain.
     * @result Error.
     */
    Error PEMToX509Certs(const String& pemBlob, Array<x509::Certificate>& resultCerts) override;

    /**
     * Reads certificate from a DER blob.
     *
     * @param derBlob raw certificate in a DER format.
     * @param[out] resultCert result certificate.
     * @result Error.
     */
    Error DERToX509Cert(const Array<uint8_t>& derBlob, x509::Certificate& resultCert) override;

    /**
     * Creates a new certificate request, based on a template.
     *
     * @param templ template for a new certificate request.
     * @param privKey private key.
     * @param[out] pemCSR result CSR in PEM format.
     * @result Error.
     */
    Error CreateCSR(const x509::CSR& templ, const PrivateKeyItf& privKey, String& pemCSR) override;

    /**
     * Reads private key from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @result RetWithError<SharedPtr<PrivateKeyItf>>.
     */
    RetWithError<SharedPtr<PrivateKeyItf>> PEMToX509PrivKey(const String& pemBlob) override;

    /**
     * Constructs x509 distinguished name(DN) from the argument list.
     *
     * @param comName common name.
     * @param[out] result result DN.
     * @result Error.
     */
    Error ASN1EncodeDN(const String& commonName, Array<uint8_t>& result) override;

    /**
     * Returns text representation of x509 distinguished name(DN).
     *
     * @param dn source binary representation of DN.
     * @param[out] result DN text representation.
     * @result Error.
     */
    Error ASN1DecodeDN(const Array<uint8_t>& dn, String& result) override;

    /**
     * Encodes array of object identifiers into ASN1 value.
     *
     * @param src array of object identifiers.
     * @param asn1Value result ASN1 value.
     */
    Error ASN1EncodeObjectIds(const Array<asn1::ObjectIdentifier>& src, Array<uint8_t>& asn1Value) override;

    /**
     * Encodes big integer in ASN1 format.
     *
     * @param number big integer.
     * @param[out] asn1Value result ASN1 value.
     * @result Error.
     */
    Error ASN1EncodeBigInt(const Array<uint8_t>& number, Array<uint8_t>& asn1Value) override;

    /**
     * Creates ASN1 sequence from already encoded DER items.
     *
     * @param items DER encoded items.
     * @param[out] asn1Value result ASN1 value.
     * @result Error.
     */
    Error ASN1EncodeDERSequence(const Array<Array<uint8_t>>& items, Array<uint8_t>& asn1Value) override;

private:
    static constexpr auto cAllocatorSize
        = AOS_CONFIG_CRYPTOPROVIDER_PUB_KEYS_COUNT * Max(sizeof(RSAPublicKey), sizeof(ECDSAPublicKey));

    Error              ParseX509Certs(mbedtls_x509_crt* currentCrt, x509::Certificate& cert);
    Error              GetX509CertExtensions(x509::Certificate& cert, mbedtls_x509_crt* crt);
    Error              GetX509CertData(x509::Certificate& cert, mbedtls_x509_crt* crt);
    Error              ParseX509CertPublicKey(const mbedtls_pk_context* pk, x509::Certificate& cert);
    Error              ParseRSAKey(const mbedtls_rsa_context* rsa, x509::Certificate& cert);
    Error              ParseECKey(const mbedtls_ecp_keypair* eckey, x509::Certificate& cert);
    RetWithError<Time> ConvertTime(const mbedtls_x509_time& src);

    void  InitializeCSR(mbedtls_x509write_csr& csr, mbedtls_pk_context& pk);
    Error SetCSRProperties(mbedtls_x509write_csr& csr, mbedtls_pk_context& pk, const x509::CSR& templ);
    Error SetCSRAlternativeNames(mbedtls_x509write_csr& csr, const x509::CSR& templ);
    Error SetCSRExtraExtensions(mbedtls_x509write_csr& csr, const x509::CSR& templ);
    Error WriteCSRPem(mbedtls_x509write_csr& csr, String& pemCSR);

    RetWithError<mbedtls_svc_key_id_t> SetupOpaqueKey(mbedtls_pk_context& pk, const PrivateKeyItf& privKey);

    Error InitializeCertificate(mbedtls_x509write_cert& cert, mbedtls_pk_context& pk,
        mbedtls_ctr_drbg_context& ctr_drbg, mbedtls_entropy_context& entropy);
    Error SetCertificateProperties(mbedtls_x509write_cert& cert, mbedtls_pk_context& pk,
        mbedtls_ctr_drbg_context& ctrDrbg, const x509::Certificate& templ, const x509::Certificate& parent);
    Error WriteCertificatePem(mbedtls_x509write_cert& cert, String& pemCert);
    Error SetCertificateSerialNumber(
        mbedtls_x509write_cert& cert, mbedtls_ctr_drbg_context& ctrDrbg, const x509::Certificate& templ);
    Error SetCertificateSubjectKeyIdentifier(mbedtls_x509write_cert& cert, const x509::Certificate& templ);
    Error SetCertificateAuthorityKeyIdentifier(
        mbedtls_x509write_cert& cert, const x509::Certificate& templ, const x509::Certificate& parent);
    Error SetCertificateValidityPeriod(mbedtls_x509write_cert& cert, const x509::Certificate& templ);

    StaticAllocator<cAllocatorSize> mAllocator;
};

} // namespace crypto
} // namespace aos

#endif
