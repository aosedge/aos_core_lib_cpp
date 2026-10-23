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
#include <core/common/tools/array.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/noncopyable.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>

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
     * Starts image manager. Sets the stored items in the eInstalling state, whose installation was interrupted, to
     * eRemoved with the current timestamp, keeping their blobs and blob references. Then removes the orphan blobs, the
     * blobs not used by any stored item and the files of the blobs folder that don't match a valid digest, and calls
     * RemoveOrphanLayers. Then registers all stored items in the eRemoved state as outdated in the space allocator,
     * removes the expired items, see RemoveExpiredItems, and starts the periodic cleanup, which removes the expired
     * items, then the orphan blobs, and calls RemoveOrphanLayers every mRemoveOutdatedPeriod. The expired items are
     * never removed if mItemCacheTTL is 0. The periodic cleanup is not started if mRemoveOutdatedPeriod is 0. The
     * errors of removing a single blob or item or of registering a single item are logged: the blob or the item is
     * removed by the next cleanup, the item is registered on the next start. No item status is notified, only
     * OnItemRemovedFromCache for each removed expired item. InstallItem and RemoveItem fail with eWrongState until
     * Start succeeds.
     *
     * @note Start must not be called concurrently with Start or Stop.
     *
     * @return Error: eWrongState if the image manager is already started, eInvalidArgument if mRemoveOutdatedPeriod is
     * not 0 and is not greater than the timer resolution, storage and timer errors as is.
     */
    virtual Error Start();

    /**
     * Stops image manager. Stops the periodic cleanup, waiting for the cleanup in progress, and waits until all
     * InstallItem and RemoveItem calls in progress return. The calls waiting for the same item version, for a free slot
     * of the items in progress or for a blob being processed by another item fail with eWrongState. InstallItem and
     * RemoveItem called after Stop fail with eWrongState until Start is called again. Does nothing if the image manager
     * is not started.
     *
     * @return Error.
     */
    virtual Error Stop();

    /**
     * Retrieves deployable items statuses. Reports one status per item version in the storage. An item version being
     * installed, whose eInstalling state is already notified, is reported with its last notified state (eInstalling or
     * eDownloading) until its final state is notified. An item version being removed, whose eRemoving state is already
     * notified, is reported as eRemoving until its eRemoved state is set in the storage or the storage update fails.
     * Otherwise the stored state is reported, except a stored eInstalling state, which is reported as eFailed without
     * error. mError is set only for eFailed items and is taken from the storage. mPreinstalled is always false.
     * ePending is never reported.
     *
     * @note The statuses are appended to the output array, which is not cleared.
     *
     * @param[out] statuses list of deployable items statuses.
     * @return Error: eNoMemory if the statuses don't fit the output array, storage errors as is.
     */
    Error GetItemsStatuses(Array<UpdateItemStatus>& statuses) override;

    /**
     * Subscribes status notifications. Can be called at any time, including from a listener callback.
     *
     * @param listener status listener.
     * @return Error: eAlreadyExist if the listener is already subscribed.
     */
    Error SubscribeListener(ItemStatusListenerItf& listener) override;

    /**
     * Unsubscribes from status notifications. Can be called at any time, including from a listener callback.
     *
     * @note A notification already in progress on another thread may still call the listener after
     * UnsubscribeListener returns, so the listener must stay alive until the image manager is stopped or until the
     * notification in progress returns.
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
     * Initializes image manager. Derived classes call it from their own Init.
     *
     * @note The allocator must provide, per concurrent InstallItem (up to cMaxNumConcurrentItems), one
     * oci::ImageIndex, one oci::ImageManifest and one oci::ImageConfig at the same time, plus one
     * StaticArray<DeployableItemInfo, cMaxNumStoredItems> for GetItemsStatuses, one
     * StaticArray<DeployableItemInfo, cMaxNumStoredItems> for Start, which is also used by the expired items removal on
     * start, one StaticArray<DeployableItemInfo, cMaxNumStoredItems> for the periodic cleanup, one
     * StaticArray<ItemBlobInfo, cMaxNumItemBlobs> for the item remover and one
     * StaticArray<ItemBlobInfo, cMaxNumItemBlobs> for the expired items removal.
     *
     * @param config image manager config.
     * @param storage image manager storage.
     * @param spaceAllocator space allocator.
     * @param fileInfoProvider file info provider.
     * @param ociSpec OCI spec interface.
     * @param fsPlatform file system platform interface.
     * @param allocator allocator for temporary objects.
     * @return Error.
     */
    Error Init(const Config& config, StorageItf& storage, spaceallocator::SpaceAllocatorItf& spaceAllocator,
        fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec, fs::FSPlatformItf& fsPlatform,
        AllocatorItf& allocator);

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
     * Installs item. Waits while the same item version is being installed or removed, or while
     * cMaxNumConcurrentItems items are being processed.
     *
     * @note The final eInstalled or eFailed status is notified while the item is still in progress, so a listener
     * must not call InstallItem, RemoveItem or Stop synchronously from the callback.
     *
     * @param itemInfo item info.
     * @return Error: eWrongState if the image manager is not started or is stopped while waiting, eInvalidArgument if
     * the root digest is malformed, the item version is stored with a different root digest, the image config of a
     * service is missing or a diff ID is missing in the image config, eNoMemory if the storage is full and there is no
     * outdated item to remove, other errors as is.
     */
    Error InstallItem(const InstallItemInfo& itemInfo);

    /**
     * Removes item. Sets the item state to eRemoved in the storage, keeping the item blobs, and adds the item as
     * outdated to the space allocator. If the item is already removed, adds it as outdated to the space allocator again
     * with its stored timestamp, without notifications and storage update, so an item whose previous registration
     * failed is registered. Waits while the same item version is being installed or removed, or while
     * cMaxNumConcurrentItems items are being processed.
     *
     * @note The eRemoving status and the previous item status on the storage update failure are notified while the
     * item is still in progress, so a listener must not call InstallItem, RemoveItem or Stop synchronously from the
     * callback.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error: eWrongState if the image manager is not started or is stopped while waiting, eNotFound if the
     * item version is not stored, other errors as is.
     */
    Error RemoveItem(const String& itemID, const String& version);

    /**
     * Validates blob.
     *
     * @param digest blob digest.
     * @param path blob path.
     * @return Error: eInvalidArgument if digest is malformed, eNotSupported if digest algorithm is not sha256,
     * eNotFound if blob file doesn't exist, eInvalidChecksum if blob checksum mismatches, other errors as is.
     */
    Error ValidateBlob(const String& digest, const String& path) const;

    /**
     * Downloads blob. Notifies eDownloading state of the item being installed once per InstallItem before the first
     * download, then calls DoDownloadBlob. Derived classes must use this method (not DoDownloadBlob) to download blobs,
     * e.g. from InstallLayer.
     *
     * @param digest blob digest.
     * @param path destination blob path.
     * @return Error.
     */
    Error DownloadBlob(const String& digest, const String& path);

    /**
     * Downloads blob implementation.
     *
     * @param digest blob digest.
     * @param path destination blob path.
     * @return Error.
     */
    virtual Error DoDownloadBlob(const String& digest, const String& path) = 0;

    /**
     * Installs layer.
     *
     * @param descriptor layer descriptor.
     * @param diffDigest diff digest, empty if item has no image config.
     * @param path layer blob path.
     * @return Error.
     */
    virtual Error InstallLayer(const oci::ContentDescriptor& descriptor, const String& diffDigest, const String& path);

    /**
     * Removes layer.
     *
     * @param digest layer blob digest.
     * @param mediaType layer media type.
     * @param diffID layer diff ID, empty if item has no image config.
     * @param path layer blob path.
     * @return RetWithError<size_t>: freed size.
     */
    virtual RetWithError<size_t> RemoveLayer(
        const String& digest, const String& mediaType, const String& diffID, const String& path);

    /**
     * Validates layer.
     *
     * @param descriptor layer descriptor.
     * @param diffDigest diff digest, empty if item has no image config.
     * @param path layer blob path.
     * @return Error.
     */
    virtual Error ValidateLayer(const oci::ContentDescriptor& descriptor, const String& diffDigest, const String& path);

    /**
     * Removes orphan layers. Called after the orphan blobs are removed: on start, when no item is being installed or
     * removed, and in each periodic cleanup, when items can be installed or removed at the same time. Never called
     * concurrently with itself. The default implementation does nothing. The SM image manager overrides it to remove
     * the unpacked layers not used by any stored item, see IsLayerUsed of the SM storage, skipping the layers whose
     * diff ID is being processed. Called without the image manager mutex locked, so it may call the space allocator,
     * e.g. to free the space of the removed layers with FreeSpace. An error is logged and doesn't fail the caller.
     *
     * @return Error.
     */
    virtual Error RemoveOrphanLayers();

    /**
     * Returns space allocator.
     *
     * @return spaceallocator::SpaceAllocatorItf&.
     */
    spaceallocator::SpaceAllocatorItf& GetSpaceAllocator() const { return *mSpaceAllocator; }

    /**
     * Notifies subscribed listeners about item status change. The listeners are copied under the listeners mutex and
     * called without any lock held, so a listener may subscribe or unsubscribe listeners from the callback. A listener
     * unsubscribed during the notification may still receive the notification in progress. Must be called without the
     * image manager mutex locked.
     *
     * @note The final eInstalled or eFailed status of InstallItem and the eRemoving status of RemoveItem are notified
     * while the item is still in progress, so a listener must not call InstallItem, RemoveItem or Stop synchronously
     * from the callback.
     *
     * @param status item status.
     */
    void NotifyItemStatusChanged(const UpdateItemStatus& status);

    /**
     * Notifies subscribed listeners about item removal from cache. The listeners are copied under the listeners mutex
     * and called without any lock held, so a listener may subscribe or unsubscribe listeners from the callback. A
     * listener unsubscribed during the notification may still receive the notification in progress. Must be called
     * without the image manager mutex locked.
     *
     * @note It is called from ItemRemoverItf::RemoveItem with the space allocator lock held, so a listener must not
     * call the image manager or the space allocator from the callback.
     *
     * @param itemID item ID.
     * @param version item version.
     */
    void NotifyItemRemovedFromCache(const String& itemID, const String& version);

private:
    static constexpr auto cBlobsFolder           = "blobs";
    static constexpr auto cSHA256DigestAlgorithm = "sha256";
    static constexpr auto cMaxNumListeners       = 4;
    static constexpr auto cMaxNumBlobsInProgress = cMaxNumConcurrentItems + 2;

    struct ItemInProgress {
        StaticString<cIDLen>      mItemID;
        StaticString<cVersionLen> mVersion;
        UpdateItemType            mType;
        bool                      mNotified {};
        ItemStateEnum             mState {};
    };

    struct BlobInProgress {
        StaticString<oci::cDigestLen> mDigest;
        StaticString<cIDLen>          mItemID;
        StaticString<cVersionLen>     mVersion;
    };

    class ItemRemover : public spaceallocator::ItemRemoverItf {
    public:
        explicit ItemRemover(ImageManager& imageManager)
            : mImageManager(imageManager)
        {
        }

        RetWithError<size_t> RemoveItem(const String& id, const String& version) override
        {
            return mImageManager.RemoveItemFromCache(id, version);
        }

    private:
        ImageManager& mImageManager;
    };

    /**
     * Removes outdated item and its unused blobs from the cache. Does nothing if the item is not stored, is not in the
     * eRemoved state or is in progress. Called by the space allocator with its lock held, so it never calls the space
     * allocator and never waits for the blobs being processed.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return RetWithError<size_t>: freed size, storage errors as is.
     */
    RetWithError<size_t> RemoveItemFromCache(const String& itemID, const String& version);

    /**
     * Removes stored item in the eRemoved state and its blob references from the storage, keeping the blob files. Must
     * be called with mMutex locked.
     *
     * @param itemID item ID.
     * @param version item version.
     * @param[out] blobs blobs referenced by the removed item.
     * @return Error: eNotFound if the item is not stored, eWrongState if the item is not in the eRemoved state, storage
     * errors as is.
     */
    Error RemoveStoredItem(const String& itemID, const String& version, Array<ItemBlobInfo>& blobs);

    /**
     * Removes expired items: the stored items in the eRemoved state whose timestamp is older than mItemCacheTTL. Does
     * nothing if mItemCacheTTL is 0. Stops when the image manager is stopped. Errors are logged, and the next item is
     * processed. Must be called while the caller is registered as a call in progress, see EnterCall.
     *
     * @param items buffer for the stored items, cleared before use.
     */
    void RemoveExpiredItems(Array<DeployableItemInfo>& items);

    /**
     * Removes expired item: skips it if it is in progress, otherwise re-reads it from the storage and skips it if it
     * is no longer stored in the eRemoved state or is no longer expired, e.g. it was installed and removed again after
     * the items were read. Otherwise removes it from the outdated items of the space allocator, removes it and its
     * unused blobs from the cache, frees the space of the removed blobs and notifies OnItemRemovedFromCache. Skips the
     * item if it is no longer stored in the eRemoved state. If the item can't be removed from the storage, adds it as
     * outdated to the space allocator again with its stored timestamp. Errors are logged.
     *
     * @param item expired item.
     * @param blobs buffer for the item blobs.
     */
    void RemoveExpiredItem(const DeployableItemInfo& item, Array<ItemBlobInfo>& blobs);

    /**
     * Removes item blob if it is not processed by another item and not used by any stored item. Errors are logged.
     *
     * @param blob item blob info.
     * @return size_t: freed size.
     */
    size_t RemoveItemBlob(const ItemBlobInfo& blob);

    /**
     * Removes blob file.
     *
     * @param path blob path.
     * @return RetWithError<size_t>: freed size, 0 if the file doesn't exist.
     */
    RetWithError<size_t> RemoveBlobFile(const String& path);

    /**
     * Removes orphan blobs: iterates the blobs folder <mImagePath>/blobs/<algorithm>/<hash> and removes the entries
     * that don't match a valid blob digest and the blobs not used by any stored item. On start, when no item is being
     * installed, the blobs are removed without locking them and the space allocator is not called. Otherwise, each
     * blob is removed as an item blob (see RemoveItemBlob) and its space is freed with FreeSpace of the space
     * allocator. Errors are logged, and the next entry is processed.
     *
     * @param onStart true if called from Start, false if called from the periodic cleanup.
     */
    void RemoveOrphanBlobs(bool onStart);

    /**
     * Removes blob file if the blob is not used by any stored item, without locking the blob and without calling the
     * space allocator. Errors are logged.
     *
     * @param digest blob digest.
     * @param path blob path.
     */
    void RemoveUnusedBlob(const String& digest, const String& path);

    /**
     * Removes entry of the blobs folder that doesn't match a valid blob digest. Errors are logged.
     *
     * @param path entry path.
     */
    void RemoveInvalidBlobEntry(const String& path);

    /**
     * Periodic cleanup timer callback. Removes the expired items, see RemoveExpiredItems, then the orphan blobs, and
     * calls RemoveOrphanLayers. The expired items are removed before the orphan blobs, so that the blobs of the removed
     * expired items that are left, e.g. after a failed blob removal, are removed in the same cleanup. Does nothing if
     * the image manager is stopped or the previous cleanup is still in progress.
     */
    void PeriodicCleanup();

    /**
     * Waits until all calls in progress return.
     *
     * @return Error.
     */
    Error WaitActiveCalls();

    /**
     * Registers call in progress (InstallItem, RemoveItem or the periodic cleanup), so Stop waits until it returns.
     *
     * @return Error: eWrongState if the image manager is not started.
     */
    Error EnterCall();

    /**
     * Unregisters call in progress and wakes up the waiting threads.
     */
    void LeaveCall();

    static UpdateItemStatus CreateItemStatus(const String& itemID, UpdateItemType type, const String& version,
        ItemStateEnum state, const Error& error = ErrorEnum::eNone);

    /**
     * Adds item to the list of the items in progress. Waits while the same item version is in the list or the list is
     * full.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error: eWrongState if the image manager is stopped.
     */
    Error AcquireItem(const String& itemID, const String& version);

    /**
     * Adds item to the list of the items in progress without waiting.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return bool: false if the image manager is stopped, the item version is already in the list or the list is
     * full.
     */
    bool TryAcquireItem(const String& itemID, const String& version);

    /**
     * Removes item from the list of the items in progress and wakes up the waiting threads.
     *
     * @param itemID item ID.
     * @param version item version.
     */
    void ReleaseItem(const String& itemID, const String& version);

    /**
     * Finds item in the list of the items in progress. Must be called with mMutex locked.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return ItemInProgress* item or nullptr if not found.
     */
    ItemInProgress* FindItemInProgress(const String& itemID, const String& version);

    /**
     * Adds blob to the list of the blobs being processed. Waits while the blob is in the list or the list is full.
     *
     * @param digest blob digest.
     * @param itemID ID of the item that processes the blob.
     * @param version version of the item that processes the blob.
     * @return Error: eWrongState if the image manager is stopped while waiting.
     */
    Error LockBlob(const String& digest, const String& itemID, const String& version);

    /**
     * Adds blob to the list of the blobs being processed without waiting. Used by the item blobs removal, which must
     * not wait for the blobs being processed (see ItemRemoverItf::RemoveItem).
     *
     * @param digest blob digest.
     * @return bool: false if the blob is already in the list or the list is full.
     */
    bool TryLockBlob(const String& digest);

    /**
     * Removes blob from the list of the blobs being processed and wakes up the waiting threads.
     *
     * @param digest blob digest.
     */
    void UnlockBlob(const String& digest);

    /**
     * Finds blob in the list of the blobs being processed. Must be called with mMutex locked.
     *
     * @param digest blob digest.
     * @return BlobInProgress* blob or nullptr if not found.
     */
    BlobInProgress* FindBlobInProgress(const String& digest);

    Error StartItemInstall(const InstallItemInfo& itemInfo, DeployableItemInfo& item, bool& wasRemoved);
    Error InstallItemBlobs(const InstallItemInfo& itemInfo, bool wasRemoved);
    Error InstallManifest(const InstallItemInfo& itemInfo, const String& digest, const String& mediaType, size_t size);
    Error InstallMetadataBlob(
        const InstallItemInfo& itemInfo, const String& digest, const String& mediaType, size_t size, String& path);
    Error InstallItemLayer(
        const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, const String& diffID);
    Error AddItemBlob(const InstallItemInfo& itemInfo, const ItemBlobInfo& blob);
    Error SetItemState(DeployableItemInfo& item, ItemStateEnum state, const Error& error = ErrorEnum::eNone);

    /**
     * Splits digest into algorithm and hash.
     *
     * @param digest digest in format <algorithm>:<hash>.
     * @param[out] algorithm digest algorithm.
     * @param[out] hash digest hash.
     * @return Error: eInvalidArgument if digest is malformed.
     */
    static Error ParseDigest(const String& digest, String& algorithm, String& hash);

    /**
     * Checks whether character is lowercase alphanumeric.
     *
     * @param ch character.
     * @return bool.
     */
    static bool IsLowerAlnum(char ch);

    /**
     * Checks whether digest algorithm matches OCI image spec digest grammar.
     *
     * @param algorithm digest algorithm.
     * @return bool.
     */
    static bool IsValidDigestAlgorithm(const String& algorithm);

    /**
     * Creates digest from algorithm and hash and validates it.
     *
     * @param algorithm digest algorithm.
     * @param hash digest hash.
     * @param[out] digest digest in format <algorithm>:<hash>.
     * @return Error: eInvalidArgument if digest is malformed or doesn't fit the output string.
     */
    static Error CreateDigest(const String& algorithm, const String& hash, String& digest);

    Config                             mConfig;
    StorageItf*                        mStorage {};
    spaceallocator::SpaceAllocatorItf* mSpaceAllocator {};
    fs::FileInfoProviderItf*           mFileInfoProvider {};
    oci::OCISpecItf*                   mOCISpec {};
    fs::FSPlatformItf*                 mFSPlatform {};
    AllocatorItf*                      mAllocator {};

    Mutex                                                 mListenersMutex;
    StaticArray<ItemStatusListenerItf*, cMaxNumListeners> mListeners;

    Mutex                                               mMutex;
    ConditionalVariable                                 mCondVar;
    StaticArray<ItemInProgress, cMaxNumConcurrentItems> mItemsInProgress;
    StaticArray<BlobInProgress, cMaxNumBlobsInProgress> mBlobsInProgress;
    bool                                                mStarted {};
    bool                                                mCleanupInProgress {};
    size_t                                              mNumActiveCalls {};

    ItemRemover mItemRemover {*this};
    Timer       mTimer;
};

} // namespace aos::common::imagemanager

#endif
