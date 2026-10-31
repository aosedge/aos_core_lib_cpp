/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_IMAGEMANAGER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_IMAGEMANAGER_HPP_

#include <core/cm/fileserver/itf/fileserver.hpp>
#include <core/common/crypto/itf/cryptohelper.hpp>
#include <core/common/downloader/itf/downloader.hpp>
#include <core/common/imagemanager/imagemanager.hpp>

#include "config.hpp"
#include "itf/imagemanager.hpp"

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * CM image manager.
 */
class ImageManager : public common::imagemanager::ImageManager,
                     public ImageManagerItf,
                     public common::imagemanager::BlobInfoProviderItf {
public:
    /**
     * Initializes image manager.
     *
     * @param config image manager config.
     * @param cloudBlobInfoProvider provides download info of the blobs from the cloud.
     * @param storage image manager storage.
     * @param spaceAllocator space allocator of the blobs.
     * @param downloadSpaceAllocator space allocator of the temporary download files.
     * @param downloader downloads blobs.
     * @param cryptoHelper decrypts blobs and verifies their signatures.
     * @param fileServer translates local blob paths to URLs.
     * @param fileInfoProvider file info provider.
     * @param ociSpec OCI spec interface.
     * @param fsPlatform file system platform interface.
     * @param allocator allocator for temporary objects.
     * @return Error.
     */
    Error Init(const Config& config, common::imagemanager::BlobInfoProviderItf& cloudBlobInfoProvider,
        common::imagemanager::StorageItf& storage, spaceallocator::SpaceAllocatorItf& spaceAllocator,
        spaceallocator::SpaceAllocatorItf& downloadSpaceAllocator, downloader::DownloaderItf& downloader,
        crypto::CryptoHelperItf& cryptoHelper, fileserver::FileServerItf& fileServer,
        fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec, fs::FSPlatformItf& fsPlatform,
        AllocatorItf& allocator);

    /**
     * Sets certificates and certificate chains used to verify blob signatures.
     *
     * @param certificates list of certificates.
     * @param certificateChains list of certificate chains.
     * @return Error.
     */
    Error SetCerts(const Array<crypto::CertificateInfo>& certificates,
        const Array<crypto::CertificateChainInfo>&       certificateChains) override;

    /**
     * Installs update item.
     *
     * @param itemInfo update item info.
     * @return Error.
     */
    Error InstallItem(const UpdateItemInfo& itemInfo) override;

    /**
     * Removes update item.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error.
     */
    Error RemoveItem(const String& itemID, const String& version) override;

    /**
     * Cancels all items being installed. Waits until all InstallItem and RemoveItem calls in progress return.
     *
     * @return Error.
     */
    Error Cancel() override;

    /**
     * Returns download info of a blob stored on the CM.
     *
     * @param digest blob digest.
     * @param[out] blobInfo blob info.
     * @return Error.
     */
    Error GetBlobInfo(const String& digest, BlobInfo& blobInfo) override;

    /**
     * Returns item remover which should be passed to the download space allocator to remove outdated temporary
     * download files.
     *
     * @return spaceallocator::ItemRemoverItf&.
     */
    spaceallocator::ItemRemoverItf& GetDownloadItemRemover() { return mDownloadItemRemover; }

private:
    class DownloadItemRemover : public spaceallocator::ItemRemoverItf {
    public:
        explicit DownloadItemRemover(ImageManager& imageManager)
            : mImageManager(imageManager)
        {
        }

        RetWithError<size_t> RemoveItem(const String& itemID, const String& version) override
        {
            (void)version;

            return mImageManager.RemoveDownloadedBlob(itemID);
        }

    private:
        ImageManager& mImageManager;
    };

    Error                DownloadBlob(CancelContext& ctx, const String& path, const BlobInfo& blobInfo) override;
    RetWithError<size_t> RemoveDownloadedBlob(const String& digest);

    Error                GetDownloadPath(const String& digest, String& downloadPath) const;
    RetWithError<size_t> GetFileSize(const String& path) const;
    RetWithError<size_t> PrepareDownloadFile(const String& downloadPath, size_t size);
    Error                DownloadFile(CancelContext& ctx, const String& downloadPath, const BlobInfo& blobInfo);
    void  AcceptDownloadedSpace(spaceallocator::SpaceItf& space, const String& downloadPath, size_t existingSize);
    void  RemoveDownloadFile(const String& downloadPath, size_t size);
    Error ProcessDownloadFile(const String& downloadPath, const String& path, const BlobInfo& blobInfo);
    Error ValidateDownloadFile(const String& downloadPath, const BlobInfo& blobInfo);
    Error DecryptBlob(const String& downloadPath, const String& path, const BlobInfo& blobInfo);
    Error ValidateBlobSigns(const String& downloadPath, const String& path, const BlobInfo& blobInfo);

    Config                             mConfig;
    common::imagemanager::StorageItf*  mStorage {};
    spaceallocator::SpaceAllocatorItf* mDownloadSpaceAllocator {};
    downloader::DownloaderItf*         mDownloader {};
    crypto::CryptoHelperItf*           mCryptoHelper {};
    fileserver::FileServerItf*         mFileServer {};
    fs::FileInfoProviderItf*           mFileInfoProvider {};
    AllocatorItf*                      mAllocator {};

    Mutex                             mCertsMutex;
    crypto::CertificateInfoArray      mCertificates;
    crypto::CertificateChainInfoArray mCertificateChains;

    DownloadItemRemover mDownloadItemRemover {*this};
};

/** @}*/

} // namespace aos::cm::imagemanager

#endif
