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

    LockGuard lock {mMutex};

    auto items = MakeUnique<StaticArray<DeployableItemInfo, cMaxNumStoredItems>>(mAllocator);
    if (!items) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

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

    if (auto err = LockItem(itemInfo.mItemID, itemInfo.mVersion); !err.IsNone()) {
        return err;
    }

    auto unlockItem = DeferRelease([this, &itemInfo]() { UnlockItem(itemInfo.mItemID, itemInfo.mVersion); });

    DeployableItemInfo item;
    bool               wasRemoved = false;

    if (auto err = StartItemInstall(itemInfo, item, wasRemoved); !err.IsNone()) {
        LOG_ERR() << "Can't start item install" << Log::Field("itemID", itemInfo.mItemID)
                  << Log::Field("version", itemInfo.mVersion) << Log::Field(err);

        return err;
    }

    NotifyItemStatusChanged(
        CreateItemStatus(itemInfo.mItemID, itemInfo.mType, itemInfo.mVersion, ItemStateEnum::eInstalling));

    Error err;

    if (wasRemoved) {
        if (err = GetSpaceAllocator().RestoreOutdatedItem(itemInfo.mItemID, itemInfo.mVersion); !err.IsNone()) {
            if (err.Is(ErrorEnum::eNotFound)) {
                LOG_DBG() << "Outdated item not found" << Log::Field("itemID", itemInfo.mItemID)
                          << Log::Field("version", itemInfo.mVersion);

                err = ErrorEnum::eNone;
            } else {
                err = AOS_ERROR_WRAP(err);
            }
        }
    }

    if (err.IsNone()) {
        err = InstallItemBlobs(itemInfo);
    }

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

    if (auto err = LockItem(itemID, version); !err.IsNone()) {
        return err;
    }

    auto unlockItem = DeferRelease([this, &itemID, &version]() { UnlockItem(itemID, version); });

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

        unlockItem.Reset();

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

    unlockItem.Reset();

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

    auto hashBytes = MakeUnique<StaticArray<uint8_t, cBlobHashSize>>(mAllocator);

    if (auto err = hash.HexToByteArray(*hashBytes); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest hash"));
    }

    fs::FileInfo fileInfo;

    if (auto err = mFileInfoProvider->GetFileInfo(path, fileInfo, crypto::HashEnum::eSHA256); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (fileInfo.mCheckSum != *hashBytes) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidChecksum);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallBlob(const InstallItemInfo& itemInfo, const ItemBlobInfo& blob, size_t size)
{
    LOG_DBG() << "Install blob" << Log::Field("digest", blob.mDigest) << Log::Field("mediaType", blob.mMediaType)
              << Log::Field("size", size);

    if (auto err = LockBlob(blob.mDigest, itemInfo.mItemID, itemInfo.mVersion); !err.IsNone()) {
        return err;
    }

    auto unlockBlob = DeferRelease([this, &blob]() { UnlockBlob(blob.mDigest); });

    StaticString<cFilePathLen> path;

    if (auto err = GetBlobPath(blob.mDigest, path); !err.IsNone()) {
        return err;
    }

    if (auto err = AddItemBlob(itemInfo, blob); !err.IsNone()) {
        return err;
    }

    auto err = ValidateBlob(blob.mDigest, path);
    if (err.IsNone()) {
        return ErrorEnum::eNone;
    }

    LOG_DBG() << "Blob is not valid" << Log::Field("digest", blob.mDigest) << Log::Field(err);

    size_t removedSize = 0;

    if (Tie(removedSize, err) = RemoveBlob(blob.mDigest, path); !err.IsNone()) {
        return err;
    }

    if (removedSize > 0) {
        GetSpaceAllocator().FreeSpace(removedSize);
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

    auto finishInstall = DeferRelease([this, &path, &space, &err]() { FinishInstallBlob(path, space.Get(), err); });

    bool             notify = false;
    UpdateItemStatus status;

    {
        LockGuard lock {mMutex};

        if (auto* item = FindItemInProgress(itemInfo.mItemID, itemInfo.mVersion);
            item != nullptr && item->mNotified && item->mState == ItemStateEnum::eInstalling) {
            item->mState = ItemStateEnum::eDownloading;

            status = CreateItemStatus(item->mItemID, item->mType, item->mVersion, ItemStateEnum::eDownloading);
            notify = true;
        }
    }

    if (notify) {
        NotifyItemStatusChanged(status);
    }

    if (err = DownloadBlob(blob.mDigest, path); !err.IsNone()) {
        err = AOS_ERROR_WRAP(err);
    } else {
        err = ValidateBlob(blob.mDigest, path);
    }

    return err;
}

RetWithError<size_t> ImageManager::RemoveBlob(const String& digest, const String& path)
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

Error ImageManager::InstallLayer(
    const InstallItemInfo& itemInfo, const oci::ContentDescriptor& descriptor, const String& diffDigest)
{
    LOG_DBG() << "Install layer" << Log::Field("digest", descriptor.mDigest) << Log::Field("diffDigest", diffDigest)
              << Log::Field("size", descriptor.mSize);

    ItemBlobInfo blob;

    blob.mDigest    = descriptor.mDigest;
    blob.mMediaType = descriptor.mMediaType;
    blob.mDiffID    = diffDigest;
    blob.mIsLayer   = true;

    return InstallBlob(itemInfo, blob, descriptor.mSize);
}

RetWithError<size_t> ImageManager::RemoveLayer(
    const String& digest, const String& mediaType, const String& diffID, const String& path)
{
    LOG_DBG() << "Remove layer" << Log::Field("digest", digest) << Log::Field("mediaType", mediaType)
              << Log::Field("diffID", diffID) << Log::Field("path", path);

    return RemoveBlob(digest, path);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

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

void ImageManager::FinishInstallBlob(const String& path, spaceallocator::SpaceItf* space, const Error& err)
{
    if (err.IsNone()) {
        if (space) {
            if (auto acceptErr = space->Accept(); !acceptErr.IsNone()) {
                LOG_ERR() << "Can't accept space" << Log::Field(AOS_ERROR_WRAP(acceptErr));
            }
        }

        return;
    }

    LOG_ERR() << "Failed to install blob" << Log::Field("path", path) << Log::Field(err);

    if (auto removeErr = fs::RemoveAll(path); !removeErr.IsNone()) {
        LOG_ERR() << "Failed to remove blob" << Log::Field("path", path) << Log::Field(removeErr);
    }

    if (space) {
        if (auto releaseErr = space->Release(); !releaseErr.IsNone()) {
            LOG_ERR() << "Failed to release space" << Log::Field(AOS_ERROR_WRAP(releaseErr));
        }
    }
}

RetWithError<size_t> ImageManager::RemoveOutdatedItem(const String& itemID, const String& version)
{
    LOG_DBG() << "Remove outdated item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    return {0, ErrorEnum::eNotSupported};
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

Error ImageManager::LockItem(const String& itemID, const String& version)
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

void ImageManager::UnlockItem(const String& itemID, const String& version)
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

Error ImageManager::InstallItemBlobs(const InstallItemInfo& itemInfo)
{
    if (!itemInfo.mIsRootIndex) {
        return InstallManifestBlobs(itemInfo, itemInfo.mRootDigest, oci::cMediaTypeImageManifest, 0);
    }

    ItemBlobInfo blob;

    blob.mDigest    = itemInfo.mRootDigest;
    blob.mMediaType = oci::cMediaTypeImageIndex;

    if (auto err = InstallBlob(itemInfo, blob, 0); !err.IsNone()) {
        return err;
    }

    StaticString<cFilePathLen> path;

    if (auto err = GetBlobPath(blob.mDigest, path); !err.IsNone()) {
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
        if (auto err = InstallManifestBlobs(itemInfo, manifest.mDigest, manifest.mMediaType, manifest.mSize);
            !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallManifestBlobs(
    const InstallItemInfo& itemInfo, const String& digest, const String& mediaType, size_t size)
{
    LOG_DBG() << "Install manifest" << Log::Field("digest", digest);

    ItemBlobInfo blob;

    blob.mDigest    = digest;
    blob.mMediaType = mediaType;

    if (auto err = InstallBlob(itemInfo, blob, size); !err.IsNone()) {
        return err;
    }

    StaticString<cFilePathLen> path;

    if (auto err = GetBlobPath(digest, path); !err.IsNone()) {
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
        const auto& itemConfig = *manifest->mItemConfig;

        blob.mDigest    = itemConfig.mDigest;
        blob.mMediaType = itemConfig.mMediaType;

        if (auto err = InstallBlob(itemInfo, blob, itemConfig.mSize); !err.IsNone()) {
            return err;
        }
    }

    UniquePtr<oci::ImageConfig> imageConfig;

    if (!manifest->mConfig.mDigest.IsEmpty()) {
        const auto& config = manifest->mConfig;

        blob.mDigest    = config.mDigest;
        blob.mMediaType = config.mMediaType;

        if (auto err = InstallBlob(itemInfo, blob, config.mSize); !err.IsNone()) {
            return err;
        }

        if (auto err = GetBlobPath(config.mDigest, path); !err.IsNone()) {
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
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "diff ID is missing"));
        }
    }

    for (size_t i = 0; i < manifest->mLayers.Size(); ++i) {
        StaticString<oci::cDigestLen> diffID;

        if (imageConfig) {
            diffID = imageConfig->mRootfs.mDiffIDs[i];
        }

        if (auto err = InstallLayer(itemInfo, manifest->mLayers[i], diffID); !err.IsNone()) {
            return err;
        }
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

} // namespace aos::common::imagemanager
