/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_OPENSSL_CRYPTOPROVIDER_HPP_
#define AOS_CORE_COMMON_CRYPTO_OPENSSL_CRYPTOPROVIDER_HPP_

#include <core/common/config.hpp>

#include "../crypto.hpp"
#include "opensslprovider.hpp"

namespace aos::crypto {

/**
 * OpenSSLCryptoProvider provider.
 */
class OpenSSLCryptoProvider : public CryptoProviderItf {
public:
    /**
     * Destructor.
     */
    ~OpenSSLCryptoProvider();

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
     * Creates certificate chain using client CSR & CA key/certificate as input.
     *
     * @param csr client certificate request in a PEM format.
     * @param caKey CA private key in PEM.
     * @param caCert CA certificate in PEM.
     * @param serial serial number of certificate.
     * @param[out] pemClientCert result certificate.
     * @result Error.
     */
    Error CreateClientCert(const String& csr, const String& caKey, const String& caCert, const Array<uint8_t>& serial,
        String& clientCert) override;

    /**
     * Reads certificates from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @param[out] resultCerts result certificate chain.
     * @result Error.
     */
    Error PEMToX509Certs(const String& pemBlob, Array<x509::Certificate>& resultCerts) override;

    /**
     * Serializes input certificate object into a PEM blob.
     *
     * @param certificate input certificate object.
     * @param[out] dst destination buffer.
     * @result Error.
     */
    Error X509CertToPEM(const x509::Certificate& certificate, String& dst) override;

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

    /**
     * Returns value of the input ASN1 OCTETSTRING.
     *
     * @param src DER encoded OCTETSTRING value.
     * @param[out] dst decoded value.
     * @result Error.
     */
    Error ASN1DecodeOctetString(const Array<uint8_t>& src, Array<uint8_t>& dst) override;

    /**
     * Decodes input ASN1 OID value.
     *
     * @param inOID input ASN1 OID value.
     * @param[out] dst decoded value.
     * @result Error.
     */
    Error ASN1DecodeOID(const Array<uint8_t>& inOID, Array<uint8_t>& dst) override;

    /**
     * Creates hash instance.
     *
     * @param algorithm hash algorithm.
     * @return RetWithError<UniquePtr<HashItf>>.
     */
    RetWithError<UniquePtr<HashItf>> CreateHash(Hash algorithm) override;

    /**
     * Generates random integer value in range [0..maxValue].
     *
     * @param maxValue maximum value.
     * @return RetWithError<uint64_t>.
     */
    RetWithError<uint64_t> RandInt(uint64_t maxValue) override;

    /**
     * Generates random buffer.
     *
     * @param[out] buffer result buffer.
     * @param size buffer size.
     * @return Error.
     */
    Error RandBuffer(Array<uint8_t>& buffer, size_t size = 0) override;

    /**
     * Creates UUID v4.
     *
     * @return RetWithError<uuid::UUID>.
     */
    RetWithError<uuid::UUID> CreateUUIDv4() override;

    /**
     * Creates UUID version 5 based on a given namespace identifier and name.
     *
     * @param space namespace identifier.
     * @param name name.
     * @result RetWithError<uuid::UUID>.
     */
    RetWithError<uuid::UUID> CreateUUIDv5(const uuid::UUID& space, const Array<uint8_t>& name) override;

    /**
     * Creates a new AES encoder.
     *
     * @param mode AES mode: "CBC" supported only.
     * @param key encryption key.
     * @param iv initialization vector: must be 16 bytes for CBC mode.
     * @return RetWithError<UniquePtr<AESCipherItf>>.
     */
    RetWithError<UniquePtr<AESCipherItf>> CreateAESEncoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv) override;

    /**
     * Creates a new AES decoder.
     *
     * @param mode AES mode: "CBC" supported only.
     * @param key decryption key.
     * @param iv initialization vector: must be 16 bytes for CBC mode.
     * @return RetWithError<UniquePtr<AESCipherItf>>.
     */
    RetWithError<UniquePtr<AESCipherItf>> CreateAESDecoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv) override;

    /**
     * Verifies a digital signature using provided public key and digest.
     *
     * @param pubKey public key.
     * @param hashFunc hash function that was used to produce the digest.
     * @param padding padding type.
     * @param digest message digest to verify.
     * @param signature signature to verify against the digest.
     * @return Error.
     */
    Error Verify(const Variant<ECDSAPublicKey, RSAPublicKey>& pubKey, Hash hashFunc, Padding padding,
        const Array<uint8_t>& digest, const Array<uint8_t>& signature) override;

    /**
     * Verifies the certificate against a chain of intermediate and root certificates.
     *
     * @param rootCerts trusted root certificates.
     * @param intermCerts intermediate certificate chain used to build the path to a trusted root.
     * @param options verify options.
     * @param cert certificate to verify.
     * @return Error.
     */
    Error Verify(const Array<x509::Certificate>& rootCerts, const Array<x509::Certificate>& intermCerts,
        const VerifyOptions& options, const x509::Certificate& cert) override;

    /**
     * Discards an ASN.1 tag-length and invokes reader for its content.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param asn1reader reader handles content of the structure.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadStruct(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ASN1ReaderItf& asn1reader) override;

    /**
     * Reads an ASN.1 SET and invokes the reader for each element.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param asn1reader reader callback to handle each element.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadSet(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ASN1ReaderItf& asn1reader) override;

    /**
     * Reads an ASN.1 SEQUENCE and invokes the reader for each element.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param asn1reader reader callback to handle each element.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadSequence(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ASN1ReaderItf& asn1reader) override;

    /**
     * Reads an ASN.1 INTEGER value.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] value result integer.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadInteger(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, int& value) override;

    /**
     * Reads a large ASN.1 INTEGER (BigInt) as a byte array.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] result result integer bytes.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadBigInt(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, Array<uint8_t>& result) override;

    /**
     * Reads an ASN.1 Object Identifier (OID).
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] oid result OID.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadOID(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ObjectIdentifier& oid) override;

    /**
     * Reads an ASN.1 AlgorithmIdentifier(AID).
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] aid result AID.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadAID(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::AlgorithmIdentifier& aid) override;

    /**
     * Reads an ASN.1 OCTET STRING.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] result result octet string.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadOctetString(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, Array<uint8_t>& result) override;

    /**
     * Returns a raw ASN.1 value.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] result parsed ASN1 value.
     * @return ASN1ParseResult.
     */
    asn1::ASN1ParseResult ReadRawValue(
        const Array<uint8_t>& data, const asn1::ASN1ParseOptions& opt, asn1::ASN1Value& result) override;

private:
    class OpenSSLHash : public crypto::HashItf, private NonCopyable {
    public:
        Error Init(struct ossl_lib_ctx_st* libCtx, const char* mdtype);
        Error Update(const Array<uint8_t>& data) override;
        Error Finalize(Array<uint8_t>& hash) override;
        ~OpenSSLHash();

    private:
        struct evp_md_ctx_st* mMDCtx = nullptr;
        struct evp_md_st*     mType  = nullptr;
    };

    class OpenSSLAESCipher : public crypto::AESCipherItf, private NonCopyable {
    public:
        Error Init(
            struct ossl_lib_ctx_st* libCtx, const Array<uint8_t>& key, const Array<uint8_t>& iv, bool encrypt = true);
        Error EncryptBlock(const Block& input, Block& output) override;
        Error DecryptBlock(const Block& input, Block& output) override;
        Error Finalize(Block& output);
        ~OpenSSLAESCipher();

    private:
        bool                      mEncrypt    = false;
        struct evp_cipher_ctx_st* mCipherCtx  = nullptr;
        struct evp_cipher_st*     mCipherType = nullptr;
    };

    class OpenSSLRSAPrivKey : public crypto::PrivateKeyItf {
    public:
        Error               Init(struct evp_pkey_st* pkey);
        const PublicKeyItf& GetPublic() const override;
        Error Sign(const Array<uint8_t>& digest, const SignOptions& options, Array<uint8_t>& signature) const override;
        Error Decrypt(
            const Array<uint8_t>& cipher, const DecryptionOptions& options, Array<uint8_t>& result) const override;
        ~OpenSSLRSAPrivKey();

    private:
        struct evp_pkey_st* mPrivKey = nullptr;
    };

    static constexpr auto cAllocatorSize
        = AOS_CONFIG_CRYPTO_PUB_KEYS_COUNT * Max(sizeof(RSAPublicKey), sizeof(ECDSAPublicKey))
        + AOS_CONFIG_CRYPTO_HASHER_COUNT * sizeof(OpenSSLHash)
        + AOS_CONFIG_CRYPTO_AES_CIPHER_COUNT * sizeof(OpenSSLAESCipher)
        + AOS_CONFIG_CRYPTO_PRIV_KEYS_COUNT * sizeof(OpenSSLRSAPrivKey);

    ossl_lib_ctx_st*         mLibCtx = nullptr;
    openssl::OpenSSLProvider mOpenSSLProvider;

    StaticAllocator<cAllocatorSize> mAllocator;
};

} // namespace aos::crypto

#endif
