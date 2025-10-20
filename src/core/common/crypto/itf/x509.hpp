/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_X509_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_X509_HPP_

#include <core/common/consts.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/variant.hpp>

#include "asn1.hpp"
#include "privkey.hpp"

namespace aos::crypto {
/**
 * Certificate issuer name max length.
 */
constexpr auto cCertIssuerSize = AOS_CONFIG_CRYPTO_CERT_ISSUER_SIZE;

/**
 * Max length of a DNS name.
 */
constexpr auto cDNSNameLen = AOS_CONFIG_CRYPTO_DNS_NAME_LEN;

/**
 * Max number of alternative names for a module.
 */
constexpr auto cAltDNSNamesCount = AOS_CONFIG_CRYPTO_ALT_DNS_NAMES_MAX_COUNT;

/**
 * Certificate subject size.
 */
constexpr auto cCertSubjSize = AOS_CONFIG_CRYPTO_CERT_ISSUER_SIZE;

/**
 * Maximum length of distinguished name string representation.
 */
constexpr auto cCertDNStringSize = AOS_CONFIG_CRYPTO_DN_STRING_SIZE;

/**
 * Certificate extra extensions max number.
 */
constexpr auto cCertExtraExtCount = AOS_CONFIG_CRYPTO_EXTRA_EXTENSIONS_COUNT;

/**
 * Maximum certificate key id size(in bytes).
 */
constexpr auto cCertKeyIdSize = AOS_CONFIG_CRYPTO_CERT_KEY_ID_SIZE;

/**
 * Maximum length of a PEM certificate.
 */
constexpr auto cCertPEMLen = AOS_CONFIG_CRYPTO_CERT_PEM_LEN;

/**
 * Maximum size of a DER certificate.
 */
constexpr auto cCertDERSize = AOS_CONFIG_CRYPTO_CERT_DER_SIZE;

/**
 * Maximum length of CSR in PEM format.
 */
constexpr auto cCSRPEMLen = AOS_CONFIG_CRYPTO_CSR_PEM_LEN;

/**
 * Maximum length of private key in PEM format.
 */
constexpr auto cPrivKeyPEMLen = AOS_CONFIG_CRYPTO_PRIVKEY_PEM_LEN;

/**
 *  Serial number size(in bytes).
 */
constexpr auto cSerialNumSize = AOS_CONFIG_CRYPTO_SERIAL_NUM_SIZE;

/**
 *  Length of serial number in string representation.
 */
constexpr auto cSerialNumStrLen = cSerialNumSize * 2;

/**
 *  Maximum size of serial number encoded in DER format.
 */
constexpr auto cSerialNumDERSize = AOS_CONFIG_CRYPTO_SERIAL_NUM_DER_SIZE;

/**
 * Subject common name length.
 */
constexpr auto cSubjectCommonNameLen = AOS_CONFIG_CRYPTO_SUBJECT_COMMON_NAME_LEN;

/**
 * Max expected number of certificates in a chain stored in PEM file.
 */
constexpr auto cCertChainSize = AOS_CONFIG_CRYPTO_CERTS_CHAIN_SIZE;

/**
 * Number of certificate chains to be stored in crypto::CertLoader.
 */
constexpr auto cCertChainsCount = AOS_CONFIG_CRYPTO_CERTIFICATE_CHAINS_COUNT;

/**
 *  PEM certificate chain length.
 */
constexpr auto cCertChainPEMLen = cCertChainSize * cCertPEMLen;

/**
 * Maximum signature size.
 */
constexpr auto cSignatureSize = AOS_CONFIG_CRYPTO_SIGNATURE_SIZE;

/**
 * Max number of certificates.
 */
constexpr auto cMaxNumCertificates = AOS_CONFIG_CRYPTO_MAX_NUM_CERTIFICATES;

namespace x509 {

/**
 * Padding type.
 */
class PaddingType {
public:
    enum class Enum { ePKCS1v1_5, ePSS, eNone };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sContentTypeStrings[] = {"PKCS1v1_5", "PSS", "Node"};
        return Array<const char* const>(sContentTypeStrings, ArraySize(sContentTypeStrings));
    };
};

using PaddingEnum = PaddingType::Enum;
using Padding     = EnumStringer<PaddingType>;

/**
 * Verify options.
 */
struct VerifyOptions {
    Time mCurrentTime;
};

/**
 * x509 Certificate.
 */
struct Certificate {
    /**
     * DER encoded certificate subject.
     */
    StaticArray<uint8_t, cCertSubjSize> mSubject;
    /**
     * Certificate subject key id.
     */
    StaticArray<uint8_t, cCertKeyIdSize> mSubjectKeyId;
    /**
     * Certificate authority key id.
     */
    StaticArray<uint8_t, cCertKeyIdSize> mAuthorityKeyId;
    /**
     * DER encoded certificate subject issuer.
     */
    StaticArray<uint8_t, cCertIssuerSize> mIssuer;
    /**
     * Certificate serial number.
     */
    StaticArray<uint8_t, cSerialNumSize> mSerial;
    /**
     * Issuer URLs.
     */
    StaticArray<StaticString<cURLLen>, cMaxNumURLs> mIssuerURLs;
    /**
     * Certificate validity period.
     */
    Time mNotBefore, mNotAfter;
    /**
     * Public key.
     */
    Variant<ECDSAPublicKey, RSAPublicKey> mPublicKey;
    /**
     * Complete ASN.1 DER content (certificate, signature algorithm and signature).
     */
    StaticArray<uint8_t, cCertDERSize> mRaw;
};

/**
 * x509 Certificate request.
 */
struct CSR {
    /**
     * Certificate subject size.
     */
    StaticArray<uint8_t, cCertSubjSize> mSubject;
    /**
     * Alternative DNS names.
     */
    StaticArray<StaticString<cDNSNameLen>, cAltDNSNamesCount> mDNSNames;
    /**
     * Contains extra extensions applied to CSR.
     */
    StaticArray<asn1::Extension, cCertExtraExtCount> mExtraExtensions;
};

/**
 * Provides interface to manage certificate requests.
 */
class ProviderItf {
public:
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
    virtual Error CreateCertificate(
        const Certificate& templ, const Certificate& parent, const PrivateKeyItf& privKey, String& pemCert)
        = 0;

    /**
     * Creates certificate chain using client CSR & CA key/certificate as input.
     *
     * @param pemCSR client certificate request.
     * @param pemCAKey CA private key in PEM.
     * @param pemCACert CA certificate in PEM.
     * @param serial serial number of certificate.
     * @param[out] pemClientCert result certificate.
     * @result Error.
     */
    virtual Error CreateClientCert(
        const String& csr, const String& caKey, const String& caCert, const Array<uint8_t>& serial, String& clientCert)
        = 0;

    /**
     * Reads certificates from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @param[out] resultCerts result certificate chain.
     * @result Error.
     */
    virtual Error PEMToX509Certs(const String& pemBlob, Array<Certificate>& resultCerts) = 0;

    /**
     * Serializes input certificate object into a PEM blob.
     *
     * @param certificate input certificate object.
     * @param[out] dst destination buffer.
     * @result Error.
     */
    virtual Error X509CertToPEM(const Certificate& certificate, String& dst) = 0;

    /**
     * Reads private key from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @result RetWithError<SharedPtr<PrivateKeyItf>>.
     */
    virtual RetWithError<SharedPtr<PrivateKeyItf>> PEMToX509PrivKey(const String& pemBlob) = 0;

    /**
     * Reads certificate from a DER blob.
     *
     * @param derBlob raw certificate in a DER format.
     * @param[out] resultCert result certificate.
     * @result Error.
     */
    virtual Error DERToX509Cert(const Array<uint8_t>& derBlob, Certificate& resultCert) = 0;

    /**
     * Creates a new certificate request, based on a template.
     *
     * @param templ template for a new certificate request.
     * @param privKey private key.
     * @param[out] pemCSR result CSR in PEM format.
     * @result Error.
     */
    virtual Error CreateCSR(const CSR& templ, const PrivateKeyItf& privKey, String& pemCSR) = 0;

    /**
     * Constructs x509 distinguished name(DN) from the argument list.
     *
     * @param comName common name.
     * @param[out] result result DN.
     * @result Error.
     */
    virtual Error ASN1EncodeDN(const String& commonName, Array<uint8_t>& result) = 0;

    /**
     * Returns text representation of x509 distinguished name(DN).
     *
     * @param dn source binary representation of DN.
     * @param[out] result DN text representation.
     * @result Error.
     */
    virtual Error ASN1DecodeDN(const Array<uint8_t>& dn, String& result) = 0;

    /**
     * Encodes array of object identifiers into ASN1 value.
     *
     * @param src array of object identifiers.
     * @param asn1Value result ASN1 value.
     */
    virtual Error ASN1EncodeObjectIds(const Array<asn1::ObjectIdentifier>& src, Array<uint8_t>& asn1Value) = 0;

    /**
     * Encodes big integer in ASN1 format.
     *
     * @param number big integer.
     * @param[out] asn1Value result ASN1 value.
     * @result Error.
     */
    virtual Error ASN1EncodeBigInt(const Array<uint8_t>& number, Array<uint8_t>& asn1Value) = 0;

    /**
     * Creates ASN1 sequence from already encoded DER items.
     *
     * @param items DER encoded items.
     * @param[out] asn1Value result ASN1 value.
     * @result Error.
     */
    virtual Error ASN1EncodeDERSequence(const Array<Array<uint8_t>>& items, Array<uint8_t>& asn1Value) = 0;

    /**
     * Returns value of the input ASN1 OCTETSTRING.
     *
     * @param src DER encoded OCTETSTRING value.
     * @param[out] dst decoded value.
     * @result Error.
     */
    virtual Error ASN1DecodeOctetString(const Array<uint8_t>& src, Array<uint8_t>& dst) = 0;

    /**
     * Decodes input ASN1 OID value.
     *
     * @param inOID input ASN1 OID value.
     * @param[out] dst decoded value.
     * @result Error.
     */
    virtual Error ASN1DecodeOID(const Array<uint8_t>& inOID, Array<uint8_t>& dst) = 0;

    /**
     * Verifies a digital signature using the provided public key and digest.
     *
     * @param pubKey public key.
     * @param hashFunc hash function that was used to produce the digest.
     * @param padding padding type.
     * @param digest message digest to verify.
     * @param signature signature to verify against the digest.
     * @return Error.
     */
    virtual Error Verify(const Variant<ECDSAPublicKey, RSAPublicKey>& pubKey, Hash hashFunc, Padding padding,
        const Array<uint8_t>& digest, const Array<uint8_t>& signature)
        = 0;

    /**
     * Verifies the certificate against a chain of intermediate and root certificates.
     *
     * @param rootCerts trusted root certificates.
     * @param intermCerts intermediate certificate chain used to build the path to a trusted root.
     * @param options verify options.
     * @param cert certificate to verify.
     * @return Error.
     */
    virtual Error Verify(const Array<Certificate>& rootCerts, const Array<Certificate>& intermCerts,
        const VerifyOptions& options, const Certificate& cert)
        = 0;

    /**
     * Destroys object instance.
     */
    virtual ~ProviderItf() = default;
};

/**
 * A chain of certificates.
 */
using CertificateChain = StaticArray<Certificate, cCertChainSize>;

} // namespace x509
} // namespace aos::crypto

#endif
