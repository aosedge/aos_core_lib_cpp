/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_CRYPTO_HPP_
#define AOS_CORE_COMMON_CRYPTO_CRYPTO_HPP_

#include <core/common/tools/array.hpp>
#include <core/common/tools/enum.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/time.hpp>
#include <core/common/tools/uuid.hpp>
#include <core/common/tools/variant.hpp>
#include <core/common/types/types.hpp>

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
 * Number of certificate chains to be stored in crypto::CertLoader.
 */
constexpr auto cCertChainsCount = AOS_CONFIG_CRYPTO_CERTIFICATE_CHAINS_COUNT;

/**
 * Maximum size of SHA2 digest.
 */
constexpr auto cSHA2DigestSize = AOS_CONFIG_CRYPTO_SHA2_DIGEST_SIZE;

/**
 * Maximum size of SHA1 digest.
 */
constexpr auto cSHA1DigestSize = AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE;

/**
 * Maximum size of input data for SHA1 hash calculation.
 */
constexpr auto cSHA1InputDataSize = AOS_CONFIG_CRYPTO_SHA1_INPUT_SIZE;

/**
 * Maximum signature size.
 */
constexpr auto cSignatureSize = AOS_CONFIG_CRYPTO_SIGNATURE_SIZE;

/**
 * Max number of certificates.
 */
constexpr auto cMaxNumCertificates = AOS_CONFIG_CRYPTO_MAX_NUM_CERTIFICATES;

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
    enum class Enum {
        eSHA1,
        eSHA224,
        eSHA256,
        eSHA384,
        eSHA512,
        eSHA512_224,
        eSHA512_256,
        eSHA3_224,
        eSHA3_256,
        eNone,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sContentTypeStrings[] = {
            "SHA1",
            "SHA224",
            "SHA256",
            "SHA384",
            "SHA512",
            "SHA512-224",
            "SHA512-256",
            "SHA3-224",
            "SHA3-256",
            "NONE",
        };
        return Array<const char* const>(sContentTypeStrings, ArraySize(sContentTypeStrings));
    };
};

using HashEnum = HashType::Enum;
using Hash     = EnumStringer<HashType>;

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
 * Hash interface.
 */
class HashItf {
public:
    /**
     * Updates hash with input data.
     *
     * @param data input data.
     * @return Error.
     */
    virtual Error Update(const Array<uint8_t>& data) = 0;

    /**
     * Finalizes hash calculation.
     *
     * @param[out] hash result hash.
     * @return Error.
     */
    virtual Error Finalize(Array<uint8_t>& hash) = 0;

    /**
     * Destructor.
     */
    virtual ~HashItf() = default;
};

/**
 * Hasher interface.
 */
class HasherItf {
public:
    /**
     * Creates hash instance.
     *
     * @param algorithm hash algorithm.
     * @return RetWithError<UniquePtr<HashItf>>.
     */
    virtual RetWithError<UniquePtr<HashItf>> CreateHash(Hash algorithm) = 0;

    /**
     * Destructor.
     */
    virtual ~HasherItf() = default;
};

class RandomItf {
public:
    /**
     * Generates random integer value in range [0..maxValue].
     *
     * @param maxValue maximum value.
     * @return RetWithError<uint64_t>.
     */
    virtual RetWithError<uint64_t> RandInt(uint64_t maxValue) = 0;

    /**
     * Generates random buffer.
     *
     * @param[out] buffer result buffer.
     * @param size buffer size.
     * @return Error.
     */
    virtual Error RandBuffer(Array<uint8_t>& buffer, size_t size = 0) = 0;

    /**
     * Destructor.
     */
    virtual ~RandomItf() = default;
};

/***
 * UUID generator interface.
 */
class UUIDItf {
public:
    /**
     * Creates UUID v4.
     *
     * @return RetWithError<uuid::UUID>.
     */
    virtual RetWithError<uuid::UUID> CreateUUIDv4() = 0;

    /**
     * Creates UUID version 5 based on a given namespace identifier and name.
     *
     * @param space namespace identifier.
     * @param name name.
     * @result RetWithError<uuid::UUID>.
     */
    virtual RetWithError<uuid::UUID> CreateUUIDv5(const uuid::UUID& space, const Array<uint8_t>& name) = 0;

    /**
     * Destructor.
     */
    virtual ~UUIDItf() = default;
};

/**
 * AES cipher interface for 16-byte block encryption/decryption.
 */
class AESCipherItf {
public:
    /**
     * AES block.
     */
    using Block = StaticArray<uint8_t, 16>;

    /**
     * Destructor.
     */
    virtual ~AESCipherItf() = default;

    /**
     * Encrypts a 16-byte block.
     *
     * @param input  input block.
     * @param[out] output encrypted block.
     * @return Error.
     */
    virtual Error EncryptBlock(const Block& input, Block& output) = 0;

    /**
     * Decrypts a 16-byte block.
     *
     * @param input  input block.
     * @param[out] output decrypted block.
     * @return Error.
     */
    virtual Error DecryptBlock(const Block& input, Block& output) = 0;

    /**
     * Finalizes encription/decryption.
     *
     * @param[out] output final block.
     * @return Error.
     */
    virtual Error Finalize(Block& output) = 0;
};

/**
 * Interface for AES encoding/decoding.
 */
class AESEncoderDecoderItf {
public:
    /**
     * Destructor.
     */
    virtual ~AESEncoderDecoderItf() = default;

    /**
     * Creates a new AES encoder.
     *
     * @param mode AES mode: "CBC" supported only.
     * @param key encryption key.
     * @param iv initialization vector: must be 16 bytes for CBC mode.
     * @return RetWithError<UniquePtr<AESCipherItf>>.
     */
    virtual RetWithError<UniquePtr<AESCipherItf>> CreateAESEncoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv)
        = 0;

    /**
     * Creates a new AES decoder.
     *
     * @param mode AES mode: "CBC" supported only.
     * @param key decryption key.
     * @param iv initialization vector: must be 16 bytes for CBC mode.
     * @return RetWithError<UniquePtr<AESCipherItf>>.
     */
    virtual RetWithError<UniquePtr<AESCipherItf>> CreateAESDecoder(
        const String& mode, const Array<uint8_t>& key, const Array<uint8_t>& iv)
        = 0;
};

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
 * PKCS1v15 decryption options.
 */
struct PKCS1v15DecryptionOptions {
    int mKeySize = 0;
};

/**
 * OAEP decryption options.
 */
struct OAEPDecryptionOptions {
    Hash mHash;
};

/**
 * Decryption options.
 */
using DecryptionOptions = Variant<PKCS1v15DecryptionOptions, OAEPDecryptionOptions>;

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
     * @param options decryption options.
     * @param[out] result decoded message.
     * @return Error.
     */
    virtual Error Decrypt(const Array<uint8_t>& cipher, const DecryptionOptions& options, Array<uint8_t>& result) const
        = 0;

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
 * ASN1 value.
 */
struct ASN1Value {
    int            mTagClass {};
    int            mTagNumber {};
    bool           mIsConstructed {};
    Array<uint8_t> mValue;

    /**
     * Constructor.
     */
    ASN1Value() = default;

    /**
     * Constructor.
     *
     * @param tagClass ASN.1 tag class.
     * @param tagNumber ASN.1 tag number.
     * @param isConstructed indicates whether this value is a constructed type (true) or a primitive type (false).
     * @param content raw content of the ASN.1 value.
     */
    ASN1Value(int tagClass, int tagNumber, bool isConstructed, const Array<uint8_t>& content)
        : mTagClass(tagClass)
        , mTagNumber(tagNumber)
        , mIsConstructed(isConstructed)
        , mValue(content)
    {
    }

    /**
     * Constructor.
     */
    ASN1Value(const ASN1Value& other) { *this = other; }

    /**
     * Copy operator.
     */
    ASN1Value& operator=(const ASN1Value& other)
    {
        mTagClass      = other.mTagClass;
        mTagNumber     = other.mTagNumber;
        mIsConstructed = other.mIsConstructed;
        mValue.Rebind(other.mValue);

        return *this;
    }

    /**
     * Compares ASN1Value.
     *
     * @param other another ASN1Value to compare with.
     * @return bool.
     */
    bool operator==(const ASN1Value& other) const
    {
        return mTagClass == other.mTagClass && mTagNumber == other.mTagNumber && mIsConstructed == other.mIsConstructed
            && mValue == other.mValue;
    }

    /**
     * Compares ASN1Value.
     *
     * @param other another ASN1Value to compare with.
     * @return bool.
     */
    bool operator!=(const ASN1Value& other) const { return !(*this == other); }
};

/**
 * Represents an ASN.1 AlgorithmIdentifier type.
 */
struct AlgorithmIdentifier {
    ObjectIdentifier mOID;
    ASN1Value        mParams;
};

/**
 * ASN.1 structure extension. RFC 580, section 4.2
 */
struct Extension {
    ObjectIdentifier                        mID;
    StaticArray<uint8_t, cASN1ExtValueSize> mValue;

    /**
     * Checks whether current object is equal the the given one.
     *
     * @param extension object to compare with.
     * @return bool.
     */
    bool operator==(const Extension& extension) const { return extension.mID == mID && extension.mValue == mValue; }
    /**
     * Checks whether current object is not equal the the given one.
     *
     * @param extension object to compare with.
     * @return bool.
     */
    bool operator!=(const Extension& extension) const { return !operator==(extension); }
};

/**
 * Converts input time to ASN1 GeneralizedTime string.
 *
 * @param time time.
 * @return RetWithError<StaticString<cTimeStrLen>>
 */
RetWithError<StaticString<cTimeStrLen>> ConvertTimeToASN1Str(const Time& time);

/**
 * ASN1 reader.
 */
class ASN1ReaderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ASN1ReaderItf() = default;

    /**
     * Called once per parsed TLV element.
     *
     * @param value ASN1 value.
     * @return Error.
     */
    virtual Error OnASN1Element(const ASN1Value& value) = 0;
};

/**
 * ASN.1 reader implementation that delegates parsing to a user-defined handler.
 */
template <typename Handler>
class ASN1Reader : public ASN1ReaderItf {
public:
    /**
     * Constructor.
     */
    explicit ASN1Reader(Handler&& handler)
        : mHandler(Move(handler))
    {
    }

    /**
     * Processes a single ASN.1 element.
     *
     * @param value ASN.1 element value to process.
     * @return Error.
     */
    Error OnASN1Element(const ASN1Value& value) override { return mHandler(value); }

private:
    Handler mHandler;
};

/**
 * Creates ASN1Reader instance based on the provided lambda.
 *
 * @param reader ASN1 reader.
 * @return ASN1Reader<Handler>.
 */
template <typename Reader>
ASN1Reader<Reader> MakeASN1Reader(Reader&& reader)
{
    return ASN1Reader<Reader>(Move(reader));
}

/**
 * Represents the result of an ASN.1 parsing operation.
 */
struct ASN1ParseResult {
    Error          mError;
    Array<uint8_t> mRemaining;

    /**
     * Constructor.
     */
    ASN1ParseResult() = default;

    /**
     * Constructor.
     *
     * @param err error object.
     * @param remaining not parsed content.
     */
    ASN1ParseResult(const Error& err, const Array<uint8_t>& remaining)
        : mError(err)
        , mRemaining(remaining)
    {
    }

    /**
     * Copy constructor.
     *
     * @param other source parse result object.
     */
    ASN1ParseResult(const ASN1ParseResult& other) { *this = other; }

    /**
     * Copy operator.
     *
     * @param other source parse result object.
     * @return ASN1ParseResult&.
     */
    ASN1ParseResult& operator=(const ASN1ParseResult& other)
    {
        mError = other.mError;
        mRemaining.Rebind(other.mRemaining);

        return *this;
    }

    /**
     * Compares ASN1 parse results.
     *
     * @param other another parse result to compare with.
     * @return bool.
     */
    bool operator==(const ASN1ParseResult& other) const
    {
        return mError == other.mError && mRemaining == other.mRemaining;
    }

    /**
     * Compares ASN1 parse results.
     *
     * @param other another parse result to compare with.
     * @return bool.
     */
    bool operator!=(const ASN1ParseResult& other) const { return !(*this == other); }
};

/**
 * Options to control the behavior of ASN.1 parsing.
 */
struct ASN1ParseOptions {
    /**
     * Indicates whether the field is optional.
     */
    bool mOptional = false;

    /**
     * Optional tag to match during parsing.
     */
    Optional<int> mTag;
};

/**
 * Interface for decoding ASN.1 structures.
 */
class ASN1DecoderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ASN1DecoderItf() = default;

    /**
     * Discards an ASN.1 tag-length and invokes reader for its content.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param asn1reader reader handles content of the structure.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadStruct(
        const Array<uint8_t>& data, const ASN1ParseOptions& opt, ASN1ReaderItf& asn1reader)
        = 0;

    /**
     * Reads an ASN.1 SET and invokes the reader for each element.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param asn1reader reader callback to handle each element.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadSet(const Array<uint8_t>& data, const ASN1ParseOptions& opt, ASN1ReaderItf& asn1reader)
        = 0;

    /**
     * Reads an ASN.1 SEQUENCE and invokes the reader for each element.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param asn1reader reader callback to handle each element.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadSequence(
        const Array<uint8_t>& data, const ASN1ParseOptions& opt, ASN1ReaderItf& asn1reader)
        = 0;

    /**
     * Reads an ASN.1 INTEGER value.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] value result integer.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadInteger(const Array<uint8_t>& data, const ASN1ParseOptions& opt, int& value) = 0;

    /**
     * Reads a large ASN.1 INTEGER (BigInt) as a byte array.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] result result integer bytes.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadBigInt(const Array<uint8_t>& data, const ASN1ParseOptions& opt, Array<uint8_t>& result)
        = 0;

    /**
     * Reads an ASN.1 Object Identifier (OID).
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] oid result OID.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadOID(const Array<uint8_t>& data, const ASN1ParseOptions& opt, ObjectIdentifier& oid) = 0;

    /**
     * Reads an ASN.1 AlgorithmIdentifier(AID).
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] aid result AID.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadAID(const Array<uint8_t>& data, const ASN1ParseOptions& opt, AlgorithmIdentifier& aid)
        = 0;

    /**
     * Reads an ASN.1 OCTET STRING.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] result result octet string.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadOctetString(
        const Array<uint8_t>& data, const ASN1ParseOptions& opt, Array<uint8_t>& result)
        = 0;

    /**
     * Returns a raw ASN.1 value.
     *
     * @param data input data buffer.
     * @param opt parse options.
     * @param[out] result parsed ASN1 value.
     * @return ASN1ParseResult.
     */
    virtual ASN1ParseResult ReadRawValue(const Array<uint8_t>& data, const ASN1ParseOptions& opt, ASN1Value& result)
        = 0;
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

/**
 * Crypto provider interface.
 */
class CryptoProviderItf : public x509::ProviderItf,
                          public HasherItf,
                          public RandomItf,
                          public UUIDItf,
                          public AESEncoderDecoderItf,
                          public asn1::ASN1DecoderItf {
public:
    /**
     * Destructor.
     */
    virtual ~CryptoProviderItf() = default;
};

} // namespace aos::crypto

#endif
