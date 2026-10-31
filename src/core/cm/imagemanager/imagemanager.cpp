/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "imagemanager.hpp"

namespace aos::cm::imagemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ImageManager::Init(const Config& config, common::imagemanager::BlobInfoProviderItf& cloudBlobInfoProvider,
    common::imagemanager::StorageItf& storage, spaceallocator::SpaceAllocatorItf& spaceAllocator,
    spaceallocator::SpaceAllocatorItf& downloadSpaceAllocator, downloader::DownloaderItf& downloader,
    crypto::CryptoHelperItf& cryptoHelper, fileserver::FileServerItf& fileServer,
    fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec, fs::FSPlatformItf& fsPlatform,
    AllocatorItf& allocator)
{
    LOG_DBG() << "Init image manager";

    if (auto err = common::imagemanager::ImageManager::Init(
            config, cloudBlobInfoProvider, storage, spaceAllocator, fileInfoProvider, ociSpec, fsPlatform, allocator);
        !err.IsNone()) {
        return err;
    }

    mConfig                 = config;
    mStorage                = &storage;
    mDownloadSpaceAllocator = &downloadSpaceAllocator;
    mDownloader             = &downloader;
    mCryptoHelper           = &cryptoHelper;
    mFileServer             = &fileServer;
    mFileInfoProvider       = &fileInfoProvider;
    mAllocator              = &allocator;

    return ErrorEnum::eNone;
}

Error ImageManager::SetCerts(
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains)
{
    LOG_DBG() << "Set certs" << Log::Field("numCerts", certificates.Size())
              << Log::Field("numCertChains", certificateChains.Size());

    LockGuard lock {mCertsMutex};

    if (auto err = mCertificates.Assign(certificates); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mCertificateChains.Assign(certificateChains); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallItem(const UpdateItemInfo& itemInfo)
{
    LOG_DBG() << "Install item" << Log::Field("itemID", itemInfo.mItemID) << Log::Field("version", itemInfo.mVersion)
              << Log::Field("indexDigest", itemInfo.mIndexDigest);

    InstallItemInfo installItemInfo;

    installItemInfo.mItemID      = itemInfo.mItemID;
    installItemInfo.mType        = itemInfo.mType;
    installItemInfo.mVersion     = itemInfo.mVersion;
    installItemInfo.mRootDigest  = itemInfo.mIndexDigest;
    installItemInfo.mIsRootIndex = true;

    return common::imagemanager::ImageManager::InstallItem(installItemInfo);
}

Error ImageManager::RemoveItem(const String& itemID, const String& version)
{
    LOG_DBG() << "Remove item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    return common::imagemanager::ImageManager::RemoveItem(itemID, version);
}

Error ImageManager::Cancel()
{
    LOG_DBG() << "Cancel";

    return common::imagemanager::ImageManager::Cancel();
}

Error ImageManager::GetBlobInfo(const String& digest, BlobInfo& blobInfo)
{
    LOG_DBG() << "Get blob info" << Log::Field("digest", digest);

    (void)blobInfo;

    return ErrorEnum::eNotSupported;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ImageManager::DownloadBlob(CancelContext& ctx, const String& path, const BlobInfo& blobInfo)
{
    LOG_DBG() << "Download blob" << Log::Field("digest", blobInfo.mDigest) << Log::Field("path", path)
              << Log::Field("size", blobInfo.mSize);

    if (auto err = ctx.Err(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cFilePathLen> downloadPath;

    if (auto err = GetDownloadPath(blobInfo.mDigest, downloadPath); !err.IsNone()) {
        return err;
    }

    if (auto err = DownloadFile(ctx, downloadPath, blobInfo); !err.IsNone()) {
        return err;
    }

    if (auto err = ctx.Err(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ProcessDownloadFile(downloadPath, path, blobInfo);
}

RetWithError<size_t> ImageManager::RemoveDownloadedBlob(const String& digest)
{
    LOG_DBG() << "Remove downloaded blob" << Log::Field("digest", digest);

    return {0, ErrorEnum::eNotSupported};
}

Error ImageManager::GetDownloadPath(const String& digest, String& downloadPath) const
{
    StaticString<oci::cDigestLen> algorithm;
    StaticString<oci::cDigestLen> hash;

    if (auto err = ParseDigest(digest, algorithm, hash); !err.IsNone()) {
        return err;
    }

    if (mConfig.mDownloadPath.Size() + 1 + digest.Size() > cFilePathLen) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "download path too long"));
    }

    if (auto err = downloadPath.Assign(fs::JoinPath(mConfig.mDownloadPath, algorithm, hash)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<size_t> ImageManager::GetFileSize(const String& path) const
{
    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (!exists) {
        return {0, ErrorEnum::eNone};
    }

    auto [size, sizeErr] = fs::CalculateSize(*mAllocator, path);
    if (!sizeErr.IsNone()) {
        return {0, AOS_ERROR_WRAP(sizeErr)};
    }

    return {size, ErrorEnum::eNone};
}

RetWithError<size_t> ImageManager::PrepareDownloadFile(const String& downloadPath, size_t size)
{
    if (auto err = fs::MakeDirAll(fs::Dir(downloadPath)); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    auto [existingSize, err] = GetFileSize(downloadPath);
    if (!err.IsNone()) {
        return {0, err};
    }

    if (existingSize <= size) {
        return {existingSize, ErrorEnum::eNone};
    }

    LOG_WRN() << "Remove oversized download file" << Log::Field("path", downloadPath)
              << Log::Field("size", existingSize);

    if (err = fs::RemoveAll(downloadPath); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    mDownloadSpaceAllocator->FreeSpace(existingSize);

    return {0, ErrorEnum::eNone};
}

Error ImageManager::DownloadFile(CancelContext& ctx, const String& downloadPath, const BlobInfo& blobInfo)
{
    auto [existingSize, err] = PrepareDownloadFile(downloadPath, blobInfo.mSize);
    if (!err.IsNone()) {
        return err;
    }

    if (existingSize == blobInfo.mSize) {
        LOG_DBG() << "Download file is complete" << Log::Field("digest", blobInfo.mDigest)
                  << Log::Field("path", downloadPath);

        return ErrorEnum::eNone;
    }

    auto [space, allocateErr] = mDownloadSpaceAllocator->AllocateSpace(blobInfo.mSize - existingSize);
    if (!allocateErr.IsNone()) {
        return AOS_ERROR_WRAP(allocateErr);
    }

    LOG_DBG() << "Download file" << Log::Field("digest", blobInfo.mDigest) << Log::Field("path", downloadPath)
              << Log::Field("existingSize", existingSize);

    if (err = mDownloader->Download(ctx, blobInfo.mDigest, blobInfo.mURLs, downloadPath); !err.IsNone()) {
        AcceptDownloadedSpace(*space, downloadPath, existingSize);

        return AOS_ERROR_WRAP(err);
    }

    if (err = space->Accept(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void ImageManager::AcceptDownloadedSpace(
    spaceallocator::SpaceItf& space, const String& downloadPath, size_t existingSize)
{
    auto [size, sizeErr] = GetFileSize(downloadPath);
    if (!sizeErr.IsNone()) {
        LOG_ERR() << "Failed to get download file size" << Log::Field("path", downloadPath) << Log::Field(sizeErr);
    } else if (auto err = space.Resize(size > existingSize ? size - existingSize : 0); !err.IsNone()) {
        LOG_ERR() << "Failed to resize space" << Log::Field("path", downloadPath) << Log::Field(err);
    }

    if (auto err = space.Accept(); !err.IsNone()) {
        LOG_ERR() << "Failed to accept space" << Log::Field("path", downloadPath) << Log::Field(err);
    }

    if (sizeErr.IsNone() && size < existingSize) {
        mDownloadSpaceAllocator->FreeSpace(existingSize - size);
    }
}

void ImageManager::RemoveDownloadFile(const String& downloadPath, size_t size)
{
    LOG_DBG() << "Remove download file" << Log::Field("path", downloadPath);

    if (auto err = fs::RemoveAll(downloadPath); !err.IsNone()) {
        LOG_ERR() << "Failed to remove download file" << Log::Field("path", downloadPath) << Log::Field(err);

        return;
    }

    mDownloadSpaceAllocator->FreeSpace(size);
}

Error ImageManager::ProcessDownloadFile(const String& downloadPath, const String& path, const BlobInfo& blobInfo)
{
    if (auto err = ValidateDownloadFile(downloadPath, blobInfo); !err.IsNone()) {
        return err;
    }

    if (auto err = DecryptBlob(downloadPath, path, blobInfo); !err.IsNone()) {
        return err;
    }

    if (auto err = ValidateBlobSigns(downloadPath, path, blobInfo); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateDownloadFile(const String& downloadPath, const BlobInfo& blobInfo)
{
    LOG_DBG() << "Validate download file" << Log::Field("digest", blobInfo.mDigest) << Log::Field("path", downloadPath);

    auto [size, err] = GetFileSize(downloadPath);
    if (!err.IsNone()) {
        return err;
    }

    if (size != blobInfo.mSize) {
        RemoveDownloadFile(downloadPath, blobInfo.mSize);

        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "size mismatch"));
    }

    if (blobInfo.mSHA256.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    fs::FileInfo info;

    if (err = mFileInfoProvider->GetFileInfo(downloadPath, info, crypto::HashEnum::eSHA3_256); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (info.mCheckSum != blobInfo.mSHA256) {
        RemoveDownloadFile(downloadPath, blobInfo.mSize);

        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "checksum mismatch"));
    }

    return ErrorEnum::eNone;
}

Error ImageManager::DecryptBlob(const String& downloadPath, const String& path, const BlobInfo& blobInfo)
{
    if (!blobInfo.mDecryptInfo.HasValue()) {
        LOG_DBG() << "Copy blob" << Log::Field("digest", blobInfo.mDigest) << Log::Field("path", path);

        if (auto err = fs::CopyFile(*mAllocator, downloadPath, path); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    LOG_DBG() << "Decrypt blob" << Log::Field("digest", blobInfo.mDigest) << Log::Field("path", path);

    if (auto err = mCryptoHelper->Decrypt(downloadPath, path, blobInfo.mDecryptInfo.GetValue()); !err.IsNone()) {
        RemoveDownloadFile(downloadPath, blobInfo.mSize);

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateBlobSigns(const String& downloadPath, const String& path, const BlobInfo& blobInfo)
{
    if (!blobInfo.mSignInfo.HasValue()) {
        return ErrorEnum::eNone;
    }

    LOG_DBG() << "Validate blob signs" << Log::Field("digest", blobInfo.mDigest) << Log::Field("path", path);

    LockGuard lock {mCertsMutex};

    if (auto err = mCryptoHelper->ValidateSigns(path, blobInfo.mSignInfo.GetValue(), mCertificateChains, mCertificates);
        !err.IsNone()) {
        RemoveDownloadFile(downloadPath, blobInfo.mSize);

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::imagemanager
