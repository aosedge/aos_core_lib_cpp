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
class ImageManager : public ItemStatusProviderItf, public LocalBlobInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageManager() = default;

    /**
     * Initializes image manager.
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
     * Retrieves deployable items statuses.
     *
     * @param[out] statuses list of deployable items statuses.
     * @return Error.
     */
    Error GetItemsStatuses(Array<UpdateItemStatus>& statuses) override;

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
     * Installs item.
     *
     * @param itemInfo item info.
     * @return Error.
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
     * @return Error.
     */
    Error ValidateBlob(const String& digest, const String& path);

    /**
     * Downloads blob.
     *
     * @param digest blob digest.
     * @param path destination blob path.
     * @return Error.
     */
    virtual Error DownloadBlob(const String& digest, const String& path) = 0;

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

private:
    /**
     * Maximum number of stored deployable items.
     *
     * @note The maximum number of deployable items is doubled to account for multiple versions of the same item.
     */
    static constexpr auto cMaxNumStoredItems = cMaxNumUpdateItems * 2;

    /**
     * Maximum number of blobs referenced by deployable item: root index, and manifest, item config, image config and
     * layers of each manifest.
     */
    static constexpr auto cMaxNumItemBlobs = 1 + oci::cMaxNumManifests * (3 + oci::cMaxNumLayers);

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

    Config                             mConfig;
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
