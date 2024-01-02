/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CRYPTO_HPP_
#define AOS_CRYPTO_HPP_

#include "aos/common/tools/array.hpp"
#include "aos/common/tools/string.hpp"

#include "aos/common/tools/memory.hpp"
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
 * Maximum size of a PEM certificate.
 */
constexpr auto cPEMCertSize = AOS_CONFIG_CRYPTO_PEM_CERT_SIZE;

/**
 * Maximum size of a DER certificate.
 */
constexpr auto cDERCertSize = AOS_CONFIG_CRYPTO_DER_CERT_SIZE;

/**
 *  Serial number size(in bytes).
 */
constexpr auto cSerialNumSize = AOS_CONFIG_CRYPTO_SERIAL_NUM_SIZE;

/**
 *  Length of serial number in string representation.
 */
constexpr auto cSerialNumStrLen = AOS_CONFIG_CRYPTO_SERIAL_NUM_STR_LEN;

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
 * Supported key types.
 */
enum class KeyType { eRSA, eECDSA };

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
    KeyType GetKeyType() const override { return KeyType::eRSA; }

    /**
     * Tests whether current key is equal to the provided one.
     *
     * @param pubKey public key.
     * @return bool.
     */
    bool IsEqual(const PublicKeyItf& pubKey) const override
    {
        if (pubKey.GetKeyType() != KeyType::eRSA) {
            return false;
        }

        const auto& otherKey = static_cast<const RSAPublicKey&>(pubKey);

        return otherKey.mN == mN && otherKey.mE == mE;
    }

private:
    StaticArray<uint8_t, cRSAModulusSize>     mN;
    StaticArray<uint8_t, cRSAPubExponentSize> mE;
};

/**
 * RSA private key.
 */
class RSAPrivateKey : public PrivateKeyItf {
public:
    /**
     * Constructs object instance.
     *
     * @param pubKey public key component of a private key.
     */
    RSAPrivateKey(const RSAPublicKey& pubKey)
        : mPubKey(pubKey)
    {
    }

    /**
     * Returns public part of a private key.
     *
     * @return const PublicKeyItf&.
     */
    const PublicKeyItf& GetPublic() const override { return mPubKey; }

private:
    RSAPublicKey mPubKey;
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
    KeyType GetKeyType() const override { return KeyType::eECDSA; }

    /**
     * Tests whether current key is equal to the provided one.
     *
     * @param pubKey public key.
     * @return bool.
     */
    bool IsEqual(const PublicKeyItf& pubKey) const override
    {
        if (pubKey.GetKeyType() != KeyType::eECDSA) {
            return false;
        }

        const auto& otherKey = static_cast<const ECDSAPublicKey&>(pubKey);

        return otherKey.mECParamsOID == mECParamsOID && otherKey.mECPoint == mECPoint;
    }

private:
    StaticArray<uint8_t, cECDSAParamsOIDSize> mECParamsOID;
    StaticArray<uint8_t, cECDSAPointDERSize>  mECPoint;
};

/**
 * ECDSA private key.
 */
class ECDSAPrivateKey : public PrivateKeyItf {
public:
    /**
     * Constructs object instance.
     *
     * @param pubKey public key component of a private key.
     */
    ECDSAPrivateKey(const ECDSAPublicKey& pubKey)
        : mPubKey(pubKey)
    {
    }

    /**
     * Returns public part of a private key.
     *
     * @return const PublicKeyItf&.
     */
    const PublicKeyItf& GetPublic() const override { return mPubKey; }

private:
    ECDSAPublicKey mPubKey;
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

/**
 * Encodes array of object identifiers into ASN1 value.
 *
 * @param src array of object identifiers.
 * @param asn1Value result ASN1 value.
 */

inline void Encode(const Array<ObjectIdentifier>& src, Array<uint8_t>& asn1Value)
{
    (void)src;
    (void)asn1Value;
}

/**
 * Encodes big integer in ASN1 format.
 *
 * @param number big integer.
 * @param[out] asn1Value result ASN1 value.
 * @result Error.
 */
inline Error EncodeBigInt(const Array<uint8_t>& number, Array<uint8_t>& asn1Value)
{
    // Notes:
    // Current draft implementation does no encoding, just copies number into result buffer.
    // This is required for cryptoki wrapper tests, which should be provided with already encoded number.

    asn1Value.Clear();

    return asn1Value.Insert(asn1Value.end(), number.begin(), number.end());
}

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
    StaticArray<uint8_t, cDERCertSize> mRaw;
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
     * @param[out] resultCert result certificate in PEM format.
     * @result Error.
     */
    virtual Error CreateCertificate(
        const Certificate& templ, const Certificate& parent, const PrivateKeyItf& privKey, Array<uint8_t>& pemCert)
        = 0;

    /**
     * Reads certificates from a PEM blob.
     *
     * @param pemBlob raw certificates in a PEM format.
     * @param[out] resultCerts result certificate chain.
     * @result Error.
     */
    virtual Error PEMToX509Certs(const Array<uint8_t>& pemBlob, Array<Certificate>& resultCerts) = 0;

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
    virtual Error CreateCSR(const CSR& templ, const PrivateKeyItf& privKey, Array<uint8_t>& pemCSR) = 0;

    /**
     * Constructs x509 distinguished name(DN) from the argument list.
     *
     * @param comName common name.
     * @param[out] result result DN.
     * @result Error.
     */
    virtual Error CreateDN(const String& commonName, Array<uint8_t>& result) = 0;

    /**
     * Returns text representation of x509 distinguished name(DN).
     *
     * @param dn source binary representation of DN.
     * @param[out] result DN text representation.
     * @result Error.
     */
    virtual Error DNToString(const Array<uint8_t>& dn, String& result) = 0;

    /**
     * Destroys object instance.
     */
    virtual ~ProviderItf() = default;
};

} // namespace x509
} // namespace crypto
} // namespace aos

#endif
