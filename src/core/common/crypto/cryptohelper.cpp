/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>

#include "cryptoutils.hpp"

namespace aos::crypto {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

/**
 * Decrypts a file with an AES decoder.
 *
 * The plaintext is staged in a newly created, unpredictably named, owner-only file, which becomes the decrypted file
 * only after the whole content is processed: authenticated modes (GCM) return plaintext before the authentication tag
 * is verified, and a failed decryption must not leave partial output behind. The decrypted file keeps the staged
 * file's owner-only permission; a caller that needs a different policy sets it after Decrypt returns. The directory
 * of the decrypted file must be trusted: only the last component of the staged path is protected from symlinks.
 */
class FileDecoder {
public:
    /**
     * Constructor.
     *
     * @param allocator allocator to use for temporary objects.
     * @param random    random generator used to name the staged file.
     * @param decoder   AES decoder.
     * @param tagSize   size of the authentication tag stored at the end of the file, 0 if the mode has no tag.
     */
    FileDecoder(AllocatorItf& allocator, RandomItf& random, AESCipherItf& decoder, size_t tagSize)
        : mAllocator(allocator)
        , mRandom(random)
        , mDecoder(decoder)
        , mTagSize(tagSize)
    {
    }

    /**
     * Decrypts a file.
     *
     * @param encryptedFile path to the encrypted file.
     * @param decryptedFile path where the decrypted file will be written.
     * @return Error.
     */
    Error Decode(const String& encryptedFile, const String& decryptedFile)
    {
        mBuffers = MakeUnique<Buffers>(&mAllocator);
        if (!mBuffers) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        // errors of file operations have no location yet: they are wrapped once here
        return AOS_ERROR_WRAP(DecodeFile(encryptedFile, decryptedFile));
    }

private:
    /**
     * Buffers used to process a file chunk by chunk.
     */
    struct Buffers {
        StaticArray<uint8_t, cFileChunkSize>                             mRead;
        StaticArray<uint8_t, cFileChunkSize + AESCipherItf::cGCMTagSize> mIn;
        StaticArray<uint8_t, cFileChunkSize>                             mOut;
    };

    /**
     * Decrypts a file into a staged file and moves the staged file to its final path on success.
     *
     * @param encryptedFile path to the encrypted file.
     * @param decryptedFile path where the decrypted file will be written.
     * @return Error.
     */
    Error DecodeFile(const String& encryptedFile, const String& decryptedFile)
    {
        constexpr size_t cStagedFileSuffixSize = 16;
        // owner-only: the staged file holds unauthenticated plaintext, and this stays the decrypted file's
        // permission too, since renaming it into place doesn't change it. A caller that wants a different policy
        // sets it after Decrypt returns.
        constexpr uint32_t cStagedFilePerm = 0600;

        StaticString<cStagedFileSuffixSize * 2> suffix;

        if (auto err = GenerateRandomString<cStagedFileSuffixSize>(suffix, mRandom); !err.IsNone()) {
            return err;
        }

        StaticString<cFilePathLen> stagedFile;

        if (auto err = stagedFile.Format("%s.%s.tmp", decryptedFile.CStr(), suffix.CStr()); !err.IsNone()) {
            return err;
        }

        if (auto err = mInputFile.Open(encryptedFile, fs::File::Mode::Read); !err.IsNone()) {
            return err;
        }

        if (auto err = mOutputFile.Open(stagedFile, fs::File::Mode::WriteNew, cStagedFilePerm); !err.IsNone()) {
            return err;
        }

        auto err = Process();

        if (err.IsNone()) {
            err = mOutputFile.Close();
        }

        if (err.IsNone()) {
            err = fs::Rename(stagedFile, decryptedFile);
        }

        if (!err.IsNone()) {
            (void)mOutputFile.Close();

            return DiscardStagedFile(stagedFile, err);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Reads the encrypted file chunk by chunk, decrypts it and writes the result to the staged file.
     *
     * @return Error.
     */
    Error Process()
    {
        auto& [readBlock, inBlock, outBlock] = *mBuffers;

        // The authentication tag can only be recognized once the end of the file is reached, so the last tag-size
        // bytes read are always held back.
        StaticArray<uint8_t, AESCipherItf::cGCMTagSize> tail;

        while (true) {
            if (auto err = mInputFile.ReadBlock(readBlock); !err.IsNone() && !err.Is(ErrorEnum::eEOF)) {
                return err;
            }

            if (readBlock.IsEmpty()) {
                break;
            }

            inBlock.Clear();
            (void)inBlock.Append(tail).Append(readBlock);

            if (inBlock.Size() <= mTagSize) {
                tail = inBlock;

                continue;
            }

            const auto dataSize = inBlock.Size() - mTagSize;

            if (auto err = mDecoder.DecryptBlock(Array<uint8_t>(inBlock.Get(), dataSize), outBlock); !err.IsNone()) {
                return err;
            }

            if (auto err = mOutputFile.WriteBlock(outBlock); !err.IsNone()) {
                return err;
            }

            tail = Array<uint8_t>(inBlock.Get() + dataSize, mTagSize);
        }

        if (tail.Size() != mTagSize) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "file is too short to contain an auth tag"));
        }

        if (mTagSize != 0) {
            if (auto err = mDecoder.SetTag(tail); !err.IsNone()) {
                return err;
            }
        }

        // fails if the authentication tag doesn't match
        if (auto err = mDecoder.Finalize(outBlock); !err.IsNone()) {
            return err;
        }

        return mOutputFile.WriteBlock(outBlock);
    }

    /**
     * Removes the staged file on a best-effort basis, so unauthenticated plaintext isn't left behind. If it can't be
     * removed, that failure is reported instead of being hidden behind the original error, but the staged file can
     * still remain.
     *
     * @param stagedFile path to the staged file.
     * @param cause      error that caused the staged file to be discarded.
     * @return Error.
     */
    Error DiscardStagedFile(const String& stagedFile, const Error& cause) const
    {
        if (auto err = fs::Remove(stagedFile); !err.IsNone()) {
            LOG_ERR() << "Can't remove staged file" << Log::Field("path", stagedFile) << Log::Field(err)
                      << Log::Field("cause", cause);

            return Error(err, "can't remove unauthenticated data");
        }

        return cause;
    }

    AllocatorItf&      mAllocator;
    RandomItf&         mRandom;
    AESCipherItf&      mDecoder;
    size_t             mTagSize;
    UniquePtr<Buffers> mBuffers;
    fs::File           mInputFile;
    fs::File           mOutputFile;
};

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

CryptoHelper::CryptoHelper()
    : mSemaphore(cMaxNumConcurrentItems)
{
}

Error CryptoHelper::Init(AllocatorItf& allocator, iamclient::CertProviderItf& certProvider,
    CryptoProviderItf& cryptoProvider, CertLoaderItf& certLoader, const String& serviceDiscoveryURL,
    const String& caCert)
{
    mAllocator           = &allocator;
    mCertProvider        = &certProvider;
    mCryptoProvider      = &cryptoProvider;
    mCertLoader          = &certLoader;
    mServiceDiscoveryURL = serviceDiscoveryURL;

    auto caCertsPEM = MakeUnique<StaticString<cCertPEMLen>>(mAllocator);
    if (!caCertsPEM) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = fs::ReadFileToString(caCert, *caCertsPEM); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mCryptoProvider->PEMToX509Certs(*caCertsPEM, mCACerts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& cert : mCACerts) {
        if (auto err = ValidateCACert(cert); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::GetServiceDiscoveryURLs(Array<StaticString<cURLLen>>& urls)
{
    LockGuard lock {mSemaphore};

    const auto [certs, certErr] = GetOnlineCert();
    if (!certErr.IsNone()) {
        return SetDefaultServiceDiscoveryURL(urls);
    }

    if (auto err = GetServiceDiscoveryFromExtensions((*certs)[0], urls); !err.IsNone()) {
        if (!err.Is(ErrorEnum::eNotFound)) {
            LOG_WRN() << "Can't get service discovery url from extensions" << Log::Field(err);

            return err;
        }
    } else {
        return ErrorEnum::eNone;
    }

    if (auto err = GetServiceDiscoveryFromOrganization((*certs)[0], urls); !err.IsNone()) {
        if (!err.Is(ErrorEnum::eNotFound)) {
            LOG_WRN() << "Can't get service discovery url from organization" << Log::Field(err);

            return err;
        }
    } else {
        return ErrorEnum::eNone;
    }

    return SetDefaultServiceDiscoveryURL(urls);
}

Error CryptoHelper::Decrypt(const String& encryptedFile, const String& decryptedFile, const DecryptInfo& decryptInfo)
{
    LockGuard lock {mSemaphore};

    const auto& symmetricAlgName = decryptInfo.mBlockAlg;
    const auto& sessionKey       = decryptInfo.mBlockKey;
    const auto& sessionIV        = decryptInfo.mBlockIV;

    StaticString<cAlgLen> algName;
    StaticString<cAlgLen> modeName;
    StaticString<cAlgLen> paddingName;

    if (auto err = DecodeSymAlgNames(symmetricAlgName, algName, modeName, paddingName); !err.IsNone()) {
        return err;
    }

    auto [decoder, createDecoderErr] = mCryptoProvider->CreateAESDecoder(modeName, sessionKey, sessionIV);
    if (!createDecoderErr.IsNone()) {
        return AOS_ERROR_WRAP(createDecoderErr);
    }

    if (auto checkErr = CheckSessionKey(algName, modeName, sessionIV, sessionKey); !checkErr.IsNone()) {
        return AOS_ERROR_WRAP(checkErr);
    }

    // GCM is authenticated and has no padding: the padding part of the algorithm name is not used.
    const size_t tagSize = modeName == "GCM" ? AESCipherItf::cGCMTagSize : 0;

    return FileDecoder(*mAllocator, *mCryptoProvider, *decoder, tagSize).Decode(encryptedFile, decryptedFile);
}

Error CryptoHelper::ValidateSigns(const String& decryptedPath, const SignInfo& signs,
    const Array<CertificateChainInfo>& chains, const Array<CertificateInfo>& certs)
{
    LockGuard lock {mSemaphore};

    auto signCtx = MakeUnique<SignContext>(mAllocator);
    if (!signCtx) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = AddCertificates(certs, *signCtx); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = AddCertChains(chains, *signCtx); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = VerifySigns(decryptedPath, signs, *signCtx); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::DecryptMetadata(const Array<uint8_t>& input, Array<uint8_t>& output)
{
    LockGuard lock {mSemaphore};

    auto contentInfo = MakeUnique<ContentInfo>(mAllocator);
    if (!contentInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto symKey = MakeUnique<StaticArray<uint8_t, cPrivKeyPEMLen>>(mAllocator);
    if (!symKey) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = UnmarshalCMS(input, *contentInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& recipient : contentInfo->mEnvelopeData.mRecipientInfos) {
        if (auto err = GetKeyForEnvelope(recipient, *symKey); !err.IsNone()) {
            LOG_WRN() << "Can't get key for envelope" << Log::Field(err);
            continue;
        }

        if (auto err = DecryptMessage(contentInfo->mEnvelopeData.mEncryptedContent, *symKey, output); !err.IsNone()) {
            LOG_WRN() << "Can't decrypt message" << Log::Field(err);
            continue;
        }

        return ErrorEnum::eNone;
    }

    return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "can't decrypt metadata"));
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

RetWithError<SharedPtr<x509::CertificateChain>> CryptoHelper::GetOnlineCert()
{
    auto certInfo = MakeUnique<CertInfo>(mAllocator);
    if (!certInfo) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eNoMemory)};
    }

    if (auto err = mCertProvider->GetCert(cOnlineCert, {}, {}, *certInfo); !err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    auto [chain, err] = mCertLoader->LoadCertsChainByURL(certInfo->mCertURL);
    if (!err.IsNone()) {
        return {chain, AOS_ERROR_WRAP(err)};
    }

    return chain;
}

Error CryptoHelper::SetDefaultServiceDiscoveryURL(Array<StaticString<cURLLen>>& urls) const
{
    if (urls.IsEmpty()) {
        LOG_WRN() << "Service discovery URL can't be found in certificate and will be used from config";

        return AOS_ERROR_WRAP(urls.PushBack(mServiceDiscoveryURL));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::GetServiceDiscoveryFromExtensions(
    const x509::Certificate& cert, Array<StaticString<cURLLen>>& urls) const
{
    if (auto err = urls.Insert(urls.begin(), cert.mIssuerURLs.begin(), cert.mIssuerURLs.end()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (urls.IsEmpty()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::GetServiceDiscoveryFromOrganization(
    const x509::Certificate& cert, Array<StaticString<cURLLen>>& urls)
{
    auto subject = MakeUnique<StaticString<cCertSubjSize>>(mAllocator);
    if (!subject) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mCryptoProvider->ASN1DecodeDN(cert.mSubject, *subject); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    const String orgKey    = "O=";
    auto [orgPos, findErr] = subject->FindSubstr(0, orgKey);
    if (!findErr.IsNone()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    // Extract value after "O=" up to the next comma or end
    auto valueStart    = orgPos + orgKey.Size();
    auto [valueEnd, _] = subject->FindSubstr(valueStart, ",");

    auto orgName = MakeUnique<StaticString<cURLLen>>(mAllocator);
    if (!orgName) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto url = MakeUnique<StaticString<cURLLen>>(mAllocator);
    if (!url) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = orgName->Insert(orgName->begin(), subject->begin() + valueStart, subject->begin() + valueEnd);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (orgName->IsEmpty()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = url->Format("https://%s:%d", orgName->CStr(), cServiceDiscoveryDefaultPort); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = urls.EmplaceBack(*url); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::DecodeSymAlgNames(
    const String& algString, String& algName, String& modeName, String& paddingName) const
{
    // alg string example: AES128/CBC/PKCS7PADDING
    static constexpr auto cAlgoParts = 3;

    StaticArray<StaticString<cAlgLen>, cAlgoParts> parts;

    if (auto err = algString.Split(parts, '/'); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (parts.Size() >= 1) {
        if (auto err = algName.Assign(parts[0].ToUpper()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        algName.Clear();
    }

    if (parts.Size() >= 2) {
        if (auto err = modeName.Assign(parts[1].ToUpper()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        if (auto err = modeName.Assign("CBC"); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (parts.Size() >= 3) {
        if (auto err = paddingName.Assign(parts[2].ToUpper()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        if (auto err = paddingName.Assign("PKCS7PADDING"); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::GetSymmetricAlgInfo(
    const String& algName, const String& modeName, size_t& keySize, size_t& ivSize) const
{
    if (modeName == "CBC") {
        ivSize = AESCipherItf::cBlockSize;
    } else if (modeName == "GCM") {
        ivSize = AESCipherItf::cGCMIVSize;
    } else {
        return ErrorEnum::eNotSupported;
    }

    if (algName == "AES128") {
        keySize = 16;

        return ErrorEnum::eNone;
    } else if (algName == "AES192") {
        keySize = 24;

        return ErrorEnum::eNone;
    } else if (algName == "AES256") {
        keySize = 32;

        return ErrorEnum::eNone;
    } else {
    }

    return ErrorEnum::eNotSupported;
}

Error CryptoHelper::CheckSessionKey(const String& symAlgName, const String& modeName, const Array<uint8_t>& sessionIV,
    const Array<uint8_t>& sessionKey) const
{
    size_t keySize = 0;
    size_t ivSize  = 0;

    if (auto err = GetSymmetricAlgInfo(symAlgName, modeName, keySize, ivSize); !err.IsNone()) {
        return err;
    }

    if (ivSize != sessionIV.Size()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "invalid IV length"));
    }

    if (keySize != sessionKey.Size()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "invalid symmetric key"));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::AddCertificates(const Array<CertificateInfo>& certs, SignContext& ctx)
{
    ctx.mCerts.Clear();

    for (const auto& certInfo : certs) {
        StaticString<cCertFingerprintLen> fingerprint = certInfo.mFingerprint;

        (void)fingerprint.ToUpper();

        if (auto iter = ctx.mCerts.FindIf(
                [&fingerprint](const X509CertificateInfo& certInfo) { return certInfo.mFingerprint == fingerprint; });
            iter != ctx.mCerts.end()) {
            continue;
        }

        auto cert = MakeUnique<x509::Certificate>(mAllocator);
        if (!cert) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        if (auto err = mCryptoProvider->DERToX509Cert(certInfo.mCertificate, *cert); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = ctx.mCerts.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ctx.mCerts.Back().mCertificate = *cert;
        ctx.mCerts.Back().mFingerprint = fingerprint;
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::AddCertChains(const Array<CertificateChainInfo>& chains, SignContext& ctx) const
{
    ctx.mChains.Clear();

    for (const auto& chainInfo : chains) {
        if (auto iter = ctx.mChains.FindIf(
                [&chainInfo](const CertificateChainInfo& item) { return item.mName == chainInfo.mName; });
            iter != ctx.mChains.end()) {
            continue;
        }

        if (auto err = ctx.mChains.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ctx.mChains.Back().mName         = chainInfo.mName;
        ctx.mChains.Back().mFingerprints = chainInfo.mFingerprints;

        for (auto& fingerprint : ctx.mChains.Back().mFingerprints) {
            (void)fingerprint.ToUpper();
        }
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::VerifySigns(const String& file, const SignInfo& signs, SignContext& signCtx)
{
    x509::Certificate*    signCert = nullptr;
    CertificateChainInfo* chain    = nullptr;

    if (auto err = GetSignCert(signCtx, signs.mChainName, signCert, chain); !err.IsNone()) {
        return err;
    }

    StaticString<cAlgLen> algName;
    StaticString<cAlgLen> hashName;
    StaticString<cAlgLen> paddingName;

    if (auto err = DecodeSignAlgNames(signs.mAlg, algName, hashName, paddingName); !err.IsNone()) {
        return err;
    }

    auto [hash, hashErr] = DecodeHash(hashName);
    if (!hashErr.IsNone()) {
        return hashErr;
    }

    // Verify sign
    auto hashSum = MakeUnique<StaticArray<uint8_t, cMaxHashSize>>(mAllocator);
    if (!hashSum) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = CalculateFileHash(file, hash, *mCryptoProvider, *hashSum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (algName != "RSA") {
        return AOS_ERROR_WRAP(ErrorEnum::eNotSupported);
    }

    x509::Padding padding;
    if (paddingName == "PKCS1V1_5") {
        padding = x509::PaddingEnum::ePKCS1v1_5;
    } else if (paddingName == "PSS") {
        padding = x509::PaddingEnum::ePSS;
    } else {
        AOS_ERROR_WRAP(Error(ErrorEnum::eNotSupported, "unknown padding for RSA"));
    }

    if (auto err = mCryptoProvider->Verify(signCert->mPublicKey, // NOSONAR cpp:S2259 - non-null on success
            hash, padding, *hashSum, signs.mValue);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // Verify certs
    auto intermCertPool = MakeUnique<StaticArray<x509::Certificate, cMaxNumCertificates>>(mAllocator);
    if (!intermCertPool) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = CreateIntermCertPool(signCtx, *chain, *intermCertPool); !err.IsNone()) {
        return err;
    }

    x509::VerifyOptions options;

    options.mCurrentTime = signs.mTrustedTimestamp;
    // Assume any key usages.

    if (auto err = mCryptoProvider->Verify(mCACerts, *intermCertPool, options, *signCert); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<x509::Certificate*> CryptoHelper::GetCert(SignContext& signCtx, const String& fingerprint) const
{
    auto iter = signCtx.mCerts.FindIf(
        [&fingerprint](const X509CertificateInfo& info) { return info.mFingerprint == fingerprint; });

    if (iter == signCtx.mCerts.end()) {
        return {nullptr, ErrorEnum::eNotFound};
    }

    return &iter->mCertificate;
}

Error CryptoHelper::GetSignCert(
    SignContext& signCtx, const String& chainName, x509::Certificate*& signCert, CertificateChainInfo*& chain) const
{
    auto chainIt
        = signCtx.mChains.FindIf([&chainName](const CertificateChainInfo& chain) { return chain.mName == chainName; });

    if (chainIt == signCtx.mChains.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (chainIt->mName.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "bad chain name"));
    }

    if (chainIt->mFingerprints.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "chain has no fingerprints"));
    }

    auto [cert, err] = GetCert(signCtx, chainIt->mFingerprints[0]);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "signing certificate is absent"));
    }

    signCert = cert;
    chain    = chainIt;

    return ErrorEnum::eNone;
}

Error CryptoHelper::DecodeSignAlgNames(
    const String& algString, String& algName, String& hashName, String& paddingName) const
{
    // alg string example: RSA/SHA256/PKCS1v1_5 or RSA/SHA256
    static constexpr auto cAlgoParts = 3;

    StaticArray<StaticString<cAlgLen>, cAlgoParts> parts;

    if (auto err = algString.Split(parts, '/'); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (parts.Size() >= 1) {
        if (auto err = algName.Assign(parts[0].ToUpper()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        algName.Clear();
    }

    if (parts.Size() >= 2) {
        if (auto err = hashName.Assign(parts[1].ToUpper()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        if (auto err = hashName.Assign("SHA256"); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (parts.Size() >= 3) {
        if (auto err = paddingName.Assign(parts[2].ToUpper()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        if (auto err = paddingName.Assign("PKCS1v1_5"); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

RetWithError<Hash> CryptoHelper::DecodeHash(const String& hashName) const
{
    StaticString<cAlgLen> upperHash = hashName;

    (void)upperHash.ToUpper();

    if (upperHash == "SHA256") {
        return {HashEnum::eSHA256, ErrorEnum::eNone};
    } else if (upperHash == "SHA384") {
        return {HashEnum::eSHA384, ErrorEnum::eNone};
    } else if (upperHash == "SHA512") {
        return {HashEnum::eSHA512, ErrorEnum::eNone};
    } else if (upperHash == "SHA512/224") {
        return {HashEnum::eSHA512_224, ErrorEnum::eNone};
    } else if (upperHash == "SHA512/256") {
        return {HashEnum::eSHA512_256, ErrorEnum::eNone};
    } else {
        return {{}, AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "unsupported hashing algorithm"))};
    }
}

Error CryptoHelper::CreateIntermCertPool(
    SignContext& signCtx, const CertificateChainInfo& chain, Array<x509::Certificate>& pool) const
{
    pool.Clear();

    for (size_t i = 1; i < chain.mFingerprints.Size(); i++) {
        if (auto err = pool.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto [cert, getErr] = GetCert(signCtx, chain.mFingerprints[i]);
        if (!getErr.IsNone()) {
            return AOS_ERROR_WRAP(getErr);
        }

        pool.Back() = *cert;
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::UnmarshalCMS(const Array<uint8_t>& der, ContentInfo& content)
{
    auto contentInfoParser = asn1::MakeASN1Reader(
        [this, &content](const asn1::ASN1Value& value) { return ParseContentInfo(value.mValue, content); });

    auto [err, remaining] = mCryptoProvider->ReadStruct(der, {}, contentInfoParser);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!remaining.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "remaining data found"));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::ParseContentInfo(const Array<uint8_t>& data, ContentInfo& content)
{
    auto parseResult = mCryptoProvider->ReadOID(data, {}, content.mOID);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    if (content.mOID != cEnvelopedDataOid) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "unknown OID in ContentInfo"));
    }

    auto envelopeDataParser = asn1::MakeASN1Reader([this, &content](const asn1::ASN1Value& value) {
        return ParseEnvelopeData(value.mValue, content.mEnvelopeData);
    });

    auto skipExplicit = asn1::MakeASN1Reader([this, &content, &envelopeDataParser](const asn1::ASN1Value& value) {
        auto [err, remaining] = mCryptoProvider->ReadStruct(value.mValue, {}, envelopeDataParser);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (!remaining.IsEmpty()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "remaining data found"));
        }

        return Error();
    });

    auto [err, remaining] = mCryptoProvider->ReadStruct(parseResult.mRemaining, {false, 0}, skipExplicit);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!remaining.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "remaining data found"));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::ParseEnvelopeData(const Array<uint8_t>& data, EnvelopeData& envelopeData)
{
    // Parse Version
    auto parseResult = mCryptoProvider->ReadInteger(data, {}, envelopeData.mVersion);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    // Skip OriginatorInfo
    auto skipField = asn1::MakeASN1Reader([this, &envelopeData](const asn1::ASN1Value& value) {
        (void)value;

        return ErrorEnum::eNone;
    });

    parseResult = mCryptoProvider->ReadStruct(parseResult.mRemaining, {true, 0}, skipField); // optional, ignore error
    if (!parseResult.mError.IsNone() && !parseResult.mError.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    // parse RecipientInfos
    auto parseRI = asn1::MakeASN1Reader([this, &envelopeData](const asn1::ASN1Value& value) {
        if (value.mTagNumber != 16) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "invalid tag"));
        }

        if (auto err = envelopeData.mRecipientInfos.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ParseRecipientInfo(value.mValue, envelopeData.mRecipientInfos.Back());
    });

    parseResult = mCryptoProvider->ReadSet(parseResult.mRemaining, {}, parseRI);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    // parse EncryptedContentInfo
    auto parseEncContentInfo = asn1::MakeASN1Reader([this, &envelopeData](const asn1::ASN1Value& value) {
        return ParseEncryptedContentInfo(value.mValue, envelopeData.mEncryptedContent);
    });

    parseResult = mCryptoProvider->ReadStruct(parseResult.mRemaining, {}, parseEncContentInfo);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    // skip UnprotectedAttrs
    parseResult = mCryptoProvider->ReadSet(parseResult.mRemaining, {true, 1}, skipField);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    if (!parseResult.mRemaining.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "remaining data found"));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::ParseRecipientInfo(const Array<uint8_t>& data, TransRecipientInfo& content)
{
    // Parse Version
    auto parseResult = mCryptoProvider->ReadInteger(data, {}, content.mVersion);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    // Parse RID
    auto parseRID = asn1::MakeASN1Reader(
        [this, &content](const asn1::ASN1Value& value) { return ParseRID(value.mValue, content.mRID); });

    parseResult = mCryptoProvider->ReadStruct(parseResult.mRemaining, {}, parseRID);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    // Parse KeyEncryptionAlgorithm
    parseResult = mCryptoProvider->ReadAID(parseResult.mRemaining, {}, content.mKeyEncryptionAlgorithm);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    // Parse EncryptedKey
    parseResult = mCryptoProvider->ReadOctetString(parseResult.mRemaining, {}, content.mEncryptedKey);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    if (!parseResult.mRemaining.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "remaining data found"));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::ParseRID(const Array<uint8_t>& data, RecipientID& content)
{
    asn1::ASN1Value issuer;

    auto parseResult = mCryptoProvider->ReadRawValue(data, {}, issuer);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    if (auto err = content.mIssuer.Assign(issuer.mValue); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    parseResult = mCryptoProvider->ReadBigInt(parseResult.mRemaining, {}, content.mSerial);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    if (!parseResult.mRemaining.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "remaining data found"));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::ParseEncryptedContentInfo(const Array<uint8_t>& data, EncryptedContentInfo& content)
{
    auto parseResult = mCryptoProvider->ReadOID(data, {}, content.mContentType);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    parseResult = mCryptoProvider->ReadAID(parseResult.mRemaining, {}, content.mContentEncryptionAlgorithm);
    if (!parseResult.mError.IsNone()) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    // parse EncryptedContent `asn1:"optional,implicit,tag:0"`
    // OCTET STRING with custom tags are not supported
    asn1::ASN1Value encContent;

    parseResult = mCryptoProvider->ReadRawValue(parseResult.mRemaining, {true, 0}, encContent);
    if (!parseResult.mError.IsNone() && !parseResult.mError.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(parseResult.mError);
    }

    if (!parseResult.mRemaining.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "remaining data found"));
    }

    if (!parseResult.mError.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(content.mEncryptedContent.Assign(encContent.mValue));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::GetKeyForEnvelope(const TransRecipientInfo& info, Array<uint8_t>& symmetricKey)
{
    auto certInfo = MakeUnique<CertInfo>(mAllocator);
    if (!certInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mCertProvider->GetCert(cOfflineCert, info.mRID.mIssuer, info.mRID.mSerial, *certInfo);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [privKey, loadErr] = mCertLoader->LoadPrivKeyByURL(certInfo->mKeyURL);
    if (!loadErr.IsNone()) {
        return AOS_ERROR_WRAP(loadErr);
    }

    return DecryptCMSKey(info, *privKey, symmetricKey);
}

Error CryptoHelper::DecryptCMSKey(
    const TransRecipientInfo& ktri, const PrivateKeyItf& privKey, Array<uint8_t>& symmetricKey) const
{
    if (ktri.mKeyEncryptionAlgorithm.mOID != cRSAEncryptionOid) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "unknown public encryption OID"));
    }

    if (static constexpr auto cASN1TagNull = 5; ktri.mKeyEncryptionAlgorithm.mParams.mTagNumber != cASN1TagNull) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "extra parameters for RSA algorithm found"));
    }

    if (auto err = privKey.Decrypt(ktri.mEncryptedKey, DecryptionOptions {PKCS1v15DecryptionOptions {}}, symmetricKey);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::DecryptMessage(
    const EncryptedContentInfo& content, const Array<uint8_t>& symKey, Array<uint8_t>& message)
{
    static constexpr auto cTagOctetString = 4;

    if (content.mContentEncryptionAlgorithm.mOID != cAES256CBCOid) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotSupported, "unknown symmetric algorithm OID"));
    }

    if (content.mContentEncryptionAlgorithm.mParams.mTagNumber != cTagOctetString) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "can't find IV in extended params"));
    }

    if (content.mContentEncryptionAlgorithm.mParams.mValue.Size() != 16) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "invalid IV length"));
    }

    auto [decoder, err]
        = mCryptoProvider->CreateAESDecoder("CBC", symKey, content.mContentEncryptionAlgorithm.mParams.mValue);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return DecodeMessage(*decoder, content.mEncryptedContent, message);
}

Error CryptoHelper::DecodeMessage(AESCipherItf& decoder, const Array<uint8_t>& input, Array<uint8_t>& message)
{
    auto outBlock = MakeUnique<StaticArray<uint8_t, cFileChunkSize>>(mAllocator);
    if (!outBlock) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (input.Size() % AESCipherItf::cBlockSize != 0) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "message should be a multiple of CBC block size"));
    }

    if (message.MaxSize() < input.Size()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    message.Clear();

    for (size_t i = 0; i < input.Size(); i += cFileChunkSize) {
        auto blockSize = Min<size_t>(cFileChunkSize, input.Size() - i);
        auto inBlock   = Array<uint8_t>(input.Get() + i, blockSize);

        if (auto err = decoder.DecryptBlock(inBlock, *outBlock); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        (void)message.Insert(message.end(), outBlock->begin(), outBlock->end());
    }

    if (auto err = decoder.Finalize(*outBlock); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    (void)message.Insert(message.end(), outBlock->begin(), outBlock->end());

    return ErrorEnum::eNone;
}

} // namespace aos::crypto
