/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_IMAGEMANAGER_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_IMAGEMANAGER_HPP_

#include <core/common/downloader/itf/downloader.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/spaceallocator/itf/spaceallocator.hpp>
#include <core/common/tools/timer.hpp>

#include "config.hpp"
#include "itf/blobinfoprovider.hpp"
#include "itf/imagehandler.hpp"
#include "itf/imagemanager.hpp"
#include "itf/iteminfoprovider.hpp"
#include "itf/storage.hpp"

namespace aos::sm::imagemanager {

/** @addtogroup sm Service Manager
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
     * @param blobInfoProvider blob info provider.
     * @param spaceAllocator space allocator.
     * @param downloader downloader.
     * @param fileInfoProvider file info provider.
     * @param ociSpec OCI spec interface.
     * @param imageHandler image handler.
     * @param storage image manager storage.
     * @return Error.
     */
    Error Init(const Config& config, BlobInfoProviderItf& blobInfoProvider,
        spaceallocator::SpaceAllocatorItf& spaceAllocator, downloader::DownloaderItf& downloader,
        fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec, ImageHandlerItf& imageHandler,
        StorageItf& storage);

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
     * Returns all installed update items statuses.
     *
     * @param statuses list of installed update item statuses.
     * @return Error.
     */
    Error GetAllInstalledItems(Array<UpdateItemStatus>& statuses) const override;

    /**
     * Installs update item.
     *
     * @param itemInfo update item info.
     * @return Error.
     */
    Error InstallUpdateItem(const UpdateItemInfo& itemInfo) override;

    /**
     * Removes update item.
     *
     * @param itemID update item ID.
     * @param version update item version.
     * @return Error.
     */
    Error RemoveUpdateItem(const String& itemID, const String& version) override;

    /**
     * Returns blob path by its digest.
     *
     * @param digest blob digest.
     * @param[out] path result blob path.
     * @return Error.
     */
    Error GetBlobPath(const String& digest, String& path) const override;

    /**
     * Returns layer path by its digest.
     *
     * @param digest layer digest.
     * @param[out] path result layer path.
     * @return Error.
     */
    Error GetLayerPath(const String& digest, String& path) const override;

private:
    static constexpr auto cBlobsFolder         = "blobs";
    static constexpr auto cLayersFolder        = "layers";
    static constexpr auto cUnpackedLayerFolder = "layer";
    static constexpr auto cDigestFile          = "digest";
    static constexpr auto cSizeFile            = "size";
    static constexpr auto cMaxNumItemVersions  = 2;
    static constexpr auto cAllocatorSize
        = cMaxNumConcurrentItems * (sizeof(oci::ImageManifest) + sizeof(oci::ImageConfig))
        + sizeof(UpdateItemDataStaticArray);

    RetWithError<size_t> RemoveItem(const String& id, const String& version) override;

    Error CreateBlobPath(const String& digest, String& path) const;
    Error CreateLayerPath(const String& digest, String& path) const;
    Error ValidateBlob(const String& path, const String& digest) const;
    Error DownloadBlob(const String& path, const String& digest, size_t size);
    Error InstallBlob(const oci::ContentDescriptor& descriptor, bool waitInProgress = true);
    Error ValidateLayer(const String& path, const String& diffDigest) const;
    Error CreateLayerMetadata(const String& path, size_t size, spaceallocator::SpaceItf* space);
    Error UnpackLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffDigest);
    Error InstallLayer(const oci::ContentDescriptor& descriptor, const String& diffDigest);
    Error GetBlobURL(const String& digest, String& url) const;
    void  ReleaseSpace(const String& path, spaceallocator::SpaceItf* space, Error err);
    Error WaitForInProgressBlob(const String& digest);
    Error ReleaseInProgressBlob(const String& digest);
    Error AddNewUpdateItem(const UpdateItemInfo& itemInfo);
    Error StoreUpdateItem(const UpdateItemInfo& itemInfo);
    Error RemoveUpdateItem(const UpdateItemData& itemData);
    Error RemoveOldUpdateItem(Array<UpdateItemData>& itemsData);
    Error UpdateOutdatedItems();
    Error HandleOutdatedItems();
    Error HandleItemsIntegrity();
    Error ValidateUpdateItem(const UpdateItemData& itemData);
    void  ProcessOutdatedItems();

    Config                             mConfig;
    BlobInfoProviderItf*               mBlobInfoProvider {};
    spaceallocator::SpaceAllocatorItf* mSpaceAllocator {};
    downloader::DownloaderItf*         mDownloader {};
    fs::FileInfoProviderItf*           mFileInfoProvider {};
    oci::OCISpecItf*                   mOCISpec {};
    ImageHandlerItf*                   mImageHandler {};
    StorageItf*                        mStorage {};

    mutable StaticAllocator<cAllocatorSize> mAllocator;

    Timer                                                             mTimer;
    mutable Mutex                                                     mMutex;
    ConditionalVariable                                               mCV;
    StaticList<StaticString<oci::cDigestLen>, cMaxNumConcurrentItems> mInProgressBlobs;
    Thread<>                                                          mThread;
    bool                                                              mClose {};
    bool                                                              mProcessOutdatedItems {};
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
