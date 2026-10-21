/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_IMAGEMANAGER_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_IMAGEMANAGER_HPP_

#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/spaceallocator/itf/spaceallocator.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/noncopyable.hpp>

#include "itf/blobinfoprovider.hpp"
#include "itf/itemstatusprovider.hpp"
#include "itf/storage.hpp"

#include "config.hpp"

namespace aos::common::imagemanager {

/**
 * Common image manager.
 *
 * Implements the part of the image manager shared by CM and SM. CM and SM image managers are derived from this class
 * and specialize blob download and layer handling.
 */
class ImageManager : public ItemStatusProviderItf, public LocalBlobInfoProviderItf, private NonCopyable {
public:
    /**
     * Destructor.
     */
    virtual ~ImageManager() = default;

    /**
     * Starts image manager.
     *
     * @return Error.
     */
    virtual Error Start();

    /**
     * Stops image manager. Cancels all items being installed and waits until all InstallItem and RemoveItem calls in
     * progress return.
     *
     * @return Error.
     */
    virtual Error Stop();

    /**
     * Retrieves deployable items statuses. Reports one status per item version in the storage.
     *
     * @param[out] statuses list of deployable items statuses.
     * @return Error.
     */
    Error GetItemsStatuses(Array<UpdateItemStatus>& statuses) override;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error: eAlreadyExist if the listener is already subscribed.
     */
    Error SubscribeListener(ItemStatusListenerItf& listener) override;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error: eNotFound if the listener is not subscribed.
     */
    Error UnsubscribeListener(ItemStatusListenerItf& listener) override;

    /**
     * Returns local blob path.
     *
     * @param digest blob digest.
     * @param[out] path local blob path.
     * @return Error.
     */
    Error GetBlobPath(const String& digest, String& path) const override;

    /**
     * Returns item remover which should be passed to the space allocator to remove outdated items.
     *
     * @return spaceallocator::ItemRemoverItf&.
     */
    spaceallocator::ItemRemoverItf& GetItemRemover() { return mItemRemover; }

protected:
    /**
     * Install item info.
     */
    struct InstallItemInfo {
        StaticString<cIDLen>          mItemID;
        UpdateItemType                mType;
        StaticString<cVersionLen>     mVersion;
        StaticString<oci::cDigestLen> mRootDigest;
        bool                          mIsRootIndex {};
    };

    /**
     * Initializes image manager. Derived classes call it from their own Init.
     *
     * @param config image manager config.
     * @param downloadBlobInfoProvider download blob info provider.
     * @param storage image manager storage.
     * @param spaceAllocator space allocator.
     * @param fileInfoProvider file info provider.
     * @param ociSpec OCI spec interface.
     * @param fsPlatform file system platform interface.
     * @param allocator allocator for temporary objects.
     * @return Error.
     */
    Error Init(const Config& config, DownloadBlobInfoProviderItf& downloadBlobInfoProvider, StorageItf& storage,
        spaceallocator::SpaceAllocatorItf& spaceAllocator, fs::FileInfoProviderItf& fileInfoProvider,
        oci::OCISpecItf& ociSpec, fs::FSPlatformItf& fsPlatform, AllocatorItf& allocator);

    /**
     * Installs item. Waits while the same item version is being installed or removed, or while
     * cMaxNumConcurrentItems items are being processed.
     *
     * @param itemInfo item info.
     * @return Error.
     */
    Error InstallItem(const InstallItemInfo& itemInfo);

    /**
     * Removes item. Sets the item state to eRemoved in the storage, keeping the item blobs, and adds the item as
     * outdated to the space allocator.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error.
     */
    Error RemoveItem(const String& itemID, const String& version);

    /**
     * Cancels all items being installed. Waits until all InstallItem and RemoveItem calls in progress return.
     *
     * @return Error.
     */
    Error Cancel();

    /**
     * Returns space allocator.
     *
     * @return spaceallocator::SpaceAllocatorItf&.
     */
    spaceallocator::SpaceAllocatorItf& GetSpaceAllocator() const { return *mSpaceAllocator; }

private:
    static constexpr auto cBlobsFolder       = "blobs";
    static constexpr auto cBlobHashSize      = Max(crypto::cSHA256Size, crypto::cSHA512Size);
    static constexpr auto cMaxNumListeners   = 4;
    static constexpr auto cMaxNumStoredItems = cMaxNumUpdateItems * 2;
    static constexpr auto cMaxNumItemBlobs   = 1 + oci::cMaxNumManifests * (3 + oci::cMaxNumLayers);
    static constexpr auto cRetryTimeout      = Time::cSeconds * 2;

    /**
     * Downloads blob. Called by InstallItem, derived classes implement it but must not call it directly.
     *
     * @param path destination blob path.
     * @param blobInfo blob information.
     * @return Error.
     */
    virtual Error DownloadBlob(const String& path, const BlobInfo& blobInfo) = 0;

    /**
     * Cancels ongoing blob download.
     *
     * @param digest blob digest.
     * @return Error: eNotFound if no download is in progress for the blob.
     */
    virtual Error CancelDownload(const String& digest) = 0;

    /**
     * Validates installed layer. Called by the image manager after the layer blob is validated, derived classes can
     * override it to validate unpacked layer data. Returns eNotSupported by default, which the image manager treats as
     * nothing to do (skip).
     *
     * @param path layer blob path.
     * @param descriptor layer descriptor.
     * @param diffID diff ID, empty if item has no image config.
     * @return Error.
     */
    virtual Error ValidateLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffID)
    {
        (void)path;
        (void)descriptor;
        (void)diffID;

        return ErrorEnum::eNotSupported;
    }

    /**
     * Unpacks installed layer. Called by the image manager after the layer blob is installed, derived classes can
     * override it to unpack layer data. Returns eNotSupported by default, which the image manager treats as nothing to
     * do (skip).
     *
     * @param path layer blob path.
     * @param descriptor layer descriptor.
     * @param diffID diff ID, empty if item has no image config.
     * @return Error.
     */
    virtual Error UnpackLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffID)
    {
        (void)path;
        (void)descriptor;
        (void)diffID;

        return ErrorEnum::eNotSupported;
    }

    /**
     * Removes installed layer. Called by the image manager before the layer blob is removed, derived classes can
     * override it to remove unpacked layer data. Returns eNotSupported by default, which the image manager treats as
     * nothing to do (skip).
     *
     * @param path layer blob path.
     * @param descriptor layer descriptor.
     * @param diffID diff ID, empty if item has no image config.
     * @return RetWithError<size_t>: size of the unpacked layer data freed.
     */
    virtual RetWithError<size_t> RemoveLayer(
        const String& path, const oci::ContentDescriptor& descriptor, const String& diffID)
    {
        (void)path;
        (void)descriptor;
        (void)diffID;

        return {0, ErrorEnum::eNotSupported};
    }

    class ItemRemover : public spaceallocator::ItemRemoverItf {
    public:
        explicit ItemRemover(ImageManager& imageManager)
            : mImageManager(imageManager)
        {
        }

        RetWithError<size_t> RemoveItem(const String& itemID, const String& version) override
        {
            return mImageManager.RemoveOutdatedItem(itemID, version);
        }

    private:
        ImageManager& mImageManager;
    };

    RetWithError<size_t> RemoveOutdatedItem(const String& itemID, const String& version);

    Config                             mConfig;
    DownloadBlobInfoProviderItf*       mBlobInfoProvider {};
    StorageItf*                        mStorage {};
    spaceallocator::SpaceAllocatorItf* mSpaceAllocator {};
    fs::FileInfoProviderItf*           mFileInfoProvider {};
    oci::OCISpecItf*                   mOCISpec {};
    fs::FSPlatformItf*                 mFSPlatform {};
    AllocatorItf*                      mAllocator {};

    ItemRemover mItemRemover {*this};
};

} // namespace aos::common::imagemanager

#endif
