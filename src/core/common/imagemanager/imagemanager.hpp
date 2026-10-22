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
     * Stops image manager. Waits until all items being installed are processed.
     *
     * @return Error.
     */
    virtual Error Stop();

    /**
     * Retrieves deployable items statuses. Reports one status per item version in the storage. An item version being
     * installed, whose eInstalling state is already notified, is reported with its last notified state (eInstalling or
     * eDownloading) until its final state is notified. Otherwise the stored state is reported. mError is set only for
     * eFailed items and is taken from the storage. mPreinstalled is always false. ePending is never reported.
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
     * @note The final eInstalled or eFailed status of InstallItem is notified while the item is still in progress, so
     * a listener must not call InstallItem or RemoveItem synchronously from the callback.
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
     * same time, plus one StaticArray<DeployableItemInfo, cMaxNumStoredItems> for GetItemsStatuses.
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
     * Removes item.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error.
     */
    Error RemoveItem(const String& itemID, const String& version);

    /**
     * Validates blob.
     *
     * @param digest blob digest.
     * @param path blob path.
     * @return Error: eInvalidArgument if digest is malformed, eNotSupported if digest algorithm is not sha256,
     * eInvalidChecksum if blob checksum mismatches, the file info provider error if the blob file can't be read.
     */
    Error ValidateBlob(const String& digest, const String& path) const;

    /**
     * Installs blob for the item. Locks the blob and stores the blob reference for the item. If the blob already
     * exists and is valid, keeps it. Otherwise removes the existing blob, allocates space of the blob size if it is
     * not 0, downloads and validates the blob. On failure, the blob file is removed and the space is released.
     * Derived classes must use this method to download blobs, e.g. from InstallLayer.
     *
     * @param itemInfo item info.
     * @param blob item blob info.
     * @param size blob size, 0 if unknown.
     * @return Error.
     */
    Error InstallBlob(const InstallItemInfo& itemInfo, const ItemBlobInfo& blob, size_t size);

    /**
     * Removes blob. Doesn't call the space allocator: the caller frees the returned size.
     *
     * @param digest blob digest.
     * @param path blob path.
     * @return RetWithError<size_t>: freed size, 0 if the blob doesn't exist.
     */
    RetWithError<size_t> RemoveBlob(const String& digest, const String& path);

    /**
     * Installs layer of the item. Called by InstallItem for every layer. The default implementation calls InstallBlob.
     * Derived classes must call InstallBlob to download the layer blob.
     *
     * @param itemInfo item info.
     * @param descriptor layer descriptor.
     * @param diffDigest diff digest, empty if item has no image config.
     * @return Error.
     */
    virtual Error InstallLayer(
        const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, const String& diffDigest);

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
     * Returns space allocator.
     *
     * @return spaceallocator::SpaceAllocatorItf&.
     */
    spaceallocator::SpaceAllocatorItf& GetSpaceAllocator() const { return *mSpaceAllocator; }

private:
    static constexpr auto cBlobsFolder           = "blobs";
    static constexpr auto cSHA256DigestAlgorithm = "sha256";
    static constexpr auto cBlobHashSize          = crypto::cSHA512Size;
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

    /**
     * Downloads blob. Called by InstallBlob, derived classes implement it but must not call it directly.
     *
     * @param digest blob digest.
     * @param path destination blob path.
     * @return Error.
     */
    virtual Error DownloadBlob(const String& digest, const String& path) = 0;

    class ItemRemover : public spaceallocator::ItemRemoverItf {
    public:
        explicit ItemRemover(ImageManager& imageManager)
            : mImageManager(imageManager)
        {
        }

        RetWithError<size_t> RemoveItem(const String& id, const String& version) override
        {
            return mImageManager.RemoveOutdatedItem(id, version);
        }

    private:
        ImageManager& mImageManager;
    };

    RetWithError<size_t> RemoveOutdatedItem(const String& itemID, const String& version);
    void                 FinishInstallBlob(const String& path, spaceallocator::SpaceItf* space, const Error& err);
    void                 NotifyItemStatusChanged(const UpdateItemStatus& status);
    void                 NotifyItemRemovedFromCache(const String& itemID, const String& version);

    static UpdateItemStatus CreateItemStatus(const String& itemID, UpdateItemType type, const String& version,
        ItemStateEnum state, const Error& error = ErrorEnum::eNone);

    Error           LockItem(const String& itemID, const String& version, UpdateItemType type);
    void            UnlockItem(const String& itemID, const String& version);
    ItemInProgress* FindItemInProgress(const String& itemID, const String& version);
    Error           LockBlob(const String& digest, const String& itemID, const String& version);
    bool            TryLockBlob(const String& digest);
    void            UnlockBlob(const String& digest);
    BlobInProgress* FindBlobInProgress(const String& digest);
    Error           StartItemInstall(const InstallItemInfo& itemInfo, DeployableItemInfo& item, bool& wasRemoved);
    Error           InstallItemBlobs(const InstallItemInfo& itemInfo);
    Error           InstallManifestBlobs(
                  const InstallItemInfo& itemInfo, const String& digest, const String& mediaType, size_t size);
    Error AddItemBlob(const InstallItemInfo& itemInfo, const ItemBlobInfo& blob);
    Error SetItemState(DeployableItemInfo& item, ItemStateEnum state, const Error& error = ErrorEnum::eNone);

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
