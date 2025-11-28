/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_IMAGEMANAGER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_IMAGEMANAGER_HPP_

#include <core/cm/fileserver/itf/fileserver.hpp>
#include <core/cm/launcher/itf/imageinfoprovider.hpp>
#include <core/cm/smcontroller/itf/updateimageprovider.hpp>
#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/spaceallocator/spaceallocator.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/identifierpool.hpp>
#include <core/common/tools/timer.hpp>

#include "config.hpp"
#include "itf/imagemanager.hpp"
#include "itf/imageunpacker.hpp"
#include "itf/storage.hpp"

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * GID range start.
 */
static constexpr auto cGIDRangeBegin = 5000;

/**
 * GID range end.
 */
static constexpr auto cGIDRangeEnd = 10000;

/**
 * Max number of locked IDs simultaneously.
 */
static constexpr auto cMaxNumLockedGIDs = cMaxNumInstances;

/**
 * Group ID pool
 */
using GIDPool = IdentifierRangePool<cGIDRangeBegin, cGIDRangeEnd, cMaxNumLockedGIDs>;

/**
 * Image manager.
 */
class ImageManager : public ImageManagerItf,
                     public smcontroller::UpdateImageProviderItf,
                     public launcher::ImageInfoProviderItf,
                     public spaceallocator::ItemRemoverItf {
public:
    /**
     * Initializes image manager.
     *
     * @param config image manager config.
     * @param storage image storage.
     * @param spaceAllocator space allocator.
     * @param fileserver file server.
     * @param imageDecrypter image decrypter.
     * @param fileInfoProvider file info provider.
     * @param gidValidator GID validator function.
     * @return Error.
     */
    Error Init(const Config& config, storage::StorageItf& storage, spaceallocator::SpaceAllocatorItf& spaceAllocator,
        spaceallocator::SpaceAllocatorItf& tmpSpaceAllocator, fileserver::FileServerItf& fileserver,
        crypto::CryptoHelperItf& imageDecrypter, fs::FileInfoProviderItf& fileInfoProvider,
        ImageUnpackerItf& imageUnpacker, oci::OCISpecItf& ociSpec, GIDPool::Validator gidValidator);

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
     *
     * Installs update item.
     *
     * @param itemInfo update item info.
     * @param certificates list of certificates.
     * @param certificateChains list of certificate chains.
     * @param[out] status update item status.
     * @return Error.
     */
    Error InstallUpdateItems(const Array<UpdateItemInfo>& itemsInfo, const Array<crypto::CertificateInfo>& certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, Array<UpdateItemStatus>& statuses) override;

    /**
     * Uninstalls update item.
     *
     * @param ids update item ID's.
     * @param[out] statuses update items statuses.
     * @return Error.
     */
    Error UninstallUpdateItems(const Array<StaticString<cIDLen>>& ids, Array<UpdateItemStatus>& statuses) override;

    /**
     * Reverts update item.
     *
     * @param ids update item ID's.
     * @param[out] statuses update items statuses.
     * @return Error.
     */
    Error RevertUpdateItems(const Array<StaticString<cIDLen>>& ids, Array<UpdateItemStatus>& statuses) override;

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
    Error SubscribeListener(ImageStatusListenerItf& listener) override;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    Error UnsubscribeListener(ImageStatusListenerItf& listener) override;

    /**
     * Returns update image info for desired platform.
     *
     * @param id update item ID.
     * @param platform platform information.
     * @param[out] info update image info.
     * @return Error.
     */
    Error GetUpdateImageInfo(
        const String& id, const PlatformInfo& platform, smcontroller::UpdateImageInfo& info) override;

    /**
     * Returns update image info by image ID.
     *
     * @param itemID update item ID.
     * @param imageID image ID.
     * @param[out] info update image info.
     * @return Error.
     */
    Error GetUpdateImageInfo(const String& itemID, const String& imageID, smcontroller::UpdateImageInfo& info) override;

    /**
     * Returns layer image info.
     *
     * @param digest layer digest.
     * @param info layer image info.
     * @return Error.
     */
    Error GetLayerImageInfo(const String& digest, smcontroller::UpdateImageInfo& info) override;

    /**
     * Returns update item version.
     *
     * @param id update item ID.
     * @return RetWithError<StaticString<cVersionLen>>.
     */
    RetWithError<StaticString<cVersionLen>> GetItemVersion(const String& id) override;

    /**
     * Returns update item images infos.
     *
     * @param id update item ID.
     * @param[out] imagesInfos update item images info.
     * @return Error.
     */
    Error GetItemImages(const String& id, Array<ImageInfo>& imagesInfos) override;

    /**
     * Returns service config.
     *
     * @param id update item ID.
     * @param imageID image ID.
     * @param config service config.
     * @return Error.
     */
    Error GetServiceConfig(const String& id, const String& imageID, oci::ServiceConfig& config) override;

    /**
     * Returns image config.
     *
     * @param id update item ID.
     * @param imageID image ID.
     * @param config image config.
     * @return Error.
     */
    Error GetImageConfig(const String& id, const String& imageID, oci::ImageConfig& config) override;

    /**
     * Returns GID for specified update item.
     *
     * @param id update item ID.
     * @return RetWithError<gid_t>.
     */
    virtual RetWithError<gid_t> GetServiceGID(const String& id) override;

    /**
     * Removes item.
     *
     * @param id item id.
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> RemoveItem(const String& id) override;

private:
    static constexpr auto   cNumActionThreads  = AOS_CONFIG_IMAGEMANAGER_NUM_COOPERATE_ACTIONS;
    static constexpr auto   cRemovePeriod      = 24 * Time::cHours;
    static constexpr auto   cMaxNumListeners   = 1;
    static constexpr size_t cMaxItemVersions   = cMaxNumUpdateItems;
    static constexpr size_t cMaxNumItems       = cMaxNumUpdateItems * 2;
    static constexpr auto   cLayerMetadataFile = "layer.json";
    static constexpr auto   cManifestFile      = "manifest.json";

    Error InstallUpdateItem(const UpdateItemInfo& itemInfo, const Array<crypto::CertificateInfo>& certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, UpdateItemStatus& status);
    Error ValidateActiveVersionItem(const UpdateItemInfo& itemInfo, const Array<storage::ItemInfo>& items);
    Error ValidateCachedVersionItem(const UpdateItemInfo& itemInfo, const Array<storage::ItemInfo>& items);
    Error SetState(const storage::ItemInfo& item, storage::ItemState state);
    Error Remove(const storage::ItemInfo& item);
    void  ReleaseAllocatedSpace(const String& path, spaceallocator::SpaceItf* space);
    void  AcceptAllocatedSpace(spaceallocator::SpaceItf* space);
    Error InstallImage(const UpdateImageInfo& imageInfo, const UpdateItemType& itemType, const String& installPath,
        const String& itemPath, storage::ImageInfo& image, const Array<crypto::CertificateChainInfo>& certificateChains,
        const Array<crypto::CertificateInfo>& certificates);
    Error DecryptAndValidate(const UpdateImageInfo& imageInfo, const String& installPath,
        const Array<crypto::CertificateChainInfo>& certificateChains,
        const Array<crypto::CertificateInfo>& certificates, StaticString<cFilePathLen>& outDecryptedFile);
    Error PrepareURLsAndFileInfo(const String& itemPath, const String& decryptedFile, const UpdateImageInfo& imageInfo,
        storage::ImageInfo& image);
    Error UpdatePrevVersions(const Array<storage::ItemInfo>& items);
    Error UninstallUpdateItem(const String& id, UpdateItemStatus& status);
    Error RevertUpdateItem(const String& id, Array<UpdateItemStatus>& statuses);
    Error SetItemStatus(
        const Array<storage::ImageInfo>& itemImages, UpdateItemStatus& status, ImageState state, Error error);
    Error              SetOutdatedItems(const Array<storage::ItemInfo>& items);
    Error              RemoveOutdatedItems();
    Error              CleanupOrphanedItems(const Array<storage::ItemInfo>& items);
    Error              CleanupOrphanedDirectories(const Array<storage::ItemInfo>& items);
    Error              CleanupOrphanedDatabaseItems(const Array<storage::ItemInfo>& items);
    RetWithError<bool> VerifyItemIntegrity(const storage::ItemInfo& item);
    void               NotifyItemRemovedListeners(const String& id);
    void NotifyImageStatusChangedListeners(const String& itemID, const String& version, const ImageStatus& status);

    Error PrepareLayerMetadata(storage::ImageInfo& image, const String& decryptedFile, const String& tmpPath);
    Error PrepareServiceMetadata(storage::ImageInfo& image, const String& decryptedFile, const String& tmpPath);
    Error ReadManifestFromTar(const String& decryptedFile, oci::ImageManifest& manifest, const String& tmpPath);
    Error ReadDigestFromTar(
        const String& decryptedFile, const String& inputDigest, storage::ImageInfo& image, const String& tmpPath);

    storage::StorageItf*               mStorage {};
    spaceallocator::SpaceAllocatorItf* mSpaceAllocator {};
    spaceallocator::SpaceAllocatorItf* mTmpSpaceAllocator {};
    ImageUnpackerItf*                  mImageUnpacker {};
    fileserver::FileServerItf*         mFileServer {};
    crypto::CryptoHelperItf*           mImageDecrypter {};
    fs::FileInfoProviderItf*           mFileInfoProvider {};
    oci::OCISpecItf*                   mOCISpec {};
    GIDPool                            mGIDPool;

    StaticArray<ImageStatusListenerItf*, cMaxNumListeners> mListeners;

    Timer  mTimer;
    Config mConfig {};

    StaticAllocator<(sizeof(StaticArray<storage::ItemInfo, cMaxNumItems>) + sizeof(storage::ItemInfo)
                        + sizeof(oci::ImageManifest) + sizeof(oci::ImageSpec))
            * cNumActionThreads,
        cNumActionThreads * 3>
        mAllocator;

    ThreadPool<cNumActionThreads, cMaxNumItems> mActionPool;
    Mutex                                       mMutex;
};

} // namespace aos::cm::imagemanager

#endif
