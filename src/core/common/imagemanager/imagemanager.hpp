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
     * Stops image manager. Waits until all InstallItem and RemoveItem calls in progress return.
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
     * StaticArray<DeployableItemInfo, cMaxNumStoredItems> for GetItemsStatuses and one
     * StaticArray<ItemBlobInfo, cMaxNumItemBlobs> for the item remover.
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
     * must not call InstallItem or RemoveItem synchronously from the callback.
     *
     * @param itemInfo item info.
     * @return Error: eInvalidArgument if the root digest is malformed, the item version is stored with a different
     * root digest, the image config of a service is missing or a diff ID is missing in the image config, eNoMemory if
     * the storage is full and there is no outdated item to remove, other errors as is.
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
     * item is still in progress, so a listener must not call InstallItem or RemoveItem synchronously from the callback.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error: eNotFound if the item version is not stored, other errors as is.
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
     * while the item is still in progress, so a listener must not call InstallItem or RemoveItem synchronously from the
     * callback.
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

    static UpdateItemStatus CreateItemStatus(const String& itemID, UpdateItemType type, const String& version,
        ItemStateEnum state, const Error& error = ErrorEnum::eNone);

    /**
     * Adds item to the list of the items in progress. Waits while the same item version is in the list or the list is
     * full.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error.
     */
    Error AcquireItem(const String& itemID, const String& version);

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
     * @return Error.
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

    ItemRemover mItemRemover {*this};
};

} // namespace aos::common::imagemanager

#endif
