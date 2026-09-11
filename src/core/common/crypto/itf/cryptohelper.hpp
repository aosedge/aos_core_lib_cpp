/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_CRYPTOHELPER_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_CRYPTOHELPER_HPP_

#include <core/common/tools/string.hpp>

#include "x509.hpp"

namespace aos::crypto {

/**
 * Number of recipient info in envelope data.
 */
constexpr auto cRecipientsInEnvelopeData = AOS_CONFIG_CRYPTO_RECIPIENTS_IN_ENVELOPEDATA;

/**
 * Maximum size for cloud metadata.
 */
constexpr auto cCloudMetadataSize = AOS_CONFIG_CRYPTO_ENCRYPT_METADATA;

/**
 * Certificate fingerprint len.
 */
constexpr auto cCertFingerprintLen = AOS_CONFIG_CRYPTO_CERT_FINGERPRINT_LEN;

/**
 * Chain name len.
 */
constexpr auto cChainNameLen = AOS_CONFIG_CRYPTO_CHAIN_NAME_LEN;

/**
 * Algorithm len.
 */
constexpr auto cAlgLen = AOS_CONFIG_CRYPTO_ALG_LEN;

/**
 * IV size.
 */
constexpr auto cIVSize = AOS_CONFIG_CRYPTO_IV_SIZE;

/**
 * Key size.
 */
constexpr auto cKeySize = AOS_CONFIG_CRYPTO_KEY_SIZE;

/**
 * OCSP value len.
 */
constexpr auto cOCSPValueLen = AOS_CONFIG_CRYPTO_OCSP_VALUE_LEN;

/**
 * OCSP values count.
 */
constexpr auto cOCSPValuesCount = AOS_CONFIG_CRYPTO_OCSP_VALUES_COUNT;

/**
 * Certificate info.
 */
struct CertificateInfo {
    StaticArray<uint8_t, cCertDERSize> mCertificate;
    StaticString<cCertFingerprintLen>  mFingerprint;

    /**
     * Compares certificate info.
     *
     * @param rhs certificate info to compare with.
     * @return bool.
     */
    friend bool operator==(const CertificateInfo& lhs, const CertificateInfo& rhs)
    {
        return lhs.mCertificate == rhs.mCertificate && lhs.mFingerprint == rhs.mFingerprint;
    };

    /**
     * Compares certificate info.
     *
     * @param rhs certificate info to compare with.
     * @return bool.
     */
    friend bool operator!=(const CertificateInfo& lhs, const CertificateInfo& rhs) { return !(lhs == rhs); };
};

using CertificateInfoArray = StaticArray<CertificateInfo, crypto::cMaxNumCertificates>;

/**
 * Certificate chain info.
 */
struct CertificateChainInfo {
    StaticString<cChainNameLen>                                            mName;
    StaticArray<StaticString<cCertFingerprintLen>, crypto::cCertChainSize> mFingerprints;

    /**
     * Compares certificate chain info.
     *
     * @param rhs certificate chain info to compare with.
     * @return bool.
     */
    friend bool operator==(const CertificateChainInfo& lhs, const CertificateChainInfo& rhs)
    {
        return lhs.mName == rhs.mName && lhs.mFingerprints == rhs.mFingerprints;
    };

    /**
     * Compares certificate chain info.
     *
     * @param rhs certificate chain info to compare with.
     * @return bool.
     */
    friend bool operator!=(const CertificateChainInfo& lhs, const CertificateChainInfo& rhs) { return !(lhs == rhs); };
};

using CertificateChainInfoArray = StaticArray<CertificateChainInfo, cCertChainsCount>;

/**
 * Decryption info.
 */
struct DecryptInfo {
    StaticString<cAlgLen>          mBlockAlg;
    StaticArray<uint8_t, cIVSize>  mBlockIV;
    StaticArray<uint8_t, cKeySize> mBlockKey;

    /**
     * Compares decryption info.
     *
     * @param rhs decryption info to compare with.
     * @return bool.
     */
    friend bool operator==(const DecryptInfo& lhs, const DecryptInfo& rhs)
    {
        return lhs.mBlockAlg == rhs.mBlockAlg && lhs.mBlockIV == rhs.mBlockIV && lhs.mBlockKey == rhs.mBlockKey;
    };

    /**
     * Compares decryption info.
     *
     * @param rhs decryption info to compare with.
     * @return bool.
     */
    friend bool operator!=(const DecryptInfo& lhs, const DecryptInfo& rhs) { return !(lhs == rhs); };
};

/**
 * Sign info.
 */
struct SignInfo {
    StaticString<cChainNameLen>                                mChainName;
    StaticString<cAlgLen>                                      mAlg;
    StaticArray<uint8_t, crypto::cSignatureSize>               mValue;
    Time                                                       mTrustedTimestamp {};
    StaticArray<StaticString<cOCSPValueLen>, cOCSPValuesCount> mOCSPValues;
    /**
     * Compares sign info.
     *
     * @param rhs sign info to compare with.
     * @return bool.
     */
    friend bool operator==(const SignInfo& lhs, const SignInfo& rhs)
    {
        return lhs.mChainName == rhs.mChainName && lhs.mAlg == rhs.mAlg && lhs.mValue == rhs.mValue
            && lhs.mTrustedTimestamp == rhs.mTrustedTimestamp && lhs.mOCSPValues == rhs.mOCSPValues;
    };

    /**
     * Compares sign info.
     *
     * @param rhs sign info to compare with.
     * @return bool.
     */
    friend bool operator!=(const SignInfo& lhs, const SignInfo& rhs) { return !(lhs == rhs); };
};

/**
 * CryptoHelper interface for decrypting and validating cloud data.
 */
class CryptoHelperItf {
public:
    /**
     * Destructor.
     */
    virtual ~CryptoHelperItf() = default;

    /**
     * Decrypts a file using provided decryption information.
     *
     * @param encryptedPath   path to the encrypted file.
     * @param decryptedPath   path where the decrypted file will be written.
     * @param decryptionInfo  decryption information.
     * @return Error.
     */
    virtual Error Decrypt(const String& encryptedPath, const String& decryptedPath, const DecryptInfo& decryptionInfo)
        = 0;

    /**
     * Validates digital signatures of a decrypted file against provided certificates and chains.
     *
     * @param decryptedPath path to the decrypted file to validate.
     * @param signs         signature information.
     * @param chains        certificate chains for validation.
     * @param certs         certificates used for validation.
     * @return Error.
     */
    virtual Error ValidateSigns(const String& decryptedPath, const SignInfo& signs,
        const Array<CertificateChainInfo>& chains, const Array<CertificateInfo>& certs)
        = 0;

    /**
     * Decrypts metadata containing in a binary buffer.
     *
     * @param[in]  input   encrypted metadata buffer.
     * @param[out] output  buffer where the decrypted metadata will be stored.
     * @return Error.
     */
    virtual Error DecryptMetadata(const Array<uint8_t>& input, Array<uint8_t>& output) = 0;
};

} // namespace aos::crypto

#endif
