/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_AOS_COMMON_CRYPTO_CRYPTOHELPER_HPP_
#define AOS_AOS_COMMON_CRYPTO_CRYPTOHELPER_HPP_

#include <core/common/cloudprotocol/desiredstatus.hpp>
#include <core/common/crypto/cryptoutils.hpp>
#include <core/common/types/types.hpp>
#include <core/iam/certhandler/certprovider.hpp>

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
 * Certificate structure.
 */
struct CertificateInfo {
    /**
     * Certificate.
     */
    x509::Certificate mCertificate;

    /**
     * Certificate fingerprint.
     */
    StaticString<cloudprotocol::cCertFingerprintLen> mFingerprint;
};

/**
 * Signing context.
 */
struct SignContext {
    StaticArray<CertificateInfo, cMaxNumCertificates>                          mCerts;
    StaticArray<cloudprotocol::CertificateChainInfo, crypto::cCertChainsCount> mChains;
};

/**
 * CMS recipient identity info.
 */
struct RecipientID {
    StaticArray<uint8_t, cCertIssuerSize> mIssuer;
    StaticArray<uint8_t, cSerialNumSize>  mSerial;
};

/**
 * CMS transport information.
 */
struct TransRecipientInfo {
    int                                mVersion;
    RecipientID                        mRID;
    asn1::AlgorithmIdentifier          mKeyEncryptionAlgorithm;
    StaticArray<uint8_t, cCertDERSize> mEncryptedKey;
};

/**
 * CMS encrypted content info.
 */
struct EncryptedContentInfo {
    asn1::ObjectIdentifier                   mContentType;
    asn1::AlgorithmIdentifier                mContentEncryptionAlgorithm;
    StaticArray<uint8_t, cCloudMetadataSize> mEncryptedContent;
};

/**
 * CMS envelope data.
 */
struct EnvelopeData {
    int mVersion;
    // skip OriginatorInfo originatorInfo `asn1:"optional,implicit,tag:0"`
    StaticArray<TransRecipientInfo, cRecipientsInEnvelopeData> mRecipientInfos; // `asn1:"set"`
    EncryptedContentInfo                                       mEncryptedContent;
    // skip UnprotectedAttrs `asn1:"optional,implicit,tag:1,set"`
};

/**
 * CMS content info.
 */
struct ContentInfo {
    asn1::ObjectIdentifier mOID;
    EnvelopeData           mEnvelopeData;
};

/**
 * CryptoHelper interface.
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
    virtual Error Decrypt(
        const String& encryptedPath, const String& decryptedPath, const cloudprotocol::DecryptInfo& decryptionInfo)
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
    virtual Error ValidateSigns(const String& decryptedPath, const cloudprotocol::SignInfo& signs,
        const Array<cloudprotocol::CertificateChainInfo>& chains, const Array<cloudprotocol::CertificateInfo>& certs)
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

/**
 * CryptoHelper implementation.
 */
class CryptoHelper : public CryptoHelperItf {
public:
    /**
     * Constructor
     */
    CryptoHelper();

    /**
     * Initializes crypto helper.
     *
     * @param certProvider        certificate provider interface.
     * @param cryptoProvider      cryptographic provider interface.
     * @param certLoader          certificate loader interface.
     * @param serviceDiscoveryURL URL for the service discovery endpoint.
     * @param caCert              root certificate path.
     * @return Error.
     */
    Error Init(iam::certhandler::CertProviderItf& certProvider, CryptoProviderItf& cryptoProvider,
        CertLoaderItf& certLoader, const String& serviceDiscoveryURL, const String& caCert);

    /**
     * Retrieves available service discovery URLs.
     *
     * @param[out] urls result URLs.
     * @return Error.
     */
    Error GetServiceDiscoveryURLs(Array<StaticString<cURLLen>>& urls);

    /**
     * Decrypts a file using provided decryption information.
     *
     * @param encryptedPath   path to the encrypted file.
     * @param decryptedPath   path where the decrypted file will be written.
     * @param decryptionInfo  decryption information.
     * @return Error.
     */
    Error Decrypt(const String& encryptedPath, const String& decryptedPath,
        const cloudprotocol::DecryptInfo& decryptionInfo) override;

    /**
     * Validates digital signatures of a decrypted file against provided certificates and chains.
     *
     * @param decryptedPath path to the decrypted file to validate.
     * @param signs         signature information.
     * @param chains        certificate chains for validation.
     * @param certs         certificates used for validation.
     * @return Error.
     */
    Error ValidateSigns(const String& decryptedPath, const cloudprotocol::SignInfo& signs,
        const Array<cloudprotocol::CertificateChainInfo>& chains,
        const Array<cloudprotocol::CertificateInfo>&      certs) override;

    /**
     * Decrypts metadata containing in a binary buffer.
     *
     * @param[in]  input   encrypted metadata buffer.
     * @param[out] output  buffer where the decrypted metadata will be stored.
     * @return Error.
     */
    Error DecryptMetadata(const Array<uint8_t>& input, Array<uint8_t>& output) override;

private:
    static constexpr auto cMaxHashSize                 = cSHA2DigestSize;
    static constexpr auto cServiceDiscoveryDefaultPort = 9000;

    static constexpr auto cOnlineCert  = "online";
    static constexpr auto cOfflineCert = "offline";

    static constexpr auto cEnvelopedDataOid = "1.2.840.113549.1.7.3";
    static constexpr auto cRSAEncryptionOid = "1.2.840.113549.1.1.1";
    static constexpr auto cAES256CBCOid     = "2.16.840.1.101.3.4.1.42";

    static constexpr size_t cReadChunkSize = 1024;

    static constexpr auto cThreadHeapUsage = 2 * sizeof(iam::certhandler::CertInfo)
        + sizeof(StaticString<cCertSubjSize>) + sizeof(StaticArray<uint8_t, crypto::cCertPEMLen>) + sizeof(SignContext)
        + sizeof(x509::Certificate) + sizeof(StaticArray<uint8_t, cMaxHashSize>)
        + sizeof(StaticArray<x509::Certificate, cMaxNumCertificates>) + sizeof(StaticArray<uint8_t, cReadChunkSize>);

    RetWithError<SharedPtr<x509::CertificateChain>> GetOnlineCert();
    Error                                           SetDefaultServiceDiscoveryURL(Array<StaticString<cURLLen>>& urls);
    Error GetServiceDiscoveryFromExtensions(const x509::Certificate& cert, Array<StaticString<cURLLen>>& urls);
    Error GetServiceDiscoveryFromOrganization(const x509::Certificate& cert, Array<StaticString<cURLLen>>& urls);

    Error DecodeSymAlgNames(const String& algString, String& algName, String& modeName, String& paddingName);
    Error GetSymmetricAlgInfo(const String& algName, size_t& keySize, size_t& ivSize);
    Error CheckSessionKey(const String& symAlgName, const Array<uint8_t>& sessionIV, const Array<uint8_t>& sessionKey);
    Error DecodeFile(const String& encryptedFile, const String& decryptedFile, crypto::AESCipherItf& decoder);

    Error AddCertificates(const Array<cloudprotocol::CertificateInfo>& cert, SignContext& ctx);
    Error AddCertChains(const Array<cloudprotocol::CertificateChainInfo>& chains, SignContext& ctx);
    Error VerifySigns(const String& file, const cloudprotocol::SignInfo& signs, SignContext& signCtx);

    RetWithError<x509::Certificate*> GetCert(SignContext& signCtx, const String& fingerprint);
    Error GetSignCert(SignContext& signCtx, const String& chainName, x509::Certificate*& signCert,
        cloudprotocol::CertificateChainInfo*& chain);
    Error DecodeSignAlgNames(const String& algString, String& algName, String& hashName, String& paddingName);
    RetWithError<Hash> DecodeHash(const String& hashName);
    Error              CalcHashSum(const Hash& hash, const String& fileName, Array<uint8_t>& hashSum);
    Error              CreateIntermCertPool(
                     SignContext& signCtx, const cloudprotocol::CertificateChainInfo& chain, Array<x509::Certificate>& pool);

    Error UnmarshalCMS(const Array<uint8_t>& der, ContentInfo& content);
    Error ParseContentInfo(const Array<uint8_t>& data, ContentInfo& content);
    Error ParseEnvelopeData(const Array<uint8_t>& data, EnvelopeData& content);
    Error ParseRecipientInfo(const Array<uint8_t>& data, TransRecipientInfo& content);
    Error ParseRID(const Array<uint8_t>& data, RecipientID& content);
    Error ParseEncryptedContentInfo(const Array<uint8_t>& data, EncryptedContentInfo& content);
    Error GetKeyForEnvelope(const TransRecipientInfo& info, Array<uint8_t>& symmetricKey);
    Error DecryptCMSKey(const TransRecipientInfo& ktri, const PrivateKeyItf& privKey, Array<uint8_t>& symmetricKey);
    Error DecryptMessage(const EncryptedContentInfo& content, const Array<uint8_t>& symKey, Array<uint8_t>& message);
    Error DecodeMessage(AESCipherItf& decoder, const Array<uint8_t>& input, Array<uint8_t>& message);

    iam::certhandler::CertProviderItf* mCertProvider   = nullptr;
    CryptoProviderItf*                 mCryptoProvider = nullptr;
    CertLoaderItf*                     mCertLoader     = nullptr;

    StaticString<cURLLen>  mServiceDiscoveryURL;
    x509::CertificateChain mCACerts;

    Semaphore                                                  mSemaphore;
    StaticAllocator<cMaxNumConcurrentItems * cThreadHeapUsage> mAllocator;
};

} // namespace aos::crypto

#endif
