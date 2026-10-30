/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_IMAGEMANAGER_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_IMAGEMANAGER_HPP_

#include <core/common/consts.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/spaceallocator/itf/spaceallocator.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/list.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/noncopyable.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>

#include "itf/blobinfoprovider.hpp"
#include "itf/iteminfoprovider.hpp"
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
class ImageManager : public ItemStatusProviderItf, public ItemInfoProviderItf, private NonCopyable {
public:
    /**
     * Destructor.
     */
    virtual ~ImageManager() = default;

    /**
     * Starts image manager. Sets the items whose install was interrupted (left in the eInstalling state) to the
     * eRemoved state, keeping their blobs, and adds all the items in the eRemoved state as outdated to the space
     * allocator. No item status is notified. Then starts removing the expired items, i.e. the items in the eRemoved
     * state for longer than mItemCacheTTL, and then the orphan blobs, i.e. the blobs not used by any item, right away
     * and every mRemoveOutdatedPeriod. The items and blobs being processed are skipped.
     *
     * @return Error: storage errors as is. Errors of adding an outdated item are only logged.
     */
    virtual Error Start();

    /**
     * Stops image manager. Stops removing the expired items and the orphan blobs and waits until the removal in
     * progress is finished.
     *
     * @return Error.
     */
    virtual Error Stop();

    /**
     * Retrieves deployable items statuses. Reports one status per item version in the storage. An item version being
     * installed, whose eInstalling state is already notified, is reported with its last notified state (eInstalling or
     * eDownloading) until its final state is notified. An item version being removed, whose eRemoving state is already
     * notified, is reported as eRemoving until its eRemoved state is set in the storage or the storage update fails.
     * Otherwise the stored state is reported. mError is set only for eFailed items and is taken from the storage.
     * mPreinstalled is always false. ePending is never reported.
     *
     * @note The statuses are appended to the output array, which is not cleared.
     *
     * @param[out] statuses list of deployable items statuses.
     * @return Error: eNoMemory if the statuses don't fit the output array, storage errors as is.
     */
    Error GetItemsStatuses(Array<UpdateItemStatus>& statuses) override;

    /**
     * Subscribes status notifications.
     *
     * @note Must not be called from a listener notification callback: listeners are notified under the listeners
     * lock, so the call deadlocks.
     *
     * @param listener status listener.
     * @return Error: eAlreadyExist if the listener is already subscribed.
     */
    Error SubscribeListener(ItemStatusListenerItf& listener) override;

    /**
     * Unsubscribes from status notifications.
     *
     * @note Must not be called from a listener notification callback: listeners are notified under the listeners
     * lock, so the call deadlocks.
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
     * Returns unpacked layer path. Returns eNotSupported by default, as the common image manager doesn't keep the
     * unpacked layers.
     *
     * @param diffID layer diff ID.
     * @param[out] path unpacked layer path.
     * @return Error.
     */
    Error GetLayerPath(const String& diffID, String& path) const override;

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
     * @note The allocator must provide, per concurrent InstallItem (up to cMaxNumConcurrentItems), one
     * oci::ImageIndex, one oci::ImageManifest, one oci::ImageConfig and one blob hash buffer of cBlobHashSize at the
     * same time, plus two StaticArray<DeployableItemInfo, cMaxNumStoredItems> for Start or the expired items removal
     * and GetItemsStatuses, and two StaticArray<ItemBlobInfo, cMaxNumItemBlobs> for the item remover and the expired
     * items removal.
     *
     * @param config image manager config.
     * @param blobInfoProvider blob info provider.
     * @param storage image manager storage.
     * @param spaceAllocator space allocator.
     * @param fileInfoProvider file info provider.
     * @param ociSpec OCI spec interface.
     * @param fsPlatform file system platform interface.
     * @param allocator allocator for temporary objects.
     * @return Error.
     */
    Error Init(const Config& config, BlobInfoProviderItf& blobInfoProvider, StorageItf& storage,
        spaceallocator::SpaceAllocatorItf& spaceAllocator, fs::FileInfoProviderItf& fileInfoProvider,
        oci::OCISpecItf& ociSpec, fs::FSPlatformItf& fsPlatform, AllocatorItf& allocator);

    /**
     * Installs item. Waits while the same item version is being installed or removed, or while
     * cMaxNumConcurrentItems items are being processed.
     *
     * @note The final eInstalled or eFailed status is notified while the item is still in progress, so a listener
     * must not call InstallItem or RemoveItem synchronously from the callback.
     *
     * @param itemInfo item info.
     * @return Error: eInvalidArgument if the root digest is malformed, the item version is stored with a different
     * root digest or a diff ID is missing in the image config, eNoMemory if the storage is full and there is no
     * outdated item to remove, other errors as is.
     */
    Error InstallItem(const InstallItemInfo& itemInfo);

    /**
     * Removes item. Sets the item state to eRemoved in the storage, keeping the item blobs, and adds the item as
     * outdated to the space allocator. Waits while the same item version is being installed or removed, or while
     * cMaxNumConcurrentItems items are being processed. Does nothing if the item is already in the eRemoved state.
     *
     * @note The eRemoved status is notified while the item is still in progress, so a listener must not call
     * InstallItem or RemoveItem synchronously from the callback.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error: eNotFound if the item version is not stored, other errors as is.
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
    // Extra slots for the item remover and the expired items removal, which don't wait for the items and blobs. The
    // item remover is called synchronously by the space allocator, so it must not wait or take the slot of another one.
    static constexpr auto cMaxNumInProgress = cMaxNumConcurrentItems + 2;

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
     * Validates installed layer. Called by the image manager before the layer blob is validated, derived classes can
     * override it to validate unpacked layer data. If it succeeds, the layer blob is not validated. Any error other
     * than eNotSupported means the unpacked layer data is missing or invalid: the image manager calls RemoveLayer and
     * UnpackLayer for the layer. Returns eNotSupported by default, which the image manager treats as the layer is kept
     * as packed blob only (skip): RemoveLayer and UnpackLayer are not called. Derived classes which override
     * UnpackLayer must override ValidateLayer and RemoveLayer as well.
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
     * Unpacks installed layer. Called by the image manager after the layer blob is installed if ValidateLayer fails,
     * derived classes can override it to unpack layer data. Returns eNotSupported by default, which the image manager
     * treats as nothing to do (skip).
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
     * Removes installed layer. Called by the image manager before the layer is unpacked if ValidateLayer fails, and
     * before the layer blob is removed, derived classes can override it to remove unpacked layer data. Returns
     * eNotSupported by default, which the image manager treats as nothing to do (skip).
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

    /**
     * Removes orphan unpacked layers, i.e. the unpacked layers not used by any layer blob. Called by the image manager
     * after the orphan blobs are removed, on start and every mRemoveOutdatedPeriod, derived classes which keep the
     * unpacked layers override it. Returns eNotSupported by default, which the image manager treats as nothing to do
     * (skip). The image manager frees the returned size in the space allocator, so derived classes must not do it.
     *
     * @return RetWithError<size_t>: size of the removed orphan layers, freed even if an error is returned.
     */
    virtual RetWithError<size_t> RemoveOrphanLayers() { return {0, ErrorEnum::eNotSupported}; }

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
    RetWithError<size_t> RemoveItemFromCache(const String& itemID, const String& version);
    void                 ProcessCleanup();
    Error                RemoveExpiredItems();
    Error                RemoveExpiredItem(const String& itemID, const String& version);
    bool                 IsItemExpired(const DeployableItemInfo& item) const;
    RetWithError<size_t> RemoveOrphanBlobs();
    RetWithError<size_t> RemoveOrphanBlob(const String& path, const String& digest);
    RetWithError<size_t> RemoveUnusedItemBlob(const ItemBlobInfo& blob);

    Error                ValidateBlob(const String& path, const String& digest, size_t size = 0);
    Error                GetBlobInfo(const String& digest, BlobInfo& info);
    Error                DownloadItemBlob(const InstallItemInfo& itemInfo, const String& path, const String& digest);
    RetWithError<size_t> RemoveBlob(const String& path, const String& digest);
    Error                RemoveItemBlob(
                       const String& path, const oci::ContentDescriptor& descriptor, bool isLayer = false, const String& diffID = "");
    Error InstallItemBlob(const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, String& path);
    Error InstallLayerBlob(
        const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, const String& diffID);
    Error ProcessBlob(const InstallItemInfo& itemInfo, const String& path, const oci::ContentDescriptor& descriptor);
    Error ProcessLayer(const InstallItemInfo& itemInfo, const String& path, const oci::ContentDescriptor& descriptor,
        const String& diffID);
    Error InstallItemBlobs(const InstallItemInfo& itemInfo);
    Error InstallManifestBlobs(const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor);
    Error StartItemInstall(const InstallItemInfo& itemInfo, DeployableItemInfo& storedInfo);
    Error AddStoredItem(const DeployableItemInfo& item);
    Error AddItemBlob(const String& itemID, const String& version, const oci::ContentDescriptor& descriptor,
        bool isLayer = false, const String& diffID = "");

    Error SetInProgressItemState(const String& itemID, const String& version, UpdateItemType type, ItemStateEnum state,
        const Error& error = ErrorEnum::eNone);
    Error RemoveInterruptedItem(DeployableItemInfo& item);
    void  RestoreRemovingItemState(const UpdateItemStatus& prevStatus, const Error& error);
    Error SetStoredItemState(DeployableItemInfo& item, ItemStateEnum state, const Error& error = ErrorEnum::eNone);

    void NotifyItemStatusChanged(const UpdateItemStatus& status);
    void NotifyItemRemovedFromCache(const String& itemID, const String& version);

    Error             LockItem(const String& itemID, const String& version);
    bool              TryLockItem(const String& itemID, const String& version);
    void              UnlockItem(const String& itemID, const String& version);
    UpdateItemStatus* FindInProgressItem(const String& itemID, const String& version);
    Error             LockBlob(const String& digest);
    bool              TryLockBlob(const String& digest);
    void              UnlockBlob(const String& digest);

    bool IsBlobInProgress(const String& digest) const
    {
        return mBlobsInProgress.Find(digest) != mBlobsInProgress.end();
    }

    Config                             mConfig;
    BlobInfoProviderItf*               mBlobInfoProvider {};
    StorageItf*                        mStorage {};
    spaceallocator::SpaceAllocatorItf* mSpaceAllocator {};
    fs::FileInfoProviderItf*           mFileInfoProvider {};
    oci::OCISpecItf*                   mOCISpec {};
    fs::FSPlatformItf*                 mFSPlatform {};
    AllocatorItf*                      mAllocator {};

    Mutex                                                 mListenersMutex;
    StaticArray<ItemStatusListenerItf*, cMaxNumListeners> mListeners;

    Mutex mStorageMutex;

    Mutex                                                        mMutex;
    ConditionalVariable                                          mCondVar;
    StaticList<UpdateItemStatus, cMaxNumInProgress>              mItemsInProgress;
    StaticList<StaticString<oci::cDigestLen>, cMaxNumInProgress> mBlobsInProgress;

    Timer    mTimer;
    Thread<> mThread;
    bool     mClose {};
    bool     mRunCleanup {};

    ItemRemover mItemRemover {*this};
};

} // namespace aos::common::imagemanager

#endif
