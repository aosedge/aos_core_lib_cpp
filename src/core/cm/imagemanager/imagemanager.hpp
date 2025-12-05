/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_IMAGEMANAGER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_IMAGEMANAGER_HPP_

#include <core/cm/fileserver/itf/fileserver.hpp>
#include <core/common/blobinfoprovider/itf/blobinfoprovider.hpp>
#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/downloader/itf/downloader.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/spaceallocator/itf/itemremover.hpp>
#include <core/common/spaceallocator/itf/spaceallocator.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>

#include "config.hpp"
#include "itf/imagemanager.hpp"
#include "itf/iteminfoprovider.hpp"
#include "itf/storage.hpp"

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Image manager.
 */
class ImageManager : public ImageManagerItf, public ItemInfoProviderItf, public spaceallocator::ItemRemoverItf {
public:
    /**
     * Initializes image manager.
     *
     * @param config image manager config.
     * @param storage stores internal persistent data.
     * @param blobInfoProvider retrieves blobs info.
     * @param downloadingSpaceAllocator allocates temporary space for downloading.
     * @param installSpaceAllocator allocates disk space for install.
     * @param downloader downloads blobs.
     * @param fileserver translates local file paths to remote URLs.
     * @param cryptoHelper decrypts and verifies update images.
     * @param fileInfoProvider gets file info.
     * @param ociSpec parses OCI spec files.
     * @return Error.
     */
    Error Init(const Config& config, StorageItf& storage, blobinfoprovider::ProviderItf& blobInfoProvider,
        spaceallocator::SpaceAllocatorItf& downloadingSpaceAllocator,
        spaceallocator::SpaceAllocatorItf& installSpaceAllocator, downloader::DownloaderItf& downloader,
        fileserver::FileServerItf& fileserver, crypto::CryptoHelperItf& cryptoHelper,
        fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec);

    /**
     * Starts image manager.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops image manager.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Downloads update items.
     *
     * @param itemsInfo update items info.
     * @param certificates list of certificates.
     * @param certificateChains list of certificate chains.
     * @param[out] statuses update items statuses.
     * @return Error.
     */
    Error DownloadUpdateItems(const Array<UpdateItemInfo>& itemsInfo,
        const Array<crypto::CertificateInfo>&              certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, Array<UpdateItemStatus>& statuses) override;

    /**
     * Installs update items.
     *
     * @param itemsInfo update items info.
     * @param[out] statuses update items statuses.
     * @return Error.
     */
    Error InstallUpdateItems(const Array<UpdateItemInfo>& itemsInfo, Array<UpdateItemStatus>& statuses) override;

    /**
     * Cancels ongoing operations: download or install.
     *
     * @return Error.
     */
    Error Cancel() override;

    /**
     * Retrieves update items statuses.
     *
     * @param[out] statuses list of update items statuses.
     * @return Error.
     */
    Error GetUpdateItemsStatuses(Array<UpdateItemStatus>& statuses) override;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    Error SubscribeListener(ItemStatusListenerItf& listener) override;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    Error UnsubscribeListener(ItemStatusListenerItf& listener) override;

    /**
     * Returns update item index digest.
     *
     * @param itemID update item ID.
     * @param version update item version.
     * @param[out] digest result item digest.
     * @return Error.
     */
    Error GetIndexDigest(const String& itemID, const String& version, String& digest) const override;

    /**
     * Returns blob path by its digest.
     *
     * @param digest blob digest.
     * @param[out] path result blob path.
     * @return Error.
     */
    Error GetBlobPath(const String& digest, String& path) const override;

    /**
     * Returns blob URL by its digest.
     *
     * @param digest blob digest.
     * @param[out] url result blob URL.
     * @return Error.
     */
    Error GetBlobURL(const String& digest, String& url) const override;

    /**
     * Returns item current version.
     *
     * @param itemID update item ID.
     * @param[out] version result item version.
     * @return Error.
     */
    Error GetItemCurrentVersion(const String& itemID, String& version) const override;

    /**
     * Removes item.
     *
     * @param id item id.
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> RemoveItem(const String& id) override;

private:
    static constexpr auto cMaxNumListeners = 1;
    static constexpr auto cRetryTimeout    = Time::cSeconds * 2;

    Error WaitForStop();
    Error AllocateSpaceForPartialDownloads();
    Error CleanupDownloadingItems(const Array<UpdateItemInfo>& currentItems, const Array<ItemInfo>& storedItems);
    Error VerifyStoredItems(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems,
        const Array<crypto::CertificateInfo>&      certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, Array<UpdateItemStatus>& statuses);
    Error ProcessDownloadRequest(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems,
        const Array<crypto::CertificateInfo>&      certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, Array<UpdateItemStatus>& statuses);
    RetWithError<size_t> CleanupOrphanedBlobs();
    Error RemoveDifferentVersions(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems);
    Error VerifyBlobsIntegrity(
        const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems, Array<UpdateItemStatus>& statuses);
    Error SetItemsToInstalled(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems);
    Error SetItemsToRemoved(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems);
    Error DownloadItem(const UpdateItemInfo& itemInfo, const Array<crypto::CertificateInfo>& certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains);
    Error LoadIndex(const String& digest, const String& downloadPath, const String& installPath,
        const Array<crypto::CertificateInfo>&      certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, oci::ImageIndex& imageIndex);
    Error LoadManifest(const String& digest, const Array<crypto::CertificateInfo>& certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, oci::ImageManifest& manifest);
    Error LoadLayers(const Array<oci::ContentDescriptor>& layers, const Array<crypto::CertificateInfo>& certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains);
    Error EnsureBlob(const String& digest, const String& downloadPath, const String& installPath,
        const Array<crypto::CertificateInfo>&      certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, UniquePtr<spaceallocator::SpaceItf>& space);
    Error DownloadBlob(const String& digest, const String& downloadPath, const String& installPath, BlobInfo& blobInfo,
        UniquePtr<spaceallocator::SpaceItf>& downloadingSpace);
    Error GetBlobInfo(const String& digest, BlobInfo& blobInfo);
    Error CheckExistingBlob(const String& installPath, const BlobInfo& blobInfo);
    Error PrepareDownloadSpace(const String& downloadPath, const BlobInfo& blobInfo, size_t& partialDownloadSize,
        UniquePtr<spaceallocator::SpaceItf>& downloadingSpace);
    Error PerformDownload(const BlobInfo& blobInfo, const String& downloadPath, size_t partialDownloadSize,
        UniquePtr<spaceallocator::SpaceItf>& downloadingSpace);
    Error DecryptAndValidateBlob(const String& downloadPath, const String& installPath, const BlobInfo& blobInfo,
        const Array<crypto::CertificateInfo>&      certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains,
        UniquePtr<spaceallocator::SpaceItf>&       installSpace);
    Error VerifyItemBlobs(const String& indexDigest);
    Error VerifyBlobChecksum(const String& digest, const fs::FileInfo& fileInfo);
    bool  IsBlobUsedByItems(const String& blobDigest, const Array<ItemInfo>& items);
    void  NotifyItemsStatusesChanged(const Array<UpdateItemStatus>& statuses);
    bool  StartAction();
    void  StopAction();

    StorageItf*                        mStorage {};
    blobinfoprovider::ProviderItf*     mBlobInfoProvider {};
    spaceallocator::SpaceAllocatorItf* mDownloadingSpaceAllocator {};
    spaceallocator::SpaceAllocatorItf* mInstallSpaceAllocator {};
    downloader::DownloaderItf*         mDownloader {};
    fileserver::FileServerItf*         mFileServer {};
    crypto::CryptoHelperItf*           mCryptoHelper {};
    fs::FileInfoProviderItf*           mFileInfoProvider {};
    oci::OCISpecItf*                   mOCISpec {};

    StaticArray<ItemStatusListenerItf*, cMaxNumListeners> mListeners;

    Timer                         mTimer;
    Mutex                         mMutex;
    Config                        mConfig {};
    StaticString<cFilePathLen>    mBlobsDownloadPath {};
    StaticString<cFilePathLen>    mBlobsInstallPath {};
    StaticString<oci::cDigestLen> mCurrentDownloadDigest {};
    ConditionalVariable           mCondVar;
    bool                          mCancel {};
    bool                          mInProgress {};
    StaticAllocator<((sizeof(StaticArray<ItemInfo, cMaxNumUpdateItems>) * 2) + sizeof(oci::ImageIndex)
        + sizeof(oci::ImageManifest) + sizeof(StaticArray<BlobInfo, 1>)
        + sizeof(StaticArray<uint8_t, crypto::cSHA256Size>))>
        mAllocator;
};

} // namespace aos::cm::imagemanager

#endif
