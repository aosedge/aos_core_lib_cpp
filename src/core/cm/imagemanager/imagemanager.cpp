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

    return ErrorEnum::eNotSupported;
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
    LOG_DBG() << "Download blob" << Log::Field("digest", blobInfo.mDigest) << Log::Field("path", path);

    (void)ctx;

    return ErrorEnum::eNotSupported;
}

RetWithError<size_t> ImageManager::RemoveDownloadedBlob(const String& digest)
{
    LOG_DBG() << "Remove downloaded blob" << Log::Field("digest", digest);

    return {0, ErrorEnum::eNotSupported};
}

} // namespace aos::cm::imagemanager
