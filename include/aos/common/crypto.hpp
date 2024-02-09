/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CRYPTO_HPP_
#define AOS_CRYPTO_HPP_

#include "aos/common/tools/array.hpp"
#include "aos/common/tools/enum.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/string.hpp"
#include "aos/common/tools/time.hpp"
#include "aos/common/types.hpp"

namespace aos {
namespace crypto {

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
 * Maximum length of numeric string representing ASN.1 Object Identifier.
 */
constexpr auto cASN1ObjIdLen = AOS_CONFIG_CRYPTO_ASN1_OBJECT_ID_LEN;

/**
 * Maximum size of a certificate ASN.1 Extension Value.
 */
constexpr auto cASN1ExtValueSize = AOS_CONFIG_CRYPTO_ASN1_EXTENSION_VALUE_SIZE;

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
 * RSA modulus size.
 */
constexpr auto cRSAModulusSize = AOS_CONFIG_CRYPTO_RSA_MODULUS_SIZE;

/**
 * Size of RSA public exponent.
 */
constexpr auto cRSAPubExponentSize = AOS_CONFIG_CRYPTO_RSA_PUB_EXPONENT_SIZE;

/**
 * ECDSA params OID size.
 */
constexpr auto cECDSAParamsOIDSize = AOS_CONFIG_CRYPTO_ECDSA_PARAMS_OID_SIZE;

/**
 * DER-encoded X9.62 ECPoint.
 */
constexpr auto cECDSAPointDERSize = AOS_CONFIG_CRYPTO_ECDSA_POINT_DER_SIZE;

/**
 * Max expected number of certificates in a chain stored in PEM file.
 */
constexpr auto cCertChainSize = AOS_CONFIG_CRYPTO_CERTS_CHAIN_SIZE;

/**
 * Maximum size of SHA2 digest.
 */
constexpr auto cSHA2DigestSize = AOS_CONFIG_CRYPTO_SHA2_DIGEST_SIZE;

/**
 * Maximum signature size.
 */
constexpr auto cSignatureSize = AOS_CONFIG_CRYPTO_SIGNATURE_SIZE;

/**
 * Supported key types.
 */
class KeyAlgorithm {
public:
    enum class Enum { eRSA, eECDSA };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sContentTypeStrings[] = {"RSA", "ECDSA"};
        return Array<const char* const>(sContentTypeStrings, ArraySize(sContentTypeStrings));
    };
};

using KeyTypeEnum = KeyAlgorithm::Enum;
using KeyType     = EnumStringer<KeyAlgorithm>;

/**
 * Public key interface.
 */
class PublicKeyItf {
public:
    /**
     * Returns type of a public key.
     *
     * @return KeyType,
     */
    virtual KeyType GetKeyType() const = 0;

    /**
     * Tests whether current key is equal to the provided one.
     *
     * @param pubKey public key.
     * @return bool.
     */
    virtual bool IsEqual(const PublicKeyItf& pubKey) const = 0;

    /**
     * Destroys object instance.
     */
    virtual ~PublicKeyItf() = default;
};

/**
 * Supported hash functions.
 */
class HashType {
public:
    enum class Enum { eNone, eSHA1, eSHA224, eSHA256, eSHA384, eSHA512 };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sContentTypeStrings[] = {"NONE", "SHA1", "SHA224", "SHA256", "SHA384", "SHA512"};
        return Array<const char* const>(sContentTypeStrings, ArraySize(sContentTypeStrings));
    };
};

using HashEnum = HashType::Enum;
using Hash     = EnumStringer<HashType>;

/**
 * Options being used while signing.
 */
struct SignOptions {
    /**
     * Hash function to be used when signing.
     */
    Hash mHash;
};

/**
 * Private key interface.
 */
class PrivateKeyItf {
public:
    /**
     * Returns public part of a private key.
     *
     * @return const PublicKeyItf&.
     */
    virtual const PublicKeyItf& GetPublic() const = 0;

    /**
     * Calculates a signature of a given digest.
     *
     * @param digest input hash digest.
     * @param options signing options.
     * @param[out] signature result signature.
     * @return Error.
     */
    virtual Error Sign(const Array<uint8_t>& digest, const SignOptions& options, Array<uint8_t>& signature) const = 0;

    /**
     * Decrypts a cipher message.
     *
     * @param cipher encrypted message.
     * @param[out] result decoded message.
     * @return Error.
     */
    virtual Error Decrypt(const Array<uint8_t>& cipher, Array<uint8_t>& result) const = 0;

    /**
     * Destroys object instance.
     */
    virtual ~PrivateKeyItf() = default;
};

/**
 * RSA public key.
 */
class RSAPublicKey : public PublicKeyItf {
public:
    /**
     * Constructs object instance.
     *
     * @param n modulus.
     * @param e public exponent.
     */
    RSAPublicKey(const Array<uint8_t>& n, const Array<uint8_t>& e)
        : mN(n)
        , mE(e)
    {
    }

    /**
     * Returns type of a public key.
     *
     * @return KeyType,
     */
    KeyType GetKeyType() const override { return KeyTypeEnum::eRSA; }

    /**
     * Tests whether current key is equal to the provided one.
     *
     * @param pubKey public key.
     * @return bool.
     */
    bool IsEqual(const PublicKeyItf& pubKey) const override
    {
        if (pubKey.GetKeyType() != KeyTypeEnum::eRSA) {
            return false;
        }

        const auto& otherKey = static_cast<const RSAPublicKey&>(pubKey);

        return otherKey.mN == mN && otherKey.mE == mE;
    }

    /**
     * Returns RSA public modulus.
     *
     * @return const Array<uint8_t>&.
     */
    const Array<uint8_t>& GetN() const { return mN; }

    /**
     * Returns RSA public exponent.
     *
     * @return const Array<uint8_t>&.
     */
    const Array<uint8_t>& GetE() const { return mE; }

private:
    StaticArray<uint8_t, cRSAModulusSize>     mN;
    StaticArray<uint8_t, cRSAPubExponentSize> mE;
};

/**
 * ECDSA public key.
 */
class ECDSAPublicKey : public PublicKeyItf {
public:
    /**
     * Constructs object instance.
     *
     * @param n modulus.
     * @param e public exponent.
     */
    ECDSAPublicKey(const Array<uint8_t>& params, const Array<uint8_t>& point)
        : mECParamsOID(params)
        , mECPoint(point)
    {
    }

    /**
     * Returns type of a public key.
     *
     * @return KeyType,
     */
    KeyType GetKeyType() const override { return KeyTypeEnum::eECDSA; }

    /**
     * Tests whether current key is equal to the provided one.
     *
     * @param pubKey public key.
     * @return bool.
     */
    bool IsEqual(const PublicKeyItf& pubKey) const override
    {
        if (pubKey.GetKeyType() != KeyTypeEnum::eECDSA) {
            return false;
        }

        const auto& otherKey = static_cast<const ECDSAPublicKey&>(pubKey);

        return otherKey.mECParamsOID == mECParamsOID && otherKey.mECPoint == mECPoint;
    }

    /**
     * Returns ECDSA params OID.
     *
     * @return const Array<uint8_t>&.
     */
    const Array<uint8_t>& GetECParamsOID() const { return mECParamsOID; }

    /**
     * Returns ECDSA point.
     *
     * @return const Array<uint8_t>&.
     */
    const Array<uint8_t>& GetECPoint() const { return mECPoint; }

private:
    StaticArray<uint8_t, cECDSAParamsOIDSize> mECParamsOID;
    StaticArray<uint8_t, cECDSAPointDERSize>  mECPoint;
};

namespace asn1 {
/**
 * ASN.1 OBJECT IDENTIFIER
 */
using ObjectIdentifier = StaticString<cASN1ObjIdLen>;

/**
 * ASN.1 structure extension. RFC 580, section 4.2
 */
struct Extension {
    ObjectIdentifier                        mId;
    StaticArray<uint8_t, cASN1ExtValueSize> mValue;

    /**
     * Checks whether current object is equal the the given one.
     *
     * @param extension object to compare with.
     * @return bool.
     */
    bool operator==(const Extension& extension) const { return extension.mId == mId && extension.mValue == mValue; }
    /**
     * Checks whether current object is not equal the the given one.
     *
     * @param extension object to compare with.
     * @return bool.
     */
    bool operator!=(const Extension& extension) const { return !operator==(extension); }
};

} // namespace asn1

namespace x509 {

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
     * Certificate validity period.
     */
    Time mNotBefore, mNotAfter;
    /**
     * Public key.
     */
    SharedPtr<PublicKeyItf> mPublicKey;
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
     * Reads certificates from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @param[out] resultCerts result certificate chain.
     * @result Error.
     */
    virtual Error PEMToX509Certs(const String& pemBlob, Array<Certificate>& resultCerts) = 0;

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
     * Destroys object instance.
     */
    virtual ~ProviderItf() = default;
};

/**
 * A chain of certificates.
 */
using CertificateChain = StaticArray<Certificate, cCertChainSize>;

} // namespace x509
} // namespace crypto
} // namespace aos

#endif
