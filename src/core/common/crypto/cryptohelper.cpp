/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>

namespace aos::crypto {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

CryptoHelper::CryptoHelper()
    : mSemaphore(cMaxNumConcurrentItems)
{
}

Error CryptoHelper::Init(iamclient::CertProviderItf& certProvider, CryptoProviderItf& cryptoProvider,
    CertLoaderItf& certLoader, const String& serviceDiscoveryURL, const String& caCert)
{
    mCertProvider        = &certProvider;
    mCryptoProvider      = &cryptoProvider;
    mCertLoader          = &certLoader;
    mServiceDiscoveryURL = serviceDiscoveryURL;

    auto caCertsPEM = MakeUnique<StaticString<cCertPEMLen>>(&mAllocator);

    if (auto err = fs::ReadFileToString(caCert, *caCertsPEM); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mCryptoProvider->PEMToX509Certs(*caCertsPEM, mCACerts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
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
    const auto& symmetricAlgName = decryptInfo.mBlockAlg;
    const auto& sessionKey       = decryptInfo.mBlockKey;
    const auto& sessionIV        = decryptInfo.mBlockIV;

    StaticString<cAlgLen> algName, modeName, paddingName;

    auto err = DecodeSymAlgNames(symmetricAlgName, algName, modeName, paddingName);
    if (!err.IsNone()) {
        return err;
    }

    auto [decoder, createDecoderErr] = mCryptoProvider->CreateAESDecoder(modeName, sessionKey, sessionIV);
    if (!createDecoderErr.IsNone()) {
        return AOS_ERROR_WRAP(createDecoderErr);
    }

    if (auto checkErr = CheckSessionKey(algName, sessionIV, sessionKey); !checkErr.IsNone()) {
        return AOS_ERROR_WRAP(checkErr);
    }

    if (auto decodeErr = DecodeFile(encryptedFile, decryptedFile, *decoder); !decodeErr.IsNone()) {
        return AOS_ERROR_WRAP(decodeErr);
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::ValidateSigns(const String& decryptedPath, const SignInfo& signs,
    const Array<CertificateChainInfo>& chains, const Array<CertificateInfo>& certs)
{
    LockGuard lock {mSemaphore};

    auto signCtx = MakeUnique<SignContext>(&mAllocator);

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

    auto contentInfo = MakeUnique<ContentInfo>(&mAllocator);
    auto symKey      = MakeUnique<StaticArray<uint8_t, cPrivKeyPEMLen>>(&mAllocator);

    auto err = UnmarshalCMS(input, *contentInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& recipient : contentInfo->mEnvelopeData.mRecipientInfos) {
        err = GetKeyForEnvelope(recipient, *symKey);
        if (!err.IsNone()) {
            LOG_WRN() << "Can't get key for envelope" << Log::Field(err);
            continue;
        }

        err = DecryptMessage(contentInfo->mEnvelopeData.mEncryptedContent, *symKey, output);
        if (!err.IsNone()) {
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
    auto certInfo = MakeUnique<CertInfo>(&mAllocator);
    if (auto err = mCertProvider->GetCert(cOnlineCert, {}, {}, *certInfo); !err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    auto [chain, err] = mCertLoader->LoadCertsChainByURL(certInfo->mCertURL);
    if (!err.IsNone()) {
        return {chain, AOS_ERROR_WRAP(err)};
    }

    return chain;
}

Error CryptoHelper::SetDefaultServiceDiscoveryURL(Array<StaticString<cURLLen>>& urls)
{
    if (urls.IsEmpty()) {
        LOG_WRN() << "Service discovery URL can't be found in certificate and will be used from config";

        return AOS_ERROR_WRAP(urls.PushBack(mServiceDiscoveryURL));
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::GetServiceDiscoveryFromExtensions(const x509::Certificate& cert, Array<StaticString<cURLLen>>& urls)
{
    auto err = urls.Insert(urls.begin(), cert.mIssuerURLs.begin(), cert.mIssuerURLs.end());
    if (!err.IsNone()) {
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
    auto subject = MakeUnique<StaticString<cCertSubjSize>>(&mAllocator);

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

    StaticString<cURLLen> orgName, url;

    auto assignErr = orgName.Insert(orgName.begin(), subject->begin() + valueStart, subject->begin() + valueEnd);
    if (!assignErr.IsNone()) {
        return AOS_ERROR_WRAP(assignErr);
    }

    if (orgName.IsEmpty()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = url.Format("https://%s:%d", orgName.CStr(), cServiceDiscoveryDefaultPort); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = urls.EmplaceBack(url); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::DecodeSymAlgNames(const String& algString, String& algName, String& modeName, String& paddingName)
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

Error CryptoHelper::GetSymmetricAlgInfo(const String& algName, size_t& keySize, size_t& ivSize)
{
    if (algName == "AES128") {
        keySize = 16;
        ivSize  = 16;

        return ErrorEnum::eNone;
    } else if (algName == "AES192") {
        keySize = 24;
        ivSize  = 16;

        return ErrorEnum::eNone;
    } else if (algName == "AES256") {
        keySize = 32;
        ivSize  = 16;

        return ErrorEnum::eNone;
    }

    return ErrorEnum::eNotSupported;
}

Error CryptoHelper::CheckSessionKey(
    const String& symAlgName, const Array<uint8_t>& sessionIV, const Array<uint8_t>& sessionKey)
{
    size_t keySize = 0, ivSize = 0;

    auto err = GetSymmetricAlgInfo(symAlgName, keySize, ivSize);
    if (!err.IsNone()) {
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

Error CryptoHelper::DecodeFile(const String& encryptedFile, const String& decryptedFile, AESCipherItf& decoder)
{
    AESCipherItf::Block inBlock, outBlock;
    fs::File            inputFile, outputFile;

    Error err = inputFile.Open(encryptedFile, fs::File::Mode::Read);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = outputFile.Open(decryptedFile, fs::File::Mode::Write);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    while (true) {
        err = inputFile.ReadBlock(inBlock);
        if (!err.IsNone() && !err.Is(ErrorEnum::eEOF)) {
            return AOS_ERROR_WRAP(err);
        }

        if (inBlock.IsEmpty()) {
            break;
        }

        if (inBlock.Size() != inBlock.MaxSize()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "file size is incorrect"));
        }

        err = decoder.DecryptBlock(inBlock, outBlock);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = outputFile.WriteBlock(outBlock);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    err = decoder.Finalize(outBlock);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = outputFile.WriteBlock(outBlock);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = inputFile.Close();
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = outputFile.Close();
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::AddCertificates(const Array<CertificateInfo>& certs, SignContext& ctx)
{
    ctx.mCerts.Clear();

    for (const auto& certInfo : certs) {
        StaticString<cCertFingerprintLen> fingerprint = certInfo.mFingerprint;

        fingerprint.ToUpper();

        auto iter = ctx.mCerts.FindIf(
            [&fingerprint](const X509CertificateInfo& certInfo) { return certInfo.mFingerprint == fingerprint; });

        if (iter != ctx.mCerts.end()) {
            continue;
        }

        auto cert = MakeUnique<x509::Certificate>(&mAllocator);

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

Error CryptoHelper::AddCertChains(const Array<CertificateChainInfo>& chains, SignContext& ctx)
{
    ctx.mChains.Clear();

    for (const auto& chainInfo : chains) {
        auto iter = ctx.mChains.FindIf(
            [&chainInfo](const CertificateChainInfo& item) { return item.mName == chainInfo.mName; });

        if (iter != ctx.mChains.end()) {
            continue;
        }

        if (auto err = ctx.mChains.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ctx.mChains.Back().mName         = chainInfo.mName;
        ctx.mChains.Back().mFingerprints = chainInfo.mFingerprints;

        for (auto& fingerprint : ctx.mChains.Back().mFingerprints) {
            fingerprint.ToUpper();
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

    StaticString<cAlgLen> algName, hashName, paddingName;

    if (auto err = DecodeSignAlgNames(signs.mAlg, algName, hashName, paddingName); !err.IsNone()) {
        return err;
    }

    auto [hash, hashErr] = DecodeHash(hashName);
    if (!hashErr.IsNone()) {
        return hashErr;
    }

    // Verify sign
    auto hashSum = MakeUnique<StaticArray<uint8_t, cMaxHashSize>>(&mAllocator);
    if (auto err = CalcHashSum(hash, file, *hashSum); !err.IsNone()) {
        return err;
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

    auto verifyErr = mCryptoProvider->Verify(signCert->mPublicKey, hash, padding, *hashSum, signs.mValue);
    if (!verifyErr.IsNone()) {
        return AOS_ERROR_WRAP(verifyErr);
    }

    // Verify certs
    auto intermCertPool = MakeUnique<StaticArray<x509::Certificate, cMaxNumCertificates>>(&mAllocator);

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

RetWithError<x509::Certificate*> CryptoHelper::GetCert(SignContext& signCtx, const String& fingerprint)
{
    auto iter = signCtx.mCerts.FindIf(
        [&fingerprint](const X509CertificateInfo& info) { return info.mFingerprint == fingerprint; });

    if (iter == signCtx.mCerts.end()) {
        return {nullptr, ErrorEnum::eNotFound};
    }

    return &iter->mCertificate;
}

Error CryptoHelper::GetSignCert(
    SignContext& signCtx, const String& chainName, x509::Certificate*& signCert, CertificateChainInfo*& chain)
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

Error CryptoHelper::DecodeSignAlgNames(const String& algString, String& algName, String& hashName, String& paddingName)
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

RetWithError<Hash> CryptoHelper::DecodeHash(const String& hashName)
{
    StaticString<cAlgLen> upperHash = hashName;

    upperHash.ToUpper();

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

Error CryptoHelper::CalcHashSum(const Hash& hash, const String& fileName, Array<uint8_t>& hashSum)
{
    auto [hasher, err] = mCryptoProvider->CreateHash(hash);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    fs::File file;

    err = file.Open(fileName, fs::File::Mode::Read);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto buffer = MakeUnique<StaticArray<uint8_t, cReadChunkSize>>(&mAllocator);

    while (true) {
        err = file.ReadBlock(*buffer);
        if (!err.IsNone() && !err.Is(ErrorEnum::eEOF)) {
            return AOS_ERROR_WRAP(err);
        }

        if (buffer->IsEmpty()) {
            break;
        }

        err = hasher->Update(*buffer);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (err.Is(ErrorEnum::eEOF)) {
            break;
        }
    }

    file.Close();

    err = hasher->Finalize(hashSum);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error CryptoHelper::CreateIntermCertPool(
    SignContext& signCtx, const CertificateChainInfo& chain, Array<x509::Certificate>& pool)
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
    auto certInfo = MakeUnique<CertInfo>(&mAllocator);

    auto err = mCertProvider->GetCert(cOfflineCert, info.mRID.mIssuer, info.mRID.mSerial, *certInfo);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [privKey, loadErr] = mCertLoader->LoadPrivKeyByURL(certInfo->mKeyURL);
    if (!loadErr.IsNone()) {
        return AOS_ERROR_WRAP(loadErr);
    }

    return DecryptCMSKey(info, *privKey, symmetricKey);
}

Error CryptoHelper::DecryptCMSKey(
    const TransRecipientInfo& ktri, const PrivateKeyItf& privKey, Array<uint8_t>& symmetricKey)
{
    if (ktri.mKeyEncryptionAlgorithm.mOID != cRSAEncryptionOid) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "unknown public encryption OID"));
    }

    static constexpr auto cASN1TagNull = 5;
    if (ktri.mKeyEncryptionAlgorithm.mParams.mTagNumber != cASN1TagNull) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "extra parameters for RSA algorithm found"));
    }

    if (auto decryptErr
        = privKey.Decrypt(ktri.mEncryptedKey, DecryptionOptions {PKCS1v15DecryptionOptions {}}, symmetricKey);
        !decryptErr.IsNone()) {
        return AOS_ERROR_WRAP(decryptErr);
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
    AESCipherItf::Block inBlock, outBlock;
    const auto          cBlockSize = inBlock.MaxSize();

    if (input.Size() % cBlockSize != 0) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "message should be a multiple of CBC block size"));
    }

    if (message.MaxSize() < input.Size()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    message.Clear();

    for (size_t i = 0; i < input.Size(); i += cBlockSize) {
        inBlock = Array<uint8_t>(input.begin() + i, cBlockSize);

        auto err = decoder.DecryptBlock(inBlock, outBlock);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        message.Insert(message.end(), outBlock.begin(), outBlock.end());
    }

    auto err = decoder.Finalize(outBlock);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    message.Insert(message.end(), outBlock.begin(), outBlock.end());

    return ErrorEnum::eNone;
}

} // namespace aos::crypto
