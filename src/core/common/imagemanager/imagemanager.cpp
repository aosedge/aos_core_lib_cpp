/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "imagemanager.hpp"

namespace aos::common::imagemanager {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

Error ParseDigest(const String& digest, String& algorithm, String& hash)
{
    auto [pos, err] = digest.FindSubstr(0, ":");
    if (!err.IsNone() || pos == 0 || pos + 1 >= digest.Size()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
    }

    if (err = algorithm.Assign(String(digest.CStr(), pos)); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
    }

    if (err = hash.Assign(String(digest.CStr() + pos + 1, digest.Size() - pos - 1)); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
    }

    const auto isLowerAlnum = [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'); };

    if (!isLowerAlnum(algorithm[0])) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
    }

    for (const auto ch : algorithm) {
        if (!(isLowerAlnum(ch) || ch == '+' || ch == '.' || ch == '_' || ch == '-')) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
        }
    }

    for (const auto ch : hash) {
        if (!isLowerAlnum(ch)) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
        }
    }

    return ErrorEnum::eNone;
}

RetWithError<crypto::Hash> GetSupportedHash(const String& hashName)
{
    StaticString<crypto::cAlgLen> upperHash = hashName;

    (void)upperHash.ToUpper();

    if (upperHash == "SHA256") {
        return {crypto::HashEnum::eSHA256, ErrorEnum::eNone};
    } else if (upperHash == "SHA512") {
        return {crypto::HashEnum::eSHA512, ErrorEnum::eNone};
    } else {
        return {{}, AOS_ERROR_WRAP(Error(ErrorEnum::eNotSupported, "unsupported hashing algorithm"))};
    }
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ImageManager::Start()
{
    LOG_DBG() << "Start image manager";

    auto items = MakeUnique<StaticArray<DeployableItemInfo, cMaxNumStoredItems>>(mAllocator);
    if (!items) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetAllDeployableItems(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (auto& item : *items) {
        if (item.mState == ItemStateEnum::eInstalling) {
            if (auto err = RemoveInterruptedItem(item); !err.IsNone()) {
                return err;
            }
        }

        if (item.mState != ItemStateEnum::eRemoved) {
            continue;
        }

        if (auto err = mSpaceAllocator->AddOutdatedItem(item.mItemID, item.mVersion, item.mTimestamp); !err.IsNone()) {
            LOG_ERR() << "Failed to add outdated item" << Log::Field("itemID", item.mItemID)
                      << Log::Field("version", item.mVersion) << Log::Field(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::Stop()
{
    LOG_DBG() << "Stop image manager";

    return ErrorEnum::eNotSupported;
}

Error ImageManager::GetItemsStatuses(Array<UpdateItemStatus>& statuses)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get items statuses";

    auto items = MakeUnique<StaticArray<DeployableItemInfo, cMaxNumStoredItems>>(mAllocator);
    if (!items) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetAllDeployableItems(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        UpdateItemStatus status = item;

        if (const auto* itemInProgress = FindInProgressItem(item.mItemID, item.mVersion);
            itemInProgress != nullptr && itemInProgress->mState != ItemStateEnum::eUnknown) {
            status.mState = itemInProgress->mState;
            status.mError = itemInProgress->mError;
        }

        if (auto err = statuses.PushBack(status); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SubscribeListener(ItemStatusListenerItf& listener)
{
    LockGuard lock {mListenersMutex};

    LOG_DBG() << "Subscribe listener";

    if (mListeners.Contains(&listener)) {
        return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
    }

    if (auto err = mListeners.PushBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::UnsubscribeListener(ItemStatusListenerItf& listener)
{
    LockGuard lock {mListenersMutex};

    LOG_DBG() << "Unsubscribe listener";

    if (mListeners.Remove(&listener) == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetBlobPath(const String& digest, String& path) const
{
    LOG_DBG() << "Get blob path" << Log::Field("digest", digest);

    StaticString<oci::cDigestLen> algorithm;
    StaticString<oci::cDigestLen> hash;

    if (auto err = ParseDigest(digest, algorithm, hash); !err.IsNone()) {
        return err;
    }

    if (auto err = path.Assign(fs::JoinPath(mConfig.mImagePath, cBlobsFolder, algorithm, hash)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetLayerPath(const String& diffID, String& path) const
{
    (void)diffID;
    (void)path;

    return ErrorEnum::eNotSupported;
}

/***********************************************************************************************************************
 * Protected
 **********************************************************************************************************************/

Error ImageManager::Init(const Config& config, BlobInfoProviderItf& blobInfoProvider, StorageItf& storage,
    spaceallocator::SpaceAllocatorItf& spaceAllocator, fs::FileInfoProviderItf& fileInfoProvider,
    oci::OCISpecItf& ociSpec, fs::FSPlatformItf& fsPlatform, AllocatorItf& allocator)
{
    LOG_DBG() << "Init image manager";

    mConfig           = config;
    mBlobInfoProvider = &blobInfoProvider;
    mStorage          = &storage;
    mSpaceAllocator   = &spaceAllocator;
    mFileInfoProvider = &fileInfoProvider;
    mOCISpec          = &ociSpec;
    mFSPlatform       = &fsPlatform;
    mAllocator        = &allocator;

    LOG_DBG() << "Config" << Log::Field("imagePath", mConfig.mImagePath) << Log::Field("partLimit", mConfig.mPartLimit)
              << Log::Field("itemCacheTTL", mConfig.mItemCacheTTL)
              << Log::Field("removeOutdatedPeriod", mConfig.mRemoveOutdatedPeriod);

    if (auto err = fs::MakeDirAll(fs::JoinPath(mConfig.mImagePath, cBlobsFolder)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallItem(const InstallItemInfo& itemInfo)
{
    LOG_INF() << "Install item" << Log::Field("itemID", itemInfo.mItemID) << Log::Field("version", itemInfo.mVersion)
              << Log::Field("type", itemInfo.mType) << Log::Field("rootDigest", itemInfo.mRootDigest);

    if (auto err = LockItem(itemInfo.mItemID, itemInfo.mVersion); !err.IsNone()) {
        return err;
    }

    auto unlockItem = DeferRelease([this, &itemInfo]() { UnlockItem(itemInfo.mItemID, itemInfo.mVersion); });

    DeployableItemInfo storedInfo;

    if (auto err = StartItemInstall(itemInfo, storedInfo); !err.IsNone()) {
        return err;
    }

    auto err = InstallItemBlobs(itemInfo);

    if (auto syncErr = mFSPlatform->Sync(mConfig.mImagePath); !syncErr.IsNone()) {
        LOG_ERR() << "Failed to sync image path" << Log::Field(syncErr);

        if (err.IsNone()) {
            err = AOS_ERROR_WRAP(syncErr);
        }
    }

    if (err.IsNone()) {
        err = SetStoredItemState(storedInfo, ItemStateEnum::eInstalled);
    }

    if (!err.IsNone()) {
        if (auto setStateErr = SetStoredItemState(storedInfo, ItemStateEnum::eFailed, err); !setStateErr.IsNone()) {
            LOG_ERR() << "Failed to set stored item state" << Log::Field(setStateErr);

            if (setStateErr = SetInProgressItemState(
                    itemInfo.mItemID, itemInfo.mVersion, itemInfo.mType, ItemStateEnum::eFailed, err);
                !setStateErr.IsNone()) {
                LOG_ERR() << "Failed to set in-progress item state" << Log::Field(setStateErr);
            }
        }
    }

    return err;
}

Error ImageManager::RemoveItem(const String& itemID, const String& version)
{
    LOG_INF() << "Remove item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    if (auto err = LockItem(itemID, version); !err.IsNone()) {
        return err;
    }

    auto unlockItem = DeferRelease([this, &itemID, &version]() { UnlockItem(itemID, version); });

    DeployableItemInfo storedInfo;

    if (auto err = mStorage->GetDeployableItem(itemID, version, storedInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (storedInfo.mState == ItemStateEnum::eRemoved) {
        LOG_DBG() << "Item already removed" << Log::Field("itemID", itemID) << Log::Field("version", version);

        return ErrorEnum::eNone;
    }

    const UpdateItemStatus prevStatus = storedInfo;

    if (auto err = SetInProgressItemState(itemID, version, storedInfo.mType, ItemStateEnum::eRemoving); !err.IsNone()) {
        LOG_ERR() << "Failed to set in-progress item state" << Log::Field(err);
    }

    if (auto err = SetStoredItemState(storedInfo, ItemStateEnum::eRemoved); !err.IsNone()) {
        RestoreRemovingItemState(prevStatus, err);

        return err;
    }

    if (auto err = mSpaceAllocator->AddOutdatedItem(itemID, version, storedInfo.mTimestamp); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::Cancel()
{
    LOG_DBG() << "Cancel";

    return ErrorEnum::eNotSupported;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void ImageManager::NotifyItemStatusChanged(const UpdateItemStatus& status)
{
    LockGuard lock {mListenersMutex};

    LOG_DBG() << "Item status changed" << Log::Field("itemID", status.mItemID) << Log::Field("type", status.mType)
              << Log::Field("version", status.mVersion) << Log::Field("state", status.mState)
              << Log::Field(status.mError);

    for (auto* listener : mListeners) {
        listener->OnItemStatusChanged(status);
    }
}

void ImageManager::NotifyItemRemovedFromCache(const String& itemID, const String& version)
{
    LockGuard lock {mListenersMutex};

    LOG_DBG() << "Item removed from cache" << Log::Field("itemID", itemID) << Log::Field("version", version);

    for (auto* listener : mListeners) {
        listener->OnItemRemovedFromCache(itemID, version);
    }
}

Error ImageManager::ValidateBlob(const String& path, const String& digest, size_t size)
{
    LOG_DBG() << "Validate blob" << Log::Field("digest", digest) << Log::Field("path", path);

    StaticString<oci::cDigestLen> algStr;
    StaticString<oci::cDigestLen> hash;

    if (auto err = ParseDigest(digest, algStr, hash); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [alg, err] = GetSupportedHash(algStr);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto hashBytes = MakeUnique<StaticArray<uint8_t, cBlobHashSize>>(mAllocator);
    if (!hashBytes) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (err = hash.HexToByteArray(*hashBytes); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest hash"));
    }

    bool exists = false;

    if (Tie(exists, err) = fs::FileExist(path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!exists) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "file not found"));
    }

    fs::FileInfo fileInfo;

    if (err = mFileInfoProvider->GetFileInfo(path, fileInfo, alg); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (size && fileInfo.mSize != size) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "size mismatch"));
    }

    if (fileInfo.mCheckSum != *hashBytes) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "checksum mismatch"));
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetBlobInfo(const String& digest, BlobInfo& info)
{
    while (true) {
        if (auto err = mBlobInfoProvider->GetBlobInfo(digest, info); !err.IsNone()) {
            UniqueLock<Mutex> lock(mMutex);

            LOG_ERR() << "Failed to get blob info" << Log::Field("digest", digest) << Log::Field(err);

            if (err = mCondVar.Wait(lock, cRetryTimeout); !err.IsNone() && err != ErrorEnum::eTimeout) {
                return AOS_ERROR_WRAP(err);
            }

            LOG_DBG() << "Retrying get blob info" << Log::Field("digest", digest);

            continue;
        }

        break;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::DownloadItemBlob(const InstallItemInfo& itemInfo, const String& path, const String& digest)
{
    auto blobInfo = MakeUnique<BlobInfo>(mAllocator);
    if (!blobInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = GetBlobInfo(digest, *blobInfo); !err.IsNone()) {
        return err;
    }

    if (auto err = fs::MakeDirAll(fs::Dir(path)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [space, err] = mSpaceAllocator->AllocateSpace(blobInfo->mSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = SetInProgressItemState(itemInfo.mItemID, itemInfo.mVersion, itemInfo.mType, ItemStateEnum::eDownloading);
        !err.IsNone()) {
        LOG_ERR() << "Failed to set in-progress item state" << Log::Field(err);
    }

    LOG_DBG() << "Download blob" << Log::Field("digest", digest) << Log::Field("size", blobInfo->mSize);

    if (err = DownloadBlob(path, *blobInfo); !err.IsNone()) {
        if (auto removeErr = RemoveBlob(path, digest).mError; !removeErr.IsNone()) {
            LOG_ERR() << "Failed to remove blob" << Log::Field("digest", digest) << Log::Field(removeErr);
        }

        if (auto releaseErr = space->Release(); !releaseErr.IsNone()) {
            LOG_ERR() << "Failed to release space" << Log::Field("digest", digest) << Log::Field(releaseErr);
        }

        return err;
    }

    if (err = space->Accept(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallItemBlob(
    const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, String& path)
{
    LOG_DBG() << "Install blob" << Log::Field("digest", descriptor.mDigest)
              << Log::Field("mediaType", descriptor.mMediaType) << Log::Field("size", descriptor.mSize);

    if (auto err = GetBlobPath(descriptor.mDigest, path); !err.IsNone()) {
        return err;
    }

    if (auto err = LockBlob(descriptor.mDigest); !err.IsNone()) {
        return err;
    }

    auto unlockBlob = DeferRelease([this, digest = &descriptor.mDigest]() { UnlockBlob(*digest); });

    if (auto err = AddItemBlob(itemInfo.mItemID, itemInfo.mVersion, descriptor); !err.IsNone()) {
        return err;
    }

    if (auto err = ProcessBlob(itemInfo, path, descriptor); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallLayerBlob(
    const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, const String& diffID)
{
    LOG_DBG() << "Install layer blob" << Log::Field("digest", descriptor.mDigest)
              << Log::Field("mediaType", descriptor.mMediaType) << Log::Field("size", descriptor.mSize)
              << Log::Field("diffID", diffID);

    StaticString<cFilePathLen> path;

    if (auto err = GetBlobPath(descriptor.mDigest, path); !err.IsNone()) {
        return err;
    }

    if (auto err = LockBlob(descriptor.mDigest); !err.IsNone()) {
        return err;
    }

    auto unlockBlob = DeferRelease([this, digest = &descriptor.mDigest]() { UnlockBlob(*digest); });

    if (auto err = AddItemBlob(itemInfo.mItemID, itemInfo.mVersion, descriptor, true, diffID); !err.IsNone()) {
        return err;
    }

    if (auto err = ProcessLayer(itemInfo, path, descriptor, diffID); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ProcessBlob(
    const InstallItemInfo& itemInfo, const String& path, const oci::ContentDescriptor& descriptor)
{
    auto err = ValidateBlob(path, descriptor.mDigest, descriptor.mSize);
    if (err.IsNone()) {
        return ErrorEnum::eNone;
    }

    // The blob can't be validated with the unsupported digest algorithm, so downloading it again doesn't help.
    if (err.Is(ErrorEnum::eNotSupported)) {
        return err;
    }

    if (!err.Is(ErrorEnum::eNotFound)) {
        LOG_ERR() << "Blob validation failed" << Log::Field("digest", descriptor.mDigest) << Log::Field(err);
    }

    // The unpacked layer data is removed by ProcessLayer.
    if (err = RemoveItemBlob(path, descriptor); !err.IsNone()) {
        return err;
    }

    if (err = DownloadItemBlob(itemInfo, path, descriptor.mDigest); !err.IsNone()) {
        return err;
    }

    if (err = ValidateBlob(path, descriptor.mDigest, descriptor.mSize); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ProcessLayer(
    const InstallItemInfo& itemInfo, const String& path, const oci::ContentDescriptor& descriptor, const String& diffID)
{
    auto err = ValidateLayer(path, descriptor, diffID);
    if (err.IsNone()) {
        return ErrorEnum::eNone;
    }

    // eNotSupported means the layer is kept as packed blob only: there is no unpacked layer data to handle.
    if (err.Is(ErrorEnum::eNotSupported)) {
        if (err = ProcessBlob(itemInfo, path, descriptor); !err.IsNone()) {
            return err;
        }

        return ErrorEnum::eNone;
    }

    if (!err.Is(ErrorEnum::eNotFound)) {
        LOG_ERR() << "Layer validation failed" << Log::Field("digest", descriptor.mDigest) << Log::Field(err);
    }

    size_t layerSize = 0;

    Tie(layerSize, err) = RemoveLayer(path, descriptor, diffID);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotSupported)) {
        return err;
    }

    if (layerSize > 0) {
        mSpaceAllocator->FreeSpace(layerSize);
    }

    if (err = ProcessBlob(itemInfo, path, descriptor); !err.IsNone()) {
        return err;
    }

    if (err = UnpackLayer(path, descriptor, diffID); !err.IsNone() && !err.Is(ErrorEnum::eNotSupported)) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::RemoveItemBlob(
    const String& path, const oci::ContentDescriptor& descriptor, bool isLayer, const String& diffID)
{
    Error  err;
    size_t layerSize = 0;
    size_t blobSize  = 0;

    if (isLayer) {
        Tie(layerSize, err) = RemoveLayer(path, descriptor, diffID);
        if (!err.IsNone() && !err.Is(ErrorEnum::eNotSupported)) {
            return err;
        }
    }

    Tie(blobSize, err) = RemoveBlob(path, descriptor.mDigest);
    if (!err.IsNone()) {
        return err;
    }

    if (layerSize + blobSize > 0) {
        mSpaceAllocator->FreeSpace(layerSize + blobSize);
    }

    return ErrorEnum::eNone;
}

RetWithError<size_t> ImageManager::RemoveBlob(const String& path, const String& digest)
{
    LOG_DBG() << "Remove blob" << Log::Field("digest", digest) << Log::Field("path", path);

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (!exists) {
        return {0, ErrorEnum::eNone};
    }

    size_t size = 0;

    if (Tie(size, err) = fs::CalculateSize(*mAllocator, path); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (err = fs::RemoveAll(path); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    return {size, ErrorEnum::eNone};
}

RetWithError<size_t> ImageManager::RemoveOutdatedItem(const String& itemID, const String& version)
{
    LOG_DBG() << "Remove outdated item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    // The item remover is called with the space allocator lock held, so it never waits for the item or its blobs: the
    // waiting thread may hold them. The item which can't be locked is skipped.
    if (!TryLockItem(itemID, version)) {
        LOG_DBG() << "Can't lock outdated item" << Log::Field("itemID", itemID) << Log::Field("version", version);

        return {0, ErrorEnum::eNone};
    }

    auto unlockItem = DeferRelease([this, &itemID, &version]() { UnlockItem(itemID, version); });

    DeployableItemInfo item;

    auto err = mStorage->GetDeployableItem(itemID, version, item);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    // The item could be removed from the cache or installed again after it was added as outdated.
    if (err.Is(ErrorEnum::eNotFound) || item.mState != ItemStateEnum::eRemoved) {
        LOG_DBG() << "Outdated item is not removed" << Log::Field("itemID", itemID) << Log::Field("version", version);

        return {0, ErrorEnum::eNone};
    }

    auto blobs = MakeUnique<StaticArray<ItemBlobInfo, cMaxNumItemBlobs>>(mAllocator);
    if (!blobs) {
        return {0, AOS_ERROR_WRAP(ErrorEnum::eNoMemory)};
    }

    if (err = mStorage->GetItemBlobs(itemID, version, *blobs); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (err = mStorage->RemoveDeployableItem(itemID, version); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    size_t freedSize = 0;

    for (const auto& blob : *blobs) {
        size_t blobSize = 0;

        // The blob which is failed to remove becomes orphan and is removed by the orphan blobs cleanup.
        if (Tie(blobSize, err) = RemoveUnusedItemBlob(blob); !err.IsNone()) {
            LOG_ERR() << "Failed to remove item blob" << Log::Field("digest", blob.mDigest) << Log::Field(err);
        }

        freedSize += blobSize;
    }

    LOG_INF() << "Outdated item removed" << Log::Field("itemID", itemID) << Log::Field("version", version)
              << Log::Field("freedSize", freedSize);

    NotifyItemRemovedFromCache(itemID, version);

    return {freedSize, ErrorEnum::eNone};
}

RetWithError<size_t> ImageManager::RemoveUnusedItemBlob(const ItemBlobInfo& blob)
{
    // A locked blob is being processed by an item being installed, so it is in use. The blob which can't be locked is
    // kept and becomes orphan if it is not used.
    if (!TryLockBlob(blob.mDigest)) {
        return {0, ErrorEnum::eNone};
    }

    auto unlockBlob = DeferRelease([this, &blob]() { UnlockBlob(blob.mDigest); });

    auto [used, err] = mStorage->IsBlobUsed(blob.mDigest);
    if (!err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (used) {
        return {0, ErrorEnum::eNone};
    }

    StaticString<cFilePathLen> path;

    if (err = GetBlobPath(blob.mDigest, path); !err.IsNone()) {
        return {0, err};
    }

    size_t layerSize = 0;

    if (blob.mIsLayer) {
        if (Tie(layerSize, err) = RemoveLayer(path, blob, blob.mDiffID);
            !err.IsNone() && !err.Is(ErrorEnum::eNotSupported)) {
            LOG_ERR() << "Failed to remove layer" << Log::Field("digest", blob.mDigest) << Log::Field(err);
        }

        if (!err.IsNone()) {
            layerSize = 0;
        }
    }

    size_t blobSize = 0;

    if (Tie(blobSize, err) = RemoveBlob(path, blob.mDigest); !err.IsNone()) {
        return {layerSize, err};
    }

    return {layerSize + blobSize, ErrorEnum::eNone};
}

Error ImageManager::LockItem(const String& itemID, const String& version)
{
    UniqueLock lock {mMutex};

    if (auto err = mCondVar.Wait(lock,
            [&]() {
                return FindInProgressItem(itemID, version) == nullptr
                    && mItemsInProgress.Size() < cMaxNumConcurrentItems;
            });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    UpdateItemStatus item;

    item.mItemID  = itemID;
    item.mVersion = version;

    if (auto err = mItemsInProgress.PushBack(item); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool ImageManager::TryLockItem(const String& itemID, const String& version)
{
    LockGuard lock {mMutex};

    if (FindInProgressItem(itemID, version) != nullptr) {
        return false;
    }

    if (mItemsInProgress.IsFull()) {
        LOG_ERR() << "Items in progress list is full" << Log::Field("itemID", itemID) << Log::Field("version", version);

        return false;
    }

    UpdateItemStatus item;

    item.mItemID  = itemID;
    item.mVersion = version;

    if (auto err = mItemsInProgress.PushBack(item); !err.IsNone()) {
        return false;
    }

    return true;
}

void ImageManager::UnlockItem(const String& itemID, const String& version)
{
    {
        LockGuard lock {mMutex};

        mItemsInProgress.RemoveIf(
            [&](const UpdateItemStatus& item) { return item.mItemID == itemID && item.mVersion == version; });
    }

    if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify condition variable" << Log::Field(err);
    }
}

UpdateItemStatus* ImageManager::FindInProgressItem(const String& itemID, const String& version)
{
    auto it = mItemsInProgress.FindIf(
        [&](const UpdateItemStatus& item) { return item.mItemID == itemID && item.mVersion == version; });

    return it != mItemsInProgress.end() ? &*it : nullptr;
}

Error ImageManager::LockBlob(const String& digest)
{
    UniqueLock lock {mMutex};

    if (auto err = mCondVar.Wait(
            lock, [&]() { return !IsBlobInProgress(digest) && mBlobsInProgress.Size() < cMaxNumConcurrentItems; });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mBlobsInProgress.PushBack(digest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool ImageManager::TryLockBlob(const String& digest)
{
    LockGuard lock {mMutex};

    if (IsBlobInProgress(digest)) {
        return false;
    }

    if (mBlobsInProgress.IsFull()) {
        LOG_ERR() << "Blobs in progress list is full" << Log::Field("digest", digest);

        return false;
    }

    if (auto err = mBlobsInProgress.PushBack(digest); !err.IsNone()) {
        return false;
    }

    return true;
}

void ImageManager::UnlockBlob(const String& digest)
{
    {
        LockGuard lock {mMutex};

        mBlobsInProgress.RemoveIf([&digest](const String& blob) { return blob == digest; });
    }

    if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify condition variable" << Log::Field(err);
    }
}

Error ImageManager::StartItemInstall(const InstallItemInfo& itemInfo, DeployableItemInfo& storedInfo)
{
    auto err = mStorage->GetDeployableItem(itemInfo.mItemID, itemInfo.mVersion, storedInfo);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (err.IsNone()) {
        if (storedInfo.mRootDigest != itemInfo.mRootDigest) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "root digest mismatch"));
        }

        const auto prevState = storedInfo.mState;

        if (err = SetStoredItemState(storedInfo, ItemStateEnum::eInstalling, ErrorEnum::eNone); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        // The item is restored after it leaves the eRemoved state: if it stays outdated, it is not removed from the
        // cache as it is not in the eRemoved state anymore.
        if (prevState == ItemStateEnum::eRemoved) {
            if (err = mSpaceAllocator->RestoreOutdatedItem(itemInfo.mItemID, itemInfo.mVersion);
                !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
                LOG_ERR() << "Failed to restore outdated item" << Log::Field(err);
            }
        }

        return ErrorEnum::eNone;
    }

    storedInfo.mItemID     = itemInfo.mItemID;
    storedInfo.mType       = itemInfo.mType;
    storedInfo.mVersion    = itemInfo.mVersion;
    storedInfo.mRootDigest = itemInfo.mRootDigest;
    storedInfo.mState      = ItemStateEnum::eInstalling;
    storedInfo.mTimestamp  = Time::Now();
    storedInfo.mError      = ErrorEnum::eNone;

    if (err = AddStoredItem(storedInfo); !err.IsNone()) {
        return err;
    }

    if (err = SetInProgressItemState(itemInfo.mItemID, itemInfo.mVersion, itemInfo.mType, ItemStateEnum::eInstalling);
        !err.IsNone()) {
        LOG_ERR() << "Failed to set in-progress item state" << Log::Field(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::AddStoredItem(const DeployableItemInfo& item)
{
    // Serializes the storage count check with adding the item, so concurrent installs don't exceed the storage limit.
    // The item remover doesn't take this mutex, so it can be held while the space allocator is called.
    LockGuard lock {mStorageMutex};

    while (true) {
        auto [count, err] = mStorage->GetDeployableItemsCount();
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (count < cMaxNumStoredItems) {
            break;
        }

        LOG_DBG() << "Storage is full, remove outdated item" << Log::Field("count", count);

        // The space allocator drops the outdated item whatever the result, so the loop ends when no outdated item is
        // left.
        if (err = mSpaceAllocator->RemoveOutdatedItem().mError; !err.IsNone()) {
            if (err.Is(ErrorEnum::eNotFound)) {
                return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "storage is full"));
            }

            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = mStorage->AddDeployableItem(item); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallItemBlobs(const InstallItemInfo& itemInfo)
{
    if (!itemInfo.mIsRootIndex) {
        if (auto err = InstallManifestBlobs(
                itemInfo, oci::ContentDescriptor {oci::cMediaTypeImageManifest, itemInfo.mRootDigest, 0});
            !err.IsNone()) {
            return err;
        }

        return ErrorEnum::eNone;
    }

    StaticString<cFilePathLen> path;

    if (auto err
        = InstallItemBlob(itemInfo, oci::ContentDescriptor {oci::cMediaTypeImageIndex, itemInfo.mRootDigest, 0}, path);
        !err.IsNone()) {
        return err;
    }

    auto index = MakeUnique<oci::ImageIndex>(mAllocator);
    if (!index) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mOCISpec->LoadImageIndex(path, *index); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& manifest : index->mManifests) {
        if (auto err = InstallManifestBlobs(itemInfo, manifest); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallManifestBlobs(const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor)
{
    LOG_DBG() << "Install manifest" << Log::Field("digest", descriptor.mDigest);

    StaticString<cFilePathLen> path;

    if (auto err = InstallItemBlob(itemInfo, descriptor, path); !err.IsNone()) {
        return err;
    }

    auto manifest = MakeUnique<oci::ImageManifest>(mAllocator);
    if (!manifest) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mOCISpec->LoadImageManifest(path, *manifest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (manifest->mItemConfig.HasValue()) {
        if (auto err = InstallItemBlob(itemInfo, *manifest->mItemConfig, path); !err.IsNone()) {
            return err;
        }
    }

    UniquePtr<oci::ImageConfig> imageConfig;

    if (!manifest->mConfig.mDigest.IsEmpty()) {
        if (auto err = InstallItemBlob(itemInfo, manifest->mConfig, path); !err.IsNone()) {
            return err;
        }

        imageConfig = MakeUnique<oci::ImageConfig>(mAllocator);
        if (!imageConfig) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        if (auto err = mOCISpec->LoadImageConfig(path, *imageConfig); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (imageConfig->mRootfs.mDiffIDs.Size() < manifest->mLayers.Size()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "diff ID's size mismatch"));
        }
    }

    for (size_t i = 0; i < manifest->mLayers.Size(); ++i) {
        StaticString<oci::cDigestLen> diffID;

        if (imageConfig) {
            diffID = imageConfig->mRootfs.mDiffIDs[i];
        }

        if (auto err = InstallLayerBlob(itemInfo, manifest->mLayers[i], diffID); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::AddItemBlob(const String& itemID, const String& version, const oci::ContentDescriptor& descriptor,
    bool isLayer, const String& diffID)
{
    if (auto err = mStorage->AddItemBlob(itemID, version, ItemBlobInfo {descriptor, isLayer, diffID}); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SetInProgressItemState(
    const String& itemID, const String& version, UpdateItemType type, ItemStateEnum state, const Error& error)
{
    UpdateItemStatus status;
    bool             stateChanged = false;

    {
        LockGuard lock {mMutex};

        auto itemInProgress = FindInProgressItem(itemID, version);
        if (!itemInProgress) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "item not in progress"));
        }

        stateChanged = itemInProgress->mState != state;

        itemInProgress->mType  = type;
        itemInProgress->mState = state;
        itemInProgress->mError = error;

        status = *itemInProgress;
    }

    if (stateChanged) {
        NotifyItemStatusChanged(status);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::RemoveInterruptedItem(DeployableItemInfo& item)
{
    LOG_WRN() << "Remove interrupted item" << Log::Field("itemID", item.mItemID)
              << Log::Field("version", item.mVersion);

    // The install was interrupted rather than failed, so the item is kept in the cache with its blobs as outdated.
    // The item status is not notified on start, it is reported by GetItemsStatuses.
    item.mState     = ItemStateEnum::eRemoved;
    item.mTimestamp = Time::Now();
    item.mError     = ErrorEnum::eNone;

    if (auto err = mStorage->UpdateDeployableItem(item); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void ImageManager::RestoreRemovingItemState(const UpdateItemStatus& prevStatus, const Error& error)
{
    {
        LockGuard lock {mMutex};

        // The stored state is reported again once the item is not being removed.
        if (auto itemInProgress = FindInProgressItem(prevStatus.mItemID, prevStatus.mVersion); itemInProgress) {
            itemInProgress->mState = ItemStateEnum::eUnknown;
            itemInProgress->mError = ErrorEnum::eNone;
        }
    }

    auto status = prevStatus;

    status.mError = error;

    NotifyItemStatusChanged(status);
}

Error ImageManager::SetStoredItemState(DeployableItemInfo& item, ItemStateEnum state, const Error& error)
{
    auto stateChanged = (item.mState != state);

    item.mState     = state;
    item.mTimestamp = Time::Now();
    item.mError     = error;

    if (auto err = mStorage->UpdateDeployableItem(item); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    {
        LockGuard lock {mMutex};

        if (auto itemInProgress = FindInProgressItem(item.mItemID, item.mVersion); itemInProgress) {
            itemInProgress->mType  = item.mType;
            itemInProgress->mState = state;
            itemInProgress->mError = error;
        }
    }

    if (stateChanged) {
        NotifyItemStatusChanged(item);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::common::imagemanager
