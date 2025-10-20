/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_ASN1_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_ASN1_HPP_

#include <core/common/tools/enum.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/time.hpp>

namespace aos::crypto::asn1 {

/**
 * Maximum length of numeric string representing ASN.1 Object Identifier.
 */
constexpr auto cASN1ObjIdLen = AOS_CONFIG_CRYPTO_ASN1_OBJECT_ID_LEN;

/**
 * Maximum size of a certificate ASN.1 Extension Value.
 */
constexpr auto cASN1ExtValueSize = AOS_CONFIG_CRYPTO_ASN1_EXTENSION_VALUE_SIZE;

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

} // namespace aos::crypto::asn1

#endif
