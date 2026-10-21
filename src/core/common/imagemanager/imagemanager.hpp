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
     * Initializes image manager. Derived classes call it from their own Init.
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
     * @return Error: eInvalidArgument if digest is malformed, eNotSupported if digest algorithm is not sha256,
     * eInvalidChecksum if blob checksum mismatches, the file info provider error if the blob file can't be read.
     */
    Error ValidateBlob(const String& digest, const String& path) const;

    /**
     * Installs blob: allocates space of the blob size if it is not 0, downloads and validates the blob. On failure, the
     * blob file is removed and the space is released. Derived classes must use this method to download blobs, e.g. from
     * InstallLayer.
     *
     * @param digest blob digest.
     * @param size blob size, 0 if unknown.
     * @param path destination blob path.
     * @return Error.
     */
    Error InstallBlob(const String& digest, size_t size, const String& path);

    /**
     * Removes blob. Doesn't call the space allocator: the caller frees the returned size.
     *
     * @param digest blob digest.
     * @param path blob path.
     * @return RetWithError<size_t>: freed size, 0 if the blob doesn't exist.
     */
    RetWithError<size_t> RemoveBlob(const String& digest, const String& path);

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
    static constexpr auto cBlobsFolder           = "blobs";
    static constexpr auto cSHA256DigestAlgorithm = "sha256";
    static constexpr auto cBlobHashSize          = crypto::cSHA512Size;

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

    ItemRemover mItemRemover {*this};
};

} // namespace aos::common::imagemanager

#endif
