/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "imagemanager.hpp"

namespace aos::common::imagemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ImageManager::Start()
{
    LOG_DBG() << "Start image manager";

    return ErrorEnum::eNotSupported;
}

Error ImageManager::Stop()
{
    LOG_DBG() << "Stop image manager";

    return ErrorEnum::eNotSupported;
}

Error ImageManager::GetItemsStatuses(Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Get items statuses";

    auto items = MakeUnique<StaticArray<DeployableItemInfo, cMaxNumStoredItems>>(mAllocator);
    if (!items) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    LockGuard lock {mMutex};

    if (auto err = mStorage->GetAllDeployableItems(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        UpdateItemStatus status;

        status.mItemID       = item.mItemID;
        status.mType         = item.mType;
        status.mVersion      = item.mVersion;
        status.mPreinstalled = false;

        if (const auto* itemInProgress = FindItemInProgress(item.mItemID, item.mVersion);
            itemInProgress != nullptr && itemInProgress->mNotified) {
            status.mState = itemInProgress->mState;
        } else if (item.mState == ItemStateEnum::eInstalling) {
            status.mState = ItemStateEnum::eFailed;
        } else {
            status.mState = item.mState;

            if (item.mState == ItemStateEnum::eFailed) {
                status.mError = item.mError;
            }
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

/***********************************************************************************************************************
 * Protected
 **********************************************************************************************************************/

Error ImageManager::Init(const Config& config, StorageItf& storage, spaceallocator::SpaceAllocatorItf& spaceAllocator,
    fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec, fs::FSPlatformItf& fsPlatform,
    AllocatorItf& allocator)
{
    LOG_DBG() << "Init image manager";

    mConfig           = config;
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
              << Log::Field("type", itemInfo.mType) << Log::Field("rootDigest", itemInfo.mRootDigest)
              << Log::Field("isRootIndex", itemInfo.mIsRootIndex);

    StaticString<oci::cDigestLen> algorithm;
    StaticString<oci::cDigestLen> hash;

    if (auto err = ParseDigest(itemInfo.mRootDigest, algorithm, hash); !err.IsNone()) {
        return err;
    }

    if (auto err = AcquireItem(itemInfo.mItemID, itemInfo.mVersion); !err.IsNone()) {
        return err;
    }

    auto releaseItem = DeferRelease([this, &itemInfo]() { ReleaseItem(itemInfo.mItemID, itemInfo.mVersion); });

    DeployableItemInfo item;
    bool               wasRemoved = false;

    if (auto err = StartItemInstall(itemInfo, item, wasRemoved); !err.IsNone()) {
        LOG_ERR() << "Can't start item install" << Log::Field("itemID", itemInfo.mItemID)
                  << Log::Field("version", itemInfo.mVersion) << Log::Field(err);

        return err;
    }

    NotifyItemStatusChanged(
        CreateItemStatus(itemInfo.mItemID, itemInfo.mType, itemInfo.mVersion, ItemStateEnum::eInstalling));

    auto err = InstallItemBlobs(itemInfo, wasRemoved);

    if (auto syncErr = mFSPlatform->Sync(mConfig.mImagePath); !syncErr.IsNone()) {
        if (err.IsNone()) {
            err = AOS_ERROR_WRAP(syncErr);
        } else {
            LOG_ERR() << "Can't sync image path" << Log::Field("path", mConfig.mImagePath)
                      << Log::Field(AOS_ERROR_WRAP(syncErr));
        }
    }

    if (err.IsNone()) {
        err = SetItemState(item, ItemStateEnum::eInstalled);
    }

    if (!err.IsNone()) {
        LOG_ERR() << "Can't install item" << Log::Field("itemID", itemInfo.mItemID)
                  << Log::Field("version", itemInfo.mVersion) << Log::Field(err);

        if (auto stateErr = SetItemState(item, ItemStateEnum::eFailed, err); !stateErr.IsNone()) {
            LOG_ERR() << "Can't set item failed state" << Log::Field("itemID", itemInfo.mItemID)
                      << Log::Field("version", itemInfo.mVersion) << Log::Field(stateErr);
        }
    } else {
        LOG_INF() << "Item installed" << Log::Field("itemID", itemInfo.mItemID)
                  << Log::Field("version", itemInfo.mVersion);
    }

    {
        LockGuard lock {mMutex};

        if (auto* itemInProgress = FindItemInProgress(itemInfo.mItemID, itemInfo.mVersion); itemInProgress != nullptr) {
            itemInProgress->mNotified = false;
        }
    }

    NotifyItemStatusChanged(CreateItemStatus(itemInfo.mItemID, itemInfo.mType, itemInfo.mVersion,
        err.IsNone() ? ItemStateEnum::eInstalled : ItemStateEnum::eFailed, err));

    return err;
}

Error ImageManager::RemoveItem(const String& itemID, const String& version)
{
    LOG_INF() << "Remove item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    if (auto err = AcquireItem(itemID, version); !err.IsNone()) {
        return err;
    }

    auto releaseItem = DeferRelease([this, &itemID, &version]() { ReleaseItem(itemID, version); });

    DeployableItemInfo item;
    ItemStateEnum      prevState {};

    {
        LockGuard lock {mMutex};

        if (auto err = mStorage->GetDeployableItem(itemID, version, item); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (item.mState != ItemStateEnum::eRemoved) {
            prevState = item.mState == ItemStateEnum::eInstalling ? ItemStateEnum::eFailed : item.mState.GetValue();

            if (auto* itemInProgress = FindItemInProgress(itemID, version); itemInProgress != nullptr) {
                itemInProgress->mType     = item.mType;
                itemInProgress->mNotified = true;
                itemInProgress->mState    = ItemStateEnum::eRemoving;
            }
        }
    }

    if (item.mState == ItemStateEnum::eRemoved) {
        LOG_DBG() << "Item already removed, add it as outdated" << Log::Field("itemID", itemID)
                  << Log::Field("version", version);

        releaseItem.Reset();

        if (auto err = GetSpaceAllocator().AddOutdatedItem(itemID, version, item.mTimestamp); !err.IsNone()) {
            LOG_ERR() << "Can't add outdated item" << Log::Field("itemID", itemID) << Log::Field("version", version)
                      << Log::Field(AOS_ERROR_WRAP(err));

            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    NotifyItemStatusChanged(CreateItemStatus(itemID, item.mType, version, ItemStateEnum::eRemoving));

    Error err;

    {
        LockGuard lock {mMutex};

        item.mState     = ItemStateEnum::eRemoved;
        item.mTimestamp = Time::Now();
        item.mError     = ErrorEnum::eNone;

        if (err = mStorage->UpdateDeployableItem(item); !err.IsNone()) {
            err = AOS_ERROR_WRAP(err);
        }

        if (auto* itemInProgress = FindItemInProgress(itemID, version); itemInProgress != nullptr) {
            itemInProgress->mNotified = false;
        }
    }

    if (!err.IsNone()) {
        LOG_ERR() << "Can't remove item" << Log::Field("itemID", itemID) << Log::Field("version", version)
                  << Log::Field(err);

        NotifyItemStatusChanged(CreateItemStatus(itemID, item.mType, version, prevState, err));

        return err;
    }

    NotifyItemStatusChanged(CreateItemStatus(itemID, item.mType, version, ItemStateEnum::eRemoved));

    releaseItem.Reset();

    if (err = GetSpaceAllocator().AddOutdatedItem(itemID, version, item.mTimestamp); !err.IsNone()) {
        LOG_ERR() << "Can't add outdated item" << Log::Field("itemID", itemID) << Log::Field("version", version)
                  << Log::Field(AOS_ERROR_WRAP(err));

        return AOS_ERROR_WRAP(err);
    }

    LOG_INF() << "Item removed" << Log::Field("itemID", itemID) << Log::Field("version", version);

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateBlob(const String& digest, const String& path) const
{
    LOG_DBG() << "Validate blob" << Log::Field("digest", digest) << Log::Field("path", path);

    StaticString<oci::cDigestLen> algorithm;
    StaticString<oci::cDigestLen> hash;

    if (auto err = ParseDigest(digest, algorithm, hash); !err.IsNone()) {
        return err;
    }

    if (algorithm != cSHA256DigestAlgorithm) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotSupported, "unsupported digest algorithm"));
    }

    StaticArray<uint8_t, crypto::cSHA256Size> hashBytes;

    if (hash.Size() != crypto::cSHA256Size * 2 || !hash.HexToByteArray(hashBytes).IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest hash"));
    }

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!exists) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "blob not found"));
    }

    fs::FileInfo fileInfo;

    if (err = mFileInfoProvider->GetFileInfo(path, fileInfo, crypto::HashEnum::eSHA256); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (fileInfo.mCheckSum != hashBytes) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidChecksum);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::DownloadBlob(const String& digest, const String& path)
{
    bool             notify = false;
    UpdateItemStatus status;

    {
        LockGuard lock {mMutex};

        if (const auto* blob = FindBlobInProgress(digest); blob != nullptr) {
            if (auto* item = FindItemInProgress(blob->mItemID, blob->mVersion);
                item != nullptr && item->mNotified && item->mState == ItemStateEnum::eInstalling) {
                item->mState = ItemStateEnum::eDownloading;

                status = CreateItemStatus(item->mItemID, item->mType, item->mVersion, ItemStateEnum::eDownloading);
                notify = true;
            }
        }
    }

    if (notify) {
        NotifyItemStatusChanged(status);
    }

    return DoDownloadBlob(digest, path);
}

Error ImageManager::InstallLayer(const oci::ContentDescriptor& descriptor, const String& diffDigest, const String& path)
{
    LOG_DBG() << "Install layer" << Log::Field("digest", descriptor.mDigest) << Log::Field("diffDigest", diffDigest)
              << Log::Field("path", path) << Log::Field("size", descriptor.mSize);

    StaticString<cFilePathLen> parentPath;

    if (auto err = fs::ParentPath(path, parentPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::MakeDirAll(parentPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::RemoveAll(path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    UniquePtr<spaceallocator::SpaceItf> space;
    Error                               err;

    if (descriptor.mSize > 0) {
        if (Tie(space, err) = GetSpaceAllocator().AllocateSpace(descriptor.mSize); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (err = DownloadBlob(descriptor.mDigest, path); !err.IsNone()) {
        err = AOS_ERROR_WRAP(err);
    } else {
        err = ValidateBlob(descriptor.mDigest, path);
    }

    if (!err.IsNone()) {
        LOG_ERR() << "Failed to install layer" << Log::Field("digest", descriptor.mDigest) << Log::Field(err);

        if (auto removeErr = fs::RemoveAll(path); !removeErr.IsNone()) {
            LOG_ERR() << "Failed to remove layer blob" << Log::Field("path", path) << Log::Field(removeErr);
        }

        if (space) {
            if (auto releaseErr = space->Release(); !releaseErr.IsNone()) {
                LOG_ERR() << "Failed to release space" << Log::Field(AOS_ERROR_WRAP(releaseErr));
            }
        }

        return err;
    }

    if (space) {
        if (auto acceptErr = space->Accept(); !acceptErr.IsNone()) {
            LOG_ERR() << "Can't accept space" << Log::Field(AOS_ERROR_WRAP(acceptErr));
        }
    }

    return ErrorEnum::eNone;
}

RetWithError<size_t> ImageManager::RemoveLayer(
    const String& digest, const String& mediaType, const String& diffID, const String& path)
{
    LOG_DBG() << "Remove layer" << Log::Field("digest", digest) << Log::Field("mediaType", mediaType)
              << Log::Field("diffID", diffID) << Log::Field("path", path);

    return RemoveBlobFile(path);
}

Error ImageManager::ValidateLayer(
    const oci::ContentDescriptor& descriptor, const String& diffDigest, const String& path)
{
    LOG_DBG() << "Validate layer" << Log::Field("digest", descriptor.mDigest) << Log::Field("diffDigest", diffDigest)
              << Log::Field("path", path);

    return ValidateBlob(descriptor.mDigest, path);
}

void ImageManager::NotifyItemStatusChanged(const UpdateItemStatus& status)
{
    LOG_DBG() << "Item status changed" << Log::Field("itemID", status.mItemID) << Log::Field("type", status.mType)
              << Log::Field("version", status.mVersion) << Log::Field("state", status.mState)
              << Log::Field(status.mError);

    StaticArray<ItemStatusListenerItf*, cMaxNumListeners> listeners;

    {
        LockGuard lock {mListenersMutex};

        listeners = mListeners;
    }

    for (auto* listener : listeners) {
        listener->OnItemStatusChanged(status);
    }
}

void ImageManager::NotifyItemRemovedFromCache(const String& itemID, const String& version)
{
    LOG_DBG() << "Item removed from cache" << Log::Field("itemID", itemID) << Log::Field("version", version);

    StaticArray<ItemStatusListenerItf*, cMaxNumListeners> listeners;

    {
        LockGuard lock {mListenersMutex};

        listeners = mListeners;
    }

    for (auto* listener : listeners) {
        listener->OnItemRemovedFromCache(itemID, version);
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

RetWithError<size_t> ImageManager::RemoveItemFromCache(const String& itemID, const String& version)
{
    LOG_DBG() << "Remove item from cache" << Log::Field("itemID", itemID) << Log::Field("version", version);

    auto blobs = MakeUnique<StaticArray<ItemBlobInfo, cMaxNumItemBlobs>>(mAllocator);
    if (!blobs) {
        return {0, AOS_ERROR_WRAP(ErrorEnum::eNoMemory)};
    }

    {
        LockGuard lock {mMutex};

        DeployableItemInfo item;

        if (auto err = mStorage->GetDeployableItem(itemID, version, item); !err.IsNone()) {
            if (err.Is(ErrorEnum::eNotFound)) {
                LOG_DBG() << "Item not stored" << Log::Field("itemID", itemID) << Log::Field("version", version);

                return {0, ErrorEnum::eNone};
            }

            return {0, AOS_ERROR_WRAP(err)};
        }

        if (item.mState != ItemStateEnum::eRemoved || FindItemInProgress(itemID, version) != nullptr) {
            LOG_DBG() << "Item is not outdated" << Log::Field("itemID", itemID) << Log::Field("version", version)
                      << Log::Field("state", item.mState);

            return {0, ErrorEnum::eNone};
        }

        if (auto err = mStorage->GetItemBlobs(itemID, version, *blobs); !err.IsNone()) {
            return {0, AOS_ERROR_WRAP(err)};
        }

        if (auto err = mStorage->RemoveDeployableItem(itemID, version); !err.IsNone()) {
            return {0, AOS_ERROR_WRAP(err)};
        }
    }

    size_t freedSize = 0;

    for (const auto& blob : *blobs) {
        freedSize += RemoveItemBlob(blob);
    }

    NotifyItemRemovedFromCache(itemID, version);

    LOG_INF() << "Item removed from cache" << Log::Field("itemID", itemID) << Log::Field("version", version)
              << Log::Field("freedSize", freedSize);

    return {freedSize, ErrorEnum::eNone};
}

size_t ImageManager::RemoveItemBlob(const ItemBlobInfo& blob)
{
    if (!TryLockBlob(blob.mDigest)) {
        LOG_DBG() << "Blob is being processed, skip" << Log::Field("digest", blob.mDigest);

        return 0;
    }

    auto unlockBlob = DeferRelease([this, &blob]() { UnlockBlob(blob.mDigest); });

    bool  used = false;
    Error err;

    {
        LockGuard lock {mMutex};

        Tie(used, err) = mStorage->IsBlobUsed(blob.mDigest);
    }

    if (!err.IsNone()) {
        LOG_ERR() << "Can't check blob usage" << Log::Field("digest", blob.mDigest) << Log::Field(AOS_ERROR_WRAP(err));

        return 0;
    }

    if (used) {
        LOG_DBG() << "Blob is used, skip" << Log::Field("digest", blob.mDigest);

        return 0;
    }

    StaticString<cFilePathLen> path;

    if (err = GetBlobPath(blob.mDigest, path); !err.IsNone()) {
        LOG_ERR() << "Can't get blob path" << Log::Field("digest", blob.mDigest) << Log::Field(err);

        return 0;
    }

    size_t size = 0;

    if (blob.mIsLayer) {
        Tie(size, err) = RemoveLayer(blob.mDigest, blob.mMediaType, blob.mDiffID, path);
    } else {
        Tie(size, err) = RemoveBlobFile(path);
    }

    if (!err.IsNone()) {
        LOG_ERR() << "Can't remove blob" << Log::Field("digest", blob.mDigest) << Log::Field(err);

        return 0;
    }

    LOG_DBG() << "Blob removed" << Log::Field("digest", blob.mDigest) << Log::Field("size", size);

    return size;
}

RetWithError<size_t> ImageManager::RemoveBlobFile(const String& path)
{
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

UpdateItemStatus ImageManager::CreateItemStatus(
    const String& itemID, UpdateItemType type, const String& version, ItemStateEnum state, const Error& error)
{
    UpdateItemStatus status;

    status.mItemID  = itemID;
    status.mType    = type;
    status.mVersion = version;
    status.mState   = state;
    status.mError   = error;

    return status;
}

Error ImageManager::AcquireItem(const String& itemID, const String& version)
{
    UniqueLock lock {mMutex};

    if (auto err = mCondVar.Wait(
            lock, [&]() { return FindItemInProgress(itemID, version) == nullptr && !mItemsInProgress.IsFull(); });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    ItemInProgress item;

    item.mItemID  = itemID;
    item.mVersion = version;

    if (auto err = mItemsInProgress.PushBack(item); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void ImageManager::ReleaseItem(const String& itemID, const String& version)
{
    {
        LockGuard lock {mMutex};

        mItemsInProgress.RemoveIf(
            [&](const ItemInProgress& item) { return item.mItemID == itemID && item.mVersion == version; });
    }

    if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify condition variable" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

ImageManager::ItemInProgress* ImageManager::FindItemInProgress(const String& itemID, const String& version)
{
    auto it = mItemsInProgress.FindIf(
        [&](const ItemInProgress& item) { return item.mItemID == itemID && item.mVersion == version; });

    return it != mItemsInProgress.end() ? it : nullptr;
}

Error ImageManager::LockBlob(const String& digest, const String& itemID, const String& version)
{
    UniqueLock lock {mMutex};

    if (auto err
        = mCondVar.Wait(lock, [&]() { return FindBlobInProgress(digest) == nullptr && !mBlobsInProgress.IsFull(); });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    BlobInProgress blob;

    blob.mDigest  = digest;
    blob.mItemID  = itemID;
    blob.mVersion = version;

    if (auto err = mBlobsInProgress.PushBack(blob); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool ImageManager::TryLockBlob(const String& digest)
{
    LockGuard lock {mMutex};

    if (FindBlobInProgress(digest) != nullptr || mBlobsInProgress.IsFull()) {
        return false;
    }

    BlobInProgress blob;

    blob.mDigest = digest;

    return mBlobsInProgress.PushBack(blob).IsNone();
}

void ImageManager::UnlockBlob(const String& digest)
{
    {
        LockGuard lock {mMutex};

        mBlobsInProgress.RemoveIf([&](const BlobInProgress& blob) { return blob.mDigest == digest; });
    }

    if (auto err = mCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify condition variable" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

ImageManager::BlobInProgress* ImageManager::FindBlobInProgress(const String& digest)
{
    auto it = mBlobsInProgress.FindIf([&](const BlobInProgress& blob) { return blob.mDigest == digest; });

    return it != mBlobsInProgress.end() ? it : nullptr;
}

Error ImageManager::StartItemInstall(const InstallItemInfo& itemInfo, DeployableItemInfo& item, bool& wasRemoved)
{
    UniqueLock lock {mMutex};

    auto err = mStorage->GetDeployableItem(itemInfo.mItemID, itemInfo.mVersion, item);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (err.IsNone()) {
        if (item.mRootDigest != itemInfo.mRootDigest) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "root digest mismatch"));
        }

        wasRemoved = item.mState == ItemStateEnum::eRemoved;

        item.mState     = ItemStateEnum::eInstalling;
        item.mTimestamp = Time::Now();
        item.mError     = ErrorEnum::eNone;

        if (err = mStorage->UpdateDeployableItem(item); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        while (true) {
            size_t count = 0;

            if (Tie(count, err) = mStorage->GetDeployableItemsCount(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (count < cMaxNumStoredItems) {
                break;
            }

            LOG_DBG() << "Storage is full, remove outdated item" << Log::Field("count", count);

            (void)lock.Unlock();

            auto [freedSize, removeErr] = GetSpaceAllocator().RemoveOutdatedItem();

            (void)lock.Lock();

            if (removeErr.Is(ErrorEnum::eNotFound)) {
                return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "storage is full"));
            }

            if (!removeErr.IsNone()) {
                return AOS_ERROR_WRAP(removeErr);
            }

            LOG_DBG() << "Outdated item removed" << Log::Field("freedSize", freedSize);
        }

        wasRemoved = false;

        item.mItemID     = itemInfo.mItemID;
        item.mType       = itemInfo.mType;
        item.mVersion    = itemInfo.mVersion;
        item.mRootDigest = itemInfo.mRootDigest;
        item.mState      = ItemStateEnum::eInstalling;
        item.mTimestamp  = Time::Now();
        item.mError      = ErrorEnum::eNone;

        if (err = mStorage->AddDeployableItem(item); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto* itemInProgress = FindItemInProgress(itemInfo.mItemID, itemInfo.mVersion); itemInProgress != nullptr) {
        itemInProgress->mType     = itemInfo.mType;
        itemInProgress->mNotified = true;
        itemInProgress->mState    = ItemStateEnum::eInstalling;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallItemBlobs(const InstallItemInfo& itemInfo, bool wasRemoved)
{
    if (wasRemoved) {
        if (auto err = GetSpaceAllocator().RestoreOutdatedItem(itemInfo.mItemID, itemInfo.mVersion); !err.IsNone()) {
            if (!err.Is(ErrorEnum::eNotFound)) {
                return AOS_ERROR_WRAP(err);
            }

            LOG_DBG() << "Outdated item not found" << Log::Field("itemID", itemInfo.mItemID)
                      << Log::Field("version", itemInfo.mVersion);
        }
    }

    if (!itemInfo.mIsRootIndex) {
        return InstallManifest(itemInfo, itemInfo.mRootDigest, oci::cMediaTypeImageManifest, 0);
    }

    StaticString<cFilePathLen> path;

    if (auto err = InstallMetadataBlob(itemInfo, itemInfo.mRootDigest, oci::cMediaTypeImageIndex, 0, path);
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
        if (auto err = InstallManifest(itemInfo, manifest.mDigest, manifest.mMediaType, manifest.mSize);
            !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallManifest(
    const InstallItemInfo& itemInfo, const String& digest, const String& mediaType, size_t size)
{
    LOG_DBG() << "Install manifest" << Log::Field("digest", digest);

    StaticString<cFilePathLen> path;

    if (auto err = InstallMetadataBlob(itemInfo, digest, mediaType, size, path); !err.IsNone()) {
        return err;
    }

    auto manifest = MakeUnique<oci::ImageManifest>(mAllocator);
    if (!manifest) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mOCISpec->LoadImageManifest(path, *manifest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (itemInfo.mType == UpdateItemTypeEnum::eService && manifest->mConfig.mDigest.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "image config is missing"));
    }

    if (manifest->mItemConfig.HasValue()) {
        const auto& itemConfig = *manifest->mItemConfig;

        if (auto err = InstallMetadataBlob(itemInfo, itemConfig.mDigest, itemConfig.mMediaType, itemConfig.mSize, path);
            !err.IsNone()) {
            return err;
        }
    }

    UniquePtr<oci::ImageConfig> imageConfig;

    if (!manifest->mConfig.mDigest.IsEmpty()) {
        const auto& config = manifest->mConfig;

        if (auto err = InstallMetadataBlob(itemInfo, config.mDigest, config.mMediaType, config.mSize, path);
            !err.IsNone()) {
            return err;
        }

        if (itemInfo.mType == UpdateItemTypeEnum::eService) {
            imageConfig = MakeUnique<oci::ImageConfig>(mAllocator);
            if (!imageConfig) {
                return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
            }

            if (auto err = mOCISpec->LoadImageConfig(path, *imageConfig); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (imageConfig->mRootfs.mDiffIDs.Size() < manifest->mLayers.Size()) {
                return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "diff ID is missing"));
            }
        }
    }

    for (size_t i = 0; i < manifest->mLayers.Size(); ++i) {
        StaticString<oci::cDigestLen> diffID;

        if (imageConfig) {
            diffID = imageConfig->mRootfs.mDiffIDs[i];
        }

        if (auto err = InstallItemLayer(itemInfo, manifest->mLayers[i], diffID); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallMetadataBlob(
    const InstallItemInfo& itemInfo, const String& digest, const String& mediaType, size_t size, String& path)
{
    LOG_DBG() << "Install metadata blob" << Log::Field("digest", digest) << Log::Field("mediaType", mediaType)
              << Log::Field("size", size);

    if (auto err = LockBlob(digest, itemInfo.mItemID, itemInfo.mVersion); !err.IsNone()) {
        return err;
    }

    auto unlockBlob = DeferRelease([this, &digest]() { UnlockBlob(digest); });

    ItemBlobInfo blob;

    blob.mDigest    = digest;
    blob.mMediaType = mediaType;
    blob.mIsLayer   = false;

    if (auto err = GetBlobPath(digest, path); !err.IsNone()) {
        return err;
    }

    if (auto err = AddItemBlob(itemInfo, blob); !err.IsNone()) {
        return err;
    }

    auto err = ValidateBlob(digest, path);
    if (err.IsNone()) {
        return ErrorEnum::eNone;
    }

    LOG_DBG() << "Blob is not valid" << Log::Field("digest", digest) << Log::Field(err);

    bool exists = false;

    if (Tie(exists, err) = fs::FileExist(path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (exists) {
        size_t removedSize = 0;

        if (Tie(removedSize, err) = fs::CalculateSize(*mAllocator, path); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (err = fs::RemoveAll(path); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (removedSize > 0) {
            GetSpaceAllocator().FreeSpace(removedSize);
        }
    }

    StaticString<cFilePathLen> parentPath;

    if (err = fs::ParentPath(path, parentPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = fs::MakeDirAll(parentPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    UniquePtr<spaceallocator::SpaceItf> space;

    if (size > 0) {
        if (Tie(space, err) = GetSpaceAllocator().AllocateSpace(size); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (err = DownloadBlob(digest, path); !err.IsNone()) {
        err = AOS_ERROR_WRAP(err);
    } else {
        err = ValidateBlob(digest, path);
    }

    if (!err.IsNone()) {
        LOG_ERR() << "Failed to install blob" << Log::Field("digest", digest) << Log::Field(err);

        if (auto removeErr = fs::RemoveAll(path); !removeErr.IsNone()) {
            LOG_ERR() << "Failed to remove blob" << Log::Field("path", path) << Log::Field(AOS_ERROR_WRAP(removeErr));
        }

        if (space) {
            if (auto releaseErr = space->Release(); !releaseErr.IsNone()) {
                LOG_ERR() << "Failed to release space" << Log::Field(AOS_ERROR_WRAP(releaseErr));
            }
        }

        return err;
    }

    if (space) {
        if (auto acceptErr = space->Accept(); !acceptErr.IsNone()) {
            LOG_ERR() << "Can't accept space" << Log::Field(AOS_ERROR_WRAP(acceptErr));
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallItemLayer(
    const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, const String& diffID)
{
    LOG_DBG() << "Install item layer" << Log::Field("digest", descriptor.mDigest)
              << Log::Field("mediaType", descriptor.mMediaType) << Log::Field("diffID", diffID);

    if (auto err = LockBlob(descriptor.mDigest, itemInfo.mItemID, itemInfo.mVersion); !err.IsNone()) {
        return err;
    }

    auto unlockBlob = DeferRelease([this, &descriptor]() { UnlockBlob(descriptor.mDigest); });

    ItemBlobInfo blob;

    blob.mDigest    = descriptor.mDigest;
    blob.mMediaType = descriptor.mMediaType;
    blob.mDiffID    = diffID;
    blob.mIsLayer   = true;

    StaticString<cFilePathLen> path;

    if (auto err = GetBlobPath(descriptor.mDigest, path); !err.IsNone()) {
        return err;
    }

    if (auto err = AddItemBlob(itemInfo, blob); !err.IsNone()) {
        return err;
    }

    auto err = ValidateLayer(descriptor, diffID, path);
    if (err.IsNone()) {
        return ErrorEnum::eNone;
    }

    LOG_DBG() << "Layer is not valid" << Log::Field("digest", descriptor.mDigest) << Log::Field(err);

    size_t removedSize = 0;

    if (Tie(removedSize, err) = RemoveLayer(descriptor.mDigest, descriptor.mMediaType, diffID, path); !err.IsNone()) {
        return err;
    }

    if (removedSize > 0) {
        GetSpaceAllocator().FreeSpace(removedSize);
    }

    if (err = InstallLayer(descriptor, diffID, path); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::AddItemBlob(const InstallItemInfo& itemInfo, const ItemBlobInfo& blob)
{
    LockGuard lock {mMutex};

    if (auto err = mStorage->AddItemBlob(itemInfo.mItemID, itemInfo.mVersion, blob); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SetItemState(DeployableItemInfo& item, ItemStateEnum state, const Error& error)
{
    LockGuard lock {mMutex};

    item.mState     = state;
    item.mTimestamp = Time::Now();
    item.mError     = error;

    if (auto err = mStorage->UpdateDeployableItem(item); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ParseDigest(const String& digest, String& algorithm, String& hash)
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

    // Allowed characters are taken from OCI image spec digest grammar: algorithm is [a-z0-9]+ separated by [+._-],
    // encoded is [a-zA-Z0-9=_-]+. Uppercase is rejected in encoded part to keep digests canonical, so the same blob
    // can't map to two different paths. Restricted character set also prevents path traversal as digest parts are
    // used as path components.
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
        if (!(isLowerAlnum(ch) || ch == '=' || ch == '_' || ch == '-')) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::common::imagemanager
