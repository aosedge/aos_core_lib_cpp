/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_IMAGEMANAGER_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_IMAGEMANAGER_HPP_

#include <core/common/downloader/itf/downloader.hpp>
#include <core/common/imagemanager/imagemanager.hpp>

#include "itf/imagehandler.hpp"
#include "itf/imagemanager.hpp"
#include "itf/storage.hpp"

namespace aos::sm::imagemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * SM image manager.
 *
 * Downloads deployable items from the CM and unpacks their layers. A layer blob is removed once the layer is unpacked.
 * The layers of the items without an image config are kept as packed blobs only.
 */
class ImageManager : public common::imagemanager::ImageManager, public ImageManagerItf {
public:
    /**
     * Initializes image manager.
     *
     * @param config image manager config.
     * @param blobInfoProvider provides download info of the blobs from the CM.
     * @param storage image manager storage.
     * @param spaceAllocator space allocator of the blobs and the unpacked layers.
     * @param downloader downloads blobs.
     * @param imageHandler unpacks layers.
     * @param fileInfoProvider file info provider.
     * @param ociSpec OCI spec interface.
     * @param fsPlatform file system platform interface.
     * @param allocator allocator for temporary objects.
     * @return Error: eInvalidArgument if the image path is too long to hold the longest blob or layer path.
     */
    Error Init(const common::imagemanager::Config& config, common::imagemanager::BlobInfoProviderItf& blobInfoProvider,
        StorageItf& storage, spaceallocator::SpaceAllocatorItf& spaceAllocator, downloader::DownloaderItf& downloader,
        ImageHandlerItf& imageHandler, fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec,
        fs::FSPlatformItf& fsPlatform, AllocatorItf& allocator);

    /**
     * Installs update item. Calling it for an installed item validates the item and repairs it if needed.
     *
     * @param itemInfo update item info.
     * @return Error.
     */
    Error InstallItem(const UpdateItemInfo& itemInfo) override;

    /**
     * Removes update item. The item is kept in the cache as an outdated item, so its blobs and unpacked layers are
     * reused if the item is installed again.
     *
     * @param itemID update item ID.
     * @param version update item version.
     * @return Error.
     */
    Error RemoveItem(const String& itemID, const String& version) override;

    /**
     * Cancels all items being installed, including the items waiting for a layer processed by another item. Waits
     * until all InstallItem and RemoveItem calls in progress return.
     *
     * @return Error.
     */
    Error Cancel() override;

    /**
     * Returns unpacked layer path. It doesn't validate the unpacked layer. The unpacked layer of an item in the
     * eRemoved state may be removed at any time, so the consumer must request the path only for the layers of the items
     * it uses.
     *
     * @param diffID layer diff ID.
     * @param[out] path unpacked layer path.
     * @return Error: eInvalidArgument if the diff ID is malformed, eNotFound if the unpacked layer doesn't exist.
     */
    Error GetLayerPath(const String& diffID, String& path) const override;

private:
    static constexpr auto cLayersFolder        = "layers";
    static constexpr auto cUnpackedLayerFolder = "layer";
    static constexpr auto cDigestFile          = "digest";
    static constexpr auto cSizeFile            = "size";
    // Extra slots for the item remover and the cleanup, which don't wait for the layers.
    static constexpr auto cMaxNumLayersInProgress = cMaxNumConcurrentItems + 2;

    Error DownloadBlob(CancelContext& ctx, const String& path, const BlobInfo& blobInfo) override;
    Error ValidateLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffID) override;
    Error UnpackLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffID) override;
    RetWithError<size_t> RemoveLayer(
        const String& path, const oci::ContentDescriptor& descriptor, const String& diffID) override;
    RetWithError<size_t> RemoveOrphanLayers() override;

    Error CreateLayerPath(const String& diffID, String& path) const;
    Error ValidateUnpackedLayer(const String& layerPath) const;
    Error InstallUnpackedLayer(const String& path, const String& mediaType, const String& layerPath);
    Error UnpackToLayerPath(const String& path, const String& mediaType, const String& layerPath, size_t unpackedSize,
        spaceallocator::SpaceItf& space);
    Error CreateLayerMetadata(const String& layerPath, size_t unpackedSize);
    RetWithError<size_t> RemoveLayerFiles(const String& layerPath);
    void                 AcceptRemainingSpace(const String& layerPath, spaceallocator::SpaceItf& space);
    void                 RemoveLayerBlob(const String& path);
    static Error         CheckLayerMediaType(const String& mediaType);
    RetWithError<size_t> RemoveOrphanLayer(const String& layerPath, const String& diffID);

    Error LockLayer(const String& diffID);
    bool  TryLockLayer(const String& diffID);
    void  UnlockLayer(const String& diffID);
    void  SetLayersCanceled(bool canceled);

    bool IsLayerInProgress(const String& diffID) const
    {
        return mLayersInProgress.Find(diffID) != mLayersInProgress.end();
    }

    StaticString<cFilePathLen> mLayersPath;
    downloader::DownloaderItf* mDownloader {};
    ImageHandlerItf*           mImageHandler {};
    StorageItf*                mStorage {};
    AllocatorItf*              mAllocator {};

    Mutex                                                              mLayersMutex;
    ConditionalVariable                                                mLayersCondVar;
    StaticList<StaticString<oci::cDigestLen>, cMaxNumLayersInProgress> mLayersInProgress;
    size_t                                                             mNumCancels {};
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
