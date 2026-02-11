/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "imagemanager.hpp"

namespace aos::cm::imagemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ImageManager::Init(const Config& config, StorageItf& storage, BlobInfoProviderItf& blobInfoProvider,
    spaceallocator::SpaceAllocatorItf& downloadingSpaceAllocator,
    spaceallocator::SpaceAllocatorItf& installSpaceAllocator, downloader::DownloaderItf& downloader,
    fileserver::FileServerItf& fileserver, crypto::CryptoHelperItf& cryptoHelper,
    fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec)
{
    LOG_DBG() << "Init image manager";

    mConfig                    = config;
    mStorage                   = &storage;
    mBlobInfoProvider          = &blobInfoProvider;
    mDownloadingSpaceAllocator = &downloadingSpaceAllocator;
    mInstallSpaceAllocator     = &installSpaceAllocator;
    mDownloader                = &downloader;
    mFileServer                = &fileserver;
    mCryptoHelper              = &cryptoHelper;
    mFileInfoProvider          = &fileInfoProvider;
    mOCISpec                   = &ociSpec;

    auto items = MakeUnique<StaticArray<ItemInfo, cMaxNumUpdateItems>>(&mAllocator);
    if (!items) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetAllItemsInfos(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mBlobsInstallPath = fs::JoinPath(mConfig.mInstallPath, cBlobsDirName);

    if (auto err = fs::MakeDirAll(mBlobsInstallPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mBlobsDownloadPath = fs::JoinPath(mConfig.mDownloadPath, cBlobsDirName);

    if (auto err = fs::MakeDirAll(mBlobsDownloadPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = AllocateSpaceForPartialDownloads(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    RegisterOutdatedItems(*items);

    auto [cleanupSize, cleanupErr] = CleanupOrphanedBlobs();
    if (!cleanupErr.IsNone()) {
        LOG_ERR() << "Failed to cleanup orphaned blobs" << Log::Field(cleanupErr);
    } else {
        LOG_DBG() << "Cleaned up orphaned blobs" << Log::Field("size", cleanupSize);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::Start()
{
    LOG_DBG() << "Start image manager";

    if (auto err = RemoveOutdatedItems(); !err.IsNone()) {
        LOG_ERR() << "Failed to remove outdated items during start" << Log::Field(err);
    }

    if (auto err = mTimer.Start(
            mConfig.mRemoveOutdatedPeriod,
            [this](void*) {
                if (auto err = RemoveOutdatedItems(); !err.IsNone()) {
                    LOG_ERR() << "Error removing outdated items" << Log::Field(err);
                }
            },
            false);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::Stop()
{
    LOG_DBG() << "Stop image manager";

    return mTimer.Stop();
}

Error ImageManager::DownloadUpdateItems(const Array<UpdateItemInfo>& itemsInfo,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains,
    Array<UpdateItemStatus>& statuses)
{
    LOG_INF() << "Download update items" << Log::Field("count", itemsInfo.Size());

    for (const auto& itemInfo : itemsInfo) {
        LOG_INF() << "Download update item" << Log::Field("itemID", itemInfo.mItemID)
                  << Log::Field("type", itemInfo.mType) << Log::Field("version", itemInfo.mVersion)
                  << Log::Field("indexDigest", itemInfo.mIndexDigest);
    }

    if (!StartAction()) {
        return ErrorEnum::eCanceled;
    }

    auto stopAction = DeferRelease(&mMutex, [this](void*) { StopAction(); });

    if (auto err = statuses.Resize(itemsInfo.Size()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (size_t i = 0; i < itemsInfo.Size(); i++) {
        statuses[i].mItemID  = itemsInfo[i].mItemID;
        statuses[i].mVersion = itemsInfo[i].mVersion;
        statuses[i].mState   = ItemStateEnum::eDownloading;
        statuses[i].mError   = ErrorEnum::eNone;
    }

    auto storedItems = MakeUnique<StaticArray<ItemInfo, cMaxNumUpdateItems>>(&mAllocator);
    if (!storedItems) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetAllItemsInfos(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (itemsInfo.Size() == 0) {
        auto err = RemovePendingItems(*storedItems, statuses);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to remove pending items" << Log::Field(err);
        }

        auto [cleanupSize, cleanupErr] = CleanupOrphanedBlobs();
        if (!cleanupErr.IsNone()) {
            LOG_ERR() << "Failed to cleanup orphaned blobs" << Log::Field(cleanupErr);

            if (err.IsNone()) {
                err = cleanupErr;
            }
        } else {
            LOG_DBG() << "Cleaned up orphaned blobs" << Log::Field("size", cleanupSize);
        }

        return err;
    }

    if (auto err = CleanupDownloadingItems(itemsInfo, *storedItems); !err.IsNone()) {
        LOG_ERR() << "Failed to cleanup downloading items" << Log::Field(err);
    }

    storedItems->Clear();

    if (auto err = mStorage->GetAllItemsInfos(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = VerifyStoredItems(itemsInfo, *storedItems, certificates, certificateChains, statuses);
        !err.IsNone()) {
        LOG_ERR() << "Failed to verify stored items" << Log::Field(err);

        return err;
    }

    if (auto err = ProcessDownloadRequest(itemsInfo, *storedItems, certificates, certificateChains, statuses);
        !err.IsNone()) {
        LOG_ERR() << "Failed to process download request" << Log::Field(err);

        return err;
    }

    auto [cleanupSize, cleanupErr] = CleanupOrphanedBlobs();
    if (!cleanupErr.IsNone()) {
        LOG_ERR() << "Failed to cleanup orphaned blobs" << Log::Field(cleanupErr);
    } else {
        LOG_DBG() << "Cleaned up orphaned blobs" << Log::Field("size", cleanupSize);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallUpdateItems(const Array<UpdateItemInfo>& itemsInfo, Array<UpdateItemStatus>& statuses)
{
    LOG_INF() << "Install update items" << Log::Field("count", itemsInfo.Size());

    for (const auto& itemInfo : itemsInfo) {
        LOG_INF() << "Install update item" << Log::Field("itemID", itemInfo.mItemID)
                  << Log::Field("type", itemInfo.mType) << Log::Field("version", itemInfo.mVersion);
    }

    if (!StartAction()) {
        return ErrorEnum::eCanceled;
    }

    auto stopAction = DeferRelease(&mMutex, [this](void*) { StopAction(); });

    if (auto err = statuses.Resize(itemsInfo.Size()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (size_t i = 0; i < itemsInfo.Size(); i++) {
        statuses[i].mItemID  = itemsInfo[i].mItemID;
        statuses[i].mVersion = itemsInfo[i].mVersion;
        statuses[i].mState   = ItemStateEnum::eInstalled;
        statuses[i].mError   = ErrorEnum::eNone;
    }

    auto storedItems = MakeUnique<StaticArray<ItemInfo, cMaxNumUpdateItems>>(&mAllocator);
    if (!storedItems) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetAllItemsInfos(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = RemoveDifferentVersions(itemsInfo, *storedItems); !err.IsNone()) {
        LOG_ERR() << "Failed to remove different versions" << Log::Field(err);
    }

    storedItems->Clear();

    if (auto err = mStorage->GetAllItemsInfos(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = VerifyBlobsIntegrity(itemsInfo, *storedItems, statuses); !err.IsNone()) {
        LOG_ERR() << "Failed to verify blobs integrity" << Log::Field(err);
    }

    storedItems->Clear();

    if (auto err = mStorage->GetAllItemsInfos(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetItemsToInstalled(itemsInfo, *storedItems); !err.IsNone()) {
        LOG_ERR() << "Failed to set items to installed" << Log::Field(err);
    }

    storedItems->Clear();

    if (auto err = mStorage->GetAllItemsInfos(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetItemsToRemoved(itemsInfo, *storedItems); !err.IsNone()) {
        LOG_ERR() << "Failed to set items to removed" << Log::Field(err);
    }

    auto [cleanupSize, cleanupErr] = CleanupOrphanedBlobs();
    if (!cleanupErr.IsNone()) {
        LOG_ERR() << "Failed to cleanup orphaned blobs" << Log::Field(cleanupErr);
    } else {
        LOG_DBG() << "Cleaned up orphaned blobs" << Log::Field("size", cleanupSize);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::Cancel()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Cancel image manager downloading";

    mCancel = true;
    mCondVar.NotifyAll();

    if (!mCurrentDownloadDigest.IsEmpty()) {
        if (auto err = mDownloader->Cancel(mCurrentDownloadDigest); !err.IsNone()) {
            LOG_ERR() << "Failed to cancel downloader" << Log::Field("digest", mCurrentDownloadDigest)
                      << Log::Field(err);

            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetUpdateItemsStatuses(Array<UpdateItemStatus>& statuses)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get update items statuses";

    auto items = MakeUnique<StaticArray<ItemInfo, cMaxNumUpdateItems>>(&mAllocator);
    if (!items) {
        return ErrorEnum::eNoMemory;
    }

    if (auto err = mStorage->GetAllItemsInfos(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        UpdateItemStatus status;

        status.mItemID  = item.mItemID;
        status.mVersion = item.mVersion;
        status.mState   = item.mState;
        status.mError   = ErrorEnum::eNone;

        if (auto err = statuses.PushBack(status); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SubscribeListener(ItemStatusListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe item status listener";

    if (mListeners.Find(&listener) != mListeners.end()) {
        return ErrorEnum::eAlreadyExist;
    }

    if (auto err = mListeners.PushBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::UnsubscribeListener(ItemStatusListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribed item status listener";

    return mListeners.Remove(&listener) != 0 ? ErrorEnum::eNone : ErrorEnum::eNotFound;
}

Error ImageManager::GetIndexDigest(const String& itemID, const String& version, String& digest) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get index digest" << Log::Field("itemID", itemID) << Log::Field("version", version);

    auto items = MakeUnique<StaticArray<ItemInfo, cMaxNumItemVersions>>(&mAllocator);
    if (!items) {
        return ErrorEnum::eNoMemory;
    }

    if (auto err = mStorage->GetItemInfos(itemID, *items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto it = items->FindIf(
        [&](const auto& item) { return item.mVersion == version && item.mState != ItemStateEnum::eDownloading; });

    if (it == items->end()) {
        return ErrorEnum::eNotFound;
    }

    digest = it->mIndexDigest;

    return ErrorEnum::eNone;
}

Error ImageManager::GetBlobPath(const String& digest, String& path) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get blob path" << Log::Field("digest", digest);

    StaticString<cFilePathLen> blobPath;
    if (auto err = GetBlobFilePath(mBlobsInstallPath, digest, blobPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    path = blobPath;

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return exists ? ErrorEnum::eNone : ErrorEnum::eNotFound;
}

Error ImageManager::GetBlobURL(const String& digest, String& url) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get blob URL" << Log::Field("digest", digest);

    StaticString<cFilePathLen> blobPath;
    if (auto err = GetBlobFilePath(mBlobsInstallPath, digest, blobPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [exists, existErr] = fs::FileExist(blobPath);
    if (!existErr.IsNone()) {
        return AOS_ERROR_WRAP(existErr);
    }

    if (!exists) {
        return ErrorEnum::eNotFound;
    }

    return mFileServer->TranslateFilePathURL(blobPath, url);
}

Error ImageManager::GetItemCurrentVersion(const String& itemID, String& version) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get item current version" << Log::Field("itemID", itemID);

    auto items = MakeUnique<StaticArray<ItemInfo, cMaxNumItemVersions>>(&mAllocator);
    if (!items) {
        return ErrorEnum::eNoMemory;
    }

    if (auto err = mStorage->GetItemInfos(itemID, *items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto it = items->FindIf([&](const auto& item) { return item.mState == ItemStateEnum::ePending; });
    if (it == items->end()) {
        it = items->FindIf([&](const auto& item) { return item.mState == ItemStateEnum::eInstalled; });
    }

    if (it == items->end()) {
        return ErrorEnum::eNotFound;
    }

    version = it->mVersion;

    return ErrorEnum::eNone;
}

RetWithError<size_t> ImageManager::RemoveItem(const String& id, const String& version)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove item" << Log::Field("id", id) << Log::Field("version", version);

    auto storedItems = MakeUnique<StaticArray<ItemInfo, cMaxNumUpdateItems>>(&mAllocator);
    if (!storedItems) {
        return {0, AOS_ERROR_WRAP(ErrorEnum::eNoMemory)};
    }

    if (auto err = mStorage->GetItemInfos(id, *storedItems); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    auto itemIt = storedItems->FindIf([](const auto& item) { return item.mState == ItemStateEnum::eRemoved; });

    if (itemIt == storedItems->end()) {
        return {0, ErrorEnum::eNotFound};
    }

    const auto& itemToRemove = *itemIt;

    if (auto err = mStorage->RemoveItem(itemToRemove.mItemID, itemToRemove.mVersion); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (auto err = mInstallSpaceAllocator->RestoreOutdatedItem(itemToRemove.mItemID, itemToRemove.mVersion);
        !err.IsNone()) {
        LOG_ERR() << "Failed to restore outdated item" << Log::Field("itemID", itemToRemove.mItemID) << Log::Field(err);
    }

    auto [totalSize, cleanupErr] = CleanupOrphanedBlobs();
    if (!cleanupErr.IsNone()) {
        return {0, AOS_ERROR_WRAP(cleanupErr)};
    }

    LOG_DBG() << "Item removed successfully" << Log::Field("id", id) << Log::Field("totalSize", totalSize);

    for (auto* listener : mListeners) {
        listener->OnItemRemoved(id);
    }

    return {totalSize, ErrorEnum::eNone};
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ImageManager::RemoveOutdatedItems()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove outdated items";

    auto items = MakeUnique<StaticArray<ItemInfo, cMaxNumUpdateItems>>(&mAllocator);
    if (!items) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mStorage->GetAllItemsInfos(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    bool hasRemovedItems = false;

    for (const auto& item : *items) {
        if (item.mState == ItemStateEnum::eRemoved && item.mTimestamp.Add(mConfig.mUpdateItemTTL) < Time::Now()) {
            LOG_DBG() << "Removing outdated item" << Log::Field("itemID", item.mItemID)
                      << Log::Field("version", item.mVersion);

            if (auto err = mStorage->RemoveItem(item.mItemID, item.mVersion); !err.IsNone()) {
                LOG_ERR() << "Failed to remove outdated item" << Log::Field("itemID", item.mItemID)
                          << Log::Field("version", item.mVersion) << Log::Field(err);

                continue;
            }

            if (auto err = mInstallSpaceAllocator->RestoreOutdatedItem(item.mItemID, item.mVersion); !err.IsNone()) {
                LOG_ERR() << "Failed to restore outdated item" << Log::Field("itemID", item.mItemID) << Log::Field(err);
            }

            for (auto* listener : mListeners) {
                listener->OnItemRemoved(item.mItemID);
            }

            hasRemovedItems = true;
        }
    }

    if (hasRemovedItems) {
        auto [totalSize, err] = CleanupOrphanedBlobs();
        if (!err.IsNone()) {
            return err;
        }

        LOG_DBG() << "Cleaned up orphaned blobs" << Log::Field("size", totalSize);

        mInstallSpaceAllocator->FreeSpace(totalSize);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::WaitForStop()
{
    UniqueLock<Mutex> lock(mMutex);

    mCondVar.Wait(lock, cRetryTimeout, [this]() { return mCancel; });

    if (mCancel) {
        return ErrorEnum::eCanceled;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::AllocateSpaceForPartialDownloads()
{
    LOG_DBG() << "Allocate space for partial downloads" << Log::Field("path", mBlobsDownloadPath);

    auto algorithmDirIterator = fs::DirIterator(mBlobsDownloadPath);

    while (algorithmDirIterator.Next()) {
        auto algorithm    = algorithmDirIterator->mPath;
        auto algorithmDir = fs::JoinPath(mBlobsDownloadPath, algorithm);

        auto fileIterator = fs::DirIterator(algorithmDir);

        while (fileIterator.Next()) {
            auto fileName = fileIterator->mPath;
            auto filePath = fs::JoinPath(algorithmDir, fileName);

            auto [fileSize, sizeErr] = fs::CalculateSize(filePath);
            if (!sizeErr.IsNone()) {
                LOG_WRN() << "Failed to get size for partial download" << Log::Field("path", filePath)
                          << Log::Field(sizeErr);

                continue;
            }

            if (fileSize == 0) {
                continue;
            }

            UniquePtr<spaceallocator::SpaceItf> space;
            Error                               err;

            if (Tie(space, err) = mDownloadingSpaceAllocator->AllocateSpace(fileSize); !err.IsNone()) {
                LOG_ERR() << "Failed to allocate space for partial download" << Log::Field("path", filePath)
                          << Log::Field("size", fileSize) << Log::Field(err);

                return AOS_ERROR_WRAP(err);
            }

            space->Accept();

            LOG_DBG() << "Allocated space for partial download" << Log::Field("path", filePath)
                      << Log::Field("size", fileSize);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::RemovePendingItems(const Array<ItemInfo>& storedItems, Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Remove pending items";

    for (const auto& storedItem : storedItems) {
        if (storedItem.mState != ItemStateEnum::ePending) {
            continue;
        }

        LOG_DBG() << "Removing pending item" << Log::Field("itemID", storedItem.mItemID)
                  << Log::Field("version", storedItem.mVersion);

        if (auto err = mStorage->RemoveItem(storedItem.mItemID, storedItem.mVersion); !err.IsNone()) {
            LOG_ERR() << "Failed to remove pending item from storage" << Log::Field("itemID", storedItem.mItemID)
                      << Log::Field("version", storedItem.mVersion) << Log::Field(err);

            continue;
        }

        UpdateItemStatus status;
        status.mItemID  = storedItem.mItemID;
        status.mVersion = storedItem.mVersion;
        status.mState   = ItemStateEnum::eRemoved;
        status.mError   = ErrorEnum::eNone;

        if (auto err = statuses.PushBack(status); !err.IsNone()) {
            LOG_ERR() << "Failed to add status to statuses array" << Log::Field(err);
        }

        NotifyItemStatusChanged(storedItem.mItemID, storedItem.mVersion, ItemStateEnum::eRemoved, ErrorEnum::eNone);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::CleanupDownloadingItems(
    const Array<UpdateItemInfo>& currentItems, const Array<ItemInfo>& storedItems)
{
    LOG_DBG() << "Cleanup downloading items";

    for (const auto& storedItem : storedItems) {
        if (storedItem.mState != ItemStateEnum::eDownloading && storedItem.mState != ItemStateEnum::ePending) {
            continue;
        }

        auto isInCurrentRequest = currentItems.FindIf([&storedItem](const auto& currentItem) {
            return storedItem.mItemID == currentItem.mItemID && storedItem.mVersion == currentItem.mVersion;
        }) != currentItems.end();

        if (!isInCurrentRequest) {
            LOG_DBG() << "Removing stale item" << Log::Field("itemID", storedItem.mItemID)
                      << Log::Field("version", storedItem.mVersion)
                      << Log::Field("state", ItemState(storedItem.mState));

            if (auto err = mStorage->RemoveItem(storedItem.mItemID, storedItem.mVersion); !err.IsNone()) {
                LOG_ERR() << "Failed to remove item from storage" << Log::Field("itemID", storedItem.mItemID)
                          << Log::Field("version", storedItem.mVersion) << Log::Field(err);
            }

            if (!storedItem.mIndexDigest.IsEmpty()) {
                StaticString<cFilePathLen> filePath;
                if (auto err = GetBlobFilePath(mBlobsInstallPath, storedItem.mIndexDigest, filePath); !err.IsNone()) {
                    LOG_ERR() << "Failed to get blob file path" << Log::Field(err);
                } else {
                    LOG_DBG() << "Remove blob" << Log::Field("path", filePath);

                    if (err = fs::RemoveAll(filePath); !err.IsNone()) {
                        LOG_ERR() << "Failed to remove blob" << Log::Field("path", filePath) << Log::Field(err);
                    }
                }
            }
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::VerifyStoredItems(const Array<UpdateItemInfo>& itemsInfo, Array<ItemInfo>& storedItems,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains,
    Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Verify stored items";

    for (auto& storedItem : storedItems) {
        if (storedItem.mState == ItemStateEnum::eInstalled || storedItem.mState == ItemStateEnum::ePending) {
            auto itemIt = itemsInfo.FindIf([&storedItem](const auto& item) {
                return item.mItemID == storedItem.mItemID && item.mVersion == storedItem.mVersion;
            });

            UpdateItemInfo itemInfo;
            itemInfo.mItemID      = storedItem.mItemID;
            itemInfo.mVersion     = storedItem.mVersion;
            itemInfo.mIndexDigest = storedItem.mIndexDigest;

            LOG_DBG() << "Verify stored item" << Log::Field("id", storedItem.mItemID)
                      << Log::Field("state", ItemState(storedItem.mState));

            if (auto err = DownloadItem(itemInfo, certificates, certificateChains); !err.IsNone()) {
                LOG_ERR() << "Failed to verify/download item" << Log::Field("id", storedItem.mItemID)
                          << Log::Field(err);

                if (itemIt != itemsInfo.end()) {
                    auto statusIdx             = itemIt - itemsInfo.begin();
                    statuses[statusIdx].mState = ItemStateEnum::eFailed;
                    statuses[statusIdx].mError = err;
                }

                if (auto updateErr
                    = mStorage->UpdateItemState(storedItem.mItemID, storedItem.mVersion, ItemStateEnum::eFailed);
                    !updateErr.IsNone()) {
                    LOG_ERR() << "Failed to update item state" << Log::Field(updateErr);
                } else {
                    storedItem.mState = ItemStateEnum::eFailed;
                }

                NotifyItemStatusChanged(storedItem.mItemID, storedItem.mVersion, ItemStateEnum::eFailed, err);

                if (err == ErrorEnum::eCanceled) {
                    return err;
                }
            } else {
                if (itemIt != itemsInfo.end()) {
                    auto statusIdx             = itemIt - itemsInfo.begin();
                    statuses[statusIdx].mState = storedItem.mState;
                    statuses[statusIdx].mError = ErrorEnum::eNone;
                }
            }
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ProcessDownloadRequest(const Array<UpdateItemInfo>& itemsInfo, Array<ItemInfo>& storedItems,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains,
    Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Process download request";

    for (size_t i = 0; i < itemsInfo.Size(); i++) {
        const auto& itemInfo = itemsInfo[i];

        LOG_DBG() << "Process item" << Log::Field("id", itemInfo.mItemID) << Log::Field("version", itemInfo.mVersion);

        auto installedOrPendingIt = storedItems.FindIf([&itemInfo](const auto& stored) {
            return stored.mItemID == itemInfo.mItemID && stored.mVersion == itemInfo.mVersion
                && (stored.mState == ItemStateEnum::eInstalled || stored.mState == ItemStateEnum::ePending);
        });

        if (installedOrPendingIt != storedItems.end()) {
            LOG_DBG() << "Item already processed in first loop, skipping";

            continue;
        }

        auto oldVersionIt = storedItems.FindIf([&itemInfo](const auto& stored) {
            return stored.mItemID == itemInfo.mItemID && stored.mVersion != itemInfo.mVersion
                && (stored.mState == ItemStateEnum::ePending || stored.mState == ItemStateEnum::eFailed
                    || stored.mState == ItemStateEnum::eDownloading);
        });

        if (oldVersionIt != storedItems.end()) {
            LOG_DBG() << "Removing old version" << Log::Field("id", oldVersionIt->mItemID)
                      << Log::Field("version", oldVersionIt->mVersion);

            if (auto removeErr = mStorage->RemoveItem(oldVersionIt->mItemID, oldVersionIt->mVersion);
                !removeErr.IsNone()) {
                LOG_ERR() << "Failed to remove old version" << Log::Field(removeErr);
            } else {
                storedItems.Erase(oldVersionIt);
            }
        }

        auto sameVersionIt = storedItems.FindIf([&itemInfo](const auto& stored) {
            return stored.mItemID == itemInfo.mItemID && stored.mVersion == itemInfo.mVersion
                && (stored.mState == ItemStateEnum::eDownloading || stored.mState == ItemStateEnum::eFailed
                    || stored.mState == ItemStateEnum::eRemoved);
        });

        if (sameVersionIt == storedItems.end()) {
            ItemInfo newItem;
            newItem.mItemID      = itemInfo.mItemID;
            newItem.mVersion     = itemInfo.mVersion;
            newItem.mIndexDigest = itemInfo.mIndexDigest;
            newItem.mState       = ItemStateEnum::eDownloading;
            newItem.mTimestamp   = Time::Now();

            if (auto addErr = mStorage->AddItem(newItem); !addErr.IsNone()) {
                LOG_ERR() << "Failed to add new item" << Log::Field(addErr);

                continue;
            }
        } else if (sameVersionIt->mState == ItemStateEnum::eRemoved) {
            if (auto updateErr
                = mStorage->UpdateItemState(itemInfo.mItemID, itemInfo.mVersion, ItemStateEnum::eDownloading);
                !updateErr.IsNone()) {
                LOG_ERR() << "Failed to update removed item state" << Log::Field(updateErr);

                continue;
            }

            if (auto err = mInstallSpaceAllocator->RestoreOutdatedItem(itemInfo.mItemID, itemInfo.mVersion);
                !err.IsNone()) {
                LOG_ERR() << "Failed to restore outdated item" << Log::Field("itemID", itemInfo.mItemID)
                          << Log::Field("version", itemInfo.mVersion) << Log::Field(err);
            }
        }

        NotifyItemStatusChanged(itemInfo.mItemID, itemInfo.mVersion, ItemStateEnum::eDownloading, ErrorEnum::eNone);

        auto downloadErr = DownloadItem(itemInfo, certificates, certificateChains);

        auto finalState = downloadErr.IsNone() ? ItemStateEnum::ePending : ItemStateEnum::eFailed;
        if (auto updateErr = mStorage->UpdateItemState(itemInfo.mItemID, itemInfo.mVersion, finalState);
            !updateErr.IsNone()) {
            LOG_ERR() << "Failed to update item state" << Log::Field(updateErr);
        }

        NotifyItemStatusChanged(itemInfo.mItemID, itemInfo.mVersion, finalState, downloadErr);

        if (!downloadErr.IsNone()) {
            LOG_ERR() << "Failed to download item" << Log::Field("id", itemInfo.mItemID)
                      << Log::Field("version", itemInfo.mVersion) << Log::Field(downloadErr);

            if (downloadErr == ErrorEnum::eCanceled) {
                return downloadErr;
            }
        }

        statuses[i].mState = finalState;
        statuses[i].mError = downloadErr;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::DownloadItem(const UpdateItemInfo& itemInfo, const Array<crypto::CertificateInfo>& certificates,
    const Array<crypto::CertificateChainInfo>& certificateChains)
{
    LOG_DBG() << "Download item" << Log::Field("itemID", itemInfo.mItemID) << Log::Field("version", itemInfo.mVersion)
              << Log::Field("digest", itemInfo.mIndexDigest);

    mCurrentItemID      = itemInfo.mItemID;
    mCurrentItemVersion = itemInfo.mVersion;

    auto clearCurrentItem = DeferRelease(&mMutex, [this](void*) {
        mCurrentItemID.Clear();
        mCurrentItemVersion.Clear();
    });

    StaticString<cFilePathLen> downloadPath;
    if (auto err = GetBlobFilePath(mBlobsDownloadPath, itemInfo.mIndexDigest, downloadPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cFilePathLen> installPath;
    if (auto err = GetBlobFilePath(mBlobsInstallPath, itemInfo.mIndexDigest, installPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);
    if (!imageIndex) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err
        = LoadIndex(itemInfo.mIndexDigest, downloadPath, installPath, certificates, certificateChains, *imageIndex);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Processing manifests" << Log::Field("count", imageIndex->mManifests.Size());

    for (const auto& manifestDescriptor : imageIndex->mManifests) {
        auto manifest = MakeUnique<oci::ImageManifest>(&mAllocator);
        if (!manifest) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        if (auto err = LoadManifest(manifestDescriptor.mDigest, certificates, certificateChains, *manifest);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = LoadBlob(manifest->mConfig, certificates, certificateChains); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (manifest->mItemConfig.HasValue()) {
            if (auto err = LoadBlob(*manifest->mItemConfig, certificates, certificateChains); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        if (auto err = LoadLayers(manifest->mLayers, certificates, certificateChains); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    LOG_DBG() << "Successfully processed item" << Log::Field("itemID", itemInfo.mItemID)
              << Log::Field("version", itemInfo.mVersion);

    return ErrorEnum::eNone;
}

Error ImageManager::LoadIndex(const String& digest, const String& downloadPath, const String& installPath,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains,
    oci::ImageIndex& imageIndex)
{
    LOG_DBG() << "Load index" << Log::Field("digest", digest);

    Error                               err;
    UniquePtr<spaceallocator::SpaceItf> space;

    auto releaseSpace = DeferRelease(&err, [&](const Error* err) {
        if (space && !err->IsNone()) {
            LOG_ERR() << "Failed to load index" << Log::Field("digest", digest) << Log::Field(*err);

            if (auto removeErr = fs::RemoveAll(installPath); !removeErr.IsNone()) {
                LOG_ERR() << "Failed to remove install file" << Log::Field("path", installPath)
                          << Log::Field(removeErr);
            }

            space->Release();

            return;
        }

        if (space) {
            space->Accept();
        }
    });

    if (err = EnsureBlob(digest, downloadPath, installPath, certificates, certificateChains, space); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = mOCISpec->LoadImageIndex(installPath, imageIndex); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::LoadManifest(const String& digest, const Array<crypto::CertificateInfo>& certificates,
    const Array<crypto::CertificateChainInfo>& certificateChains, oci::ImageManifest& manifest)
{
    LOG_DBG() << "Load manifest" << Log::Field("digest", digest);

    Error                               err;
    UniquePtr<spaceallocator::SpaceItf> space;

    StaticString<cFilePathLen> downloadPath;
    if (err = GetBlobFilePath(mBlobsDownloadPath, digest, downloadPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cFilePathLen> installPath;
    if (err = GetBlobFilePath(mBlobsInstallPath, digest, installPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto releaseSpace = DeferRelease(&err, [&](const Error* err) {
        if (space && !err->IsNone()) {
            LOG_ERR() << "Failed to load manifest" << Log::Field("digest", digest) << Log::Field(*err);

            if (auto removeErr = fs::RemoveAll(installPath); !removeErr.IsNone()) {
                LOG_ERR() << "Failed to remove install file" << Log::Field("path", installPath)
                          << Log::Field(removeErr);
            }

            space->Release();

            return;
        }

        if (space) {
            space->Accept();
        }
    });

    if (err = EnsureBlob(digest, downloadPath, installPath, certificates, certificateChains, space); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = mOCISpec->LoadImageManifest(installPath, manifest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::LoadBlob(const oci::ContentDescriptor& descriptor,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains)
{
    LOG_DBG() << "Load blob" << Log::Field("digest", descriptor.mDigest);

    Error                               err;
    UniquePtr<spaceallocator::SpaceItf> space;

    StaticString<cFilePathLen> downloadPath;
    if (err = GetBlobFilePath(mBlobsDownloadPath, descriptor.mDigest, downloadPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cFilePathLen> installPath;
    if (err = GetBlobFilePath(mBlobsInstallPath, descriptor.mDigest, installPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto releaseSpace = DeferRelease(&err, [&](const Error* err) {
        if (space && !err->IsNone()) {
            LOG_ERR() << "Failed to load blob" << Log::Field("digest", descriptor.mDigest) << Log::Field(*err);

            if (auto removeErr = fs::RemoveAll(installPath); !removeErr.IsNone()) {
                LOG_ERR() << "Failed to remove install file" << Log::Field("path", installPath)
                          << Log::Field(removeErr);
            }

            space->Release();
            return;
        }

        if (space) {
            space->Accept();
        }
    });

    if (err = EnsureBlob(descriptor.mDigest, downloadPath, installPath, certificates, certificateChains, space);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::LoadLayers(const Array<oci::ContentDescriptor>& layers,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains)
{
    LOG_DBG() << "Load layers" << Log::Field("count", layers.Size());

    for (const auto& layer : layers) {
        if (auto err = LoadBlob(layer, certificates, certificateChains); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::EnsureBlob(const String& digest, const String& downloadPath, const String& installPath,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains,
    UniquePtr<spaceallocator::SpaceItf>& space)
{
    LOG_DBG() << "Ensure blob" << Log::Field("digest", digest);

    BlobInfo                            blobInfo;
    UniquePtr<spaceallocator::SpaceItf> downloadingSpace;

    if (auto err = DownloadBlob(digest, downloadPath, installPath, blobInfo, downloadingSpace); !err.IsNone()) {
        if (err == ErrorEnum::eAlreadyExist) {
            return ErrorEnum::eNone;
        }

        return AOS_ERROR_WRAP(err);
    }

    auto err = DecryptAndValidateBlob(downloadPath, installPath, blobInfo, certificates, certificateChains, space);

    downloadingSpace->Release();

    if (auto removeErr = fs::RemoveAll(downloadPath); !removeErr.IsNone()) {
        LOG_ERR() << "Failed to remove download path" << Log::Field("path", downloadPath) << Log::Field(removeErr);
    }

    return AOS_ERROR_WRAP(err);
}

Error ImageManager::GetBlobInfo(const String& digest, BlobInfo& blobInfo)
{
    StaticArray<StaticString<oci::cDigestLen>, 1> digests;
    if (auto err = digests.PushBack(digest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto blobsInfo = MakeUnique<StaticArray<BlobInfo, 1>>(&mAllocator);
    if (!blobsInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    while (true) {
        auto err = mBlobInfoProvider->GetBlobsInfos(digests, *blobsInfo);

        if (!err.IsNone()) {
            LOG_ERR() << "Failed to get blobs info" << Log::Field("digest", digest) << Log::Field(err);

            if (auto waitErr = WaitForStop(); !waitErr.IsNone()) {
                return AOS_ERROR_WRAP(waitErr);
            }

            LOG_DBG() << "Retrying get blobs info" << Log::Field("digest", digest);

            continue;
        }

        break;
    }

    if (mCancel) {
        return ErrorEnum::eCanceled;
    }

    if (blobsInfo->IsEmpty()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    blobInfo = (*blobsInfo)[0];

    return ErrorEnum::eNone;
}

Error ImageManager::CheckExistingBlob(const String& installPath)
{
    auto [installExists, checkInstallErr] = fs::FileExist(installPath);
    if (!checkInstallErr.IsNone()) {
        return AOS_ERROR_WRAP(checkInstallErr);
    }

    if (!installExists) {
        return ErrorEnum::eNone;
    }

    fs::FileInfo fileInfo;

    if (auto err = mFileInfoProvider->GetFileInfo(installPath, fileInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<oci::cDigestLen> fileName;
    if (auto err = fs::BaseName(installPath, fileName); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto expectedSHA256 = MakeUnique<StaticArray<uint8_t, crypto::cSHA256Size>>(&mAllocator);
    if (!expectedSHA256) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = fileName.HexToByteArray(*expectedSHA256); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (fileInfo.mSHA256 == *expectedSHA256) {
        return ErrorEnum::eAlreadyExist;
    }

    LOG_WRN() << "Blob exists but SHA256 mismatch, will redownload";

    if (auto removeErr = fs::RemoveAll(installPath); !removeErr.IsNone()) {
        LOG_ERR() << "Failed to remove file with mismatched SHA256" << Log::Field(removeErr);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::PrepareDownloadSpace(const String& downloadPath, const BlobInfo& blobInfo,
    size_t& partialDownloadSize, UniquePtr<spaceallocator::SpaceItf>& downloadingSpace)
{
    auto [downloadExists, checkDownloadErr] = fs::FileExist(downloadPath);
    if (!checkDownloadErr.IsNone()) {
        return AOS_ERROR_WRAP(checkDownloadErr);
    }

    partialDownloadSize = 0;

    if (downloadExists) {
        auto [dirSize, getSizeErr] = fs::CalculateSize(downloadPath);
        if (!getSizeErr.IsNone()) {
            return AOS_ERROR_WRAP(getSizeErr);
        }

        partialDownloadSize = dirSize;
    }

    mDownloadingSpaceAllocator->FreeSpace(partialDownloadSize);

    Error err;

    Tie(downloadingSpace, err) = mDownloadingSpaceAllocator->AllocateSpace(blobInfo.mSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::PerformDownload(const BlobInfo& blobInfo, const String& downloadPath, size_t partialDownloadSize,
    UniquePtr<spaceallocator::SpaceItf>& downloadingSpace)
{
    {
        LockGuard lock {mMutex};

        mCurrentDownloadDigest = blobInfo.mDigest;
    }

    auto clearDigest = DeferRelease(&mMutex, [this](Mutex* mutex) {
        LockGuard lock {*mutex};

        mCurrentDownloadDigest.Clear();
    });

    StaticString<cFilePathLen> downloadDir;

    if (auto err = fs::ParentPath(downloadPath, downloadDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::MakeDirAll(downloadDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    while (true) {
        auto err = mDownloader->Download(blobInfo.mDigest, blobInfo.mURLs[0], downloadPath);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to download" << Log::Field("url", blobInfo.mURLs[0])
                      << Log::Field("path", downloadPath) << Log::Field(AOS_ERROR_WRAP(err));

            if (err = WaitForStop(); !err.IsNone()) {
                auto [newPartialSize, retrySizeErr] = fs::CalculateSize(downloadPath);
                if (!retrySizeErr.IsNone()) {
                    LOG_WRN() << "Failed to get partial download size" << Log::Field("path", downloadPath)
                              << Log::Field(retrySizeErr);

                    downloadingSpace->Release();

                    return err;
                }

                downloadingSpace->Release();

                Error allocationErr;

                Tie(downloadingSpace, allocationErr)
                    = mDownloadingSpaceAllocator->AllocateSpace(newPartialSize - partialDownloadSize);
                if (!allocationErr.IsNone()) {
                    return AOS_ERROR_WRAP(allocationErr);
                }

                downloadingSpace->Accept();

                return err;
            }

            LOG_DBG() << "Retrying download" << Log::Field("url", blobInfo.mURLs[0])
                      << Log::Field("path", downloadPath);

            continue;
        }

        break;
    }

    LOG_DBG() << "Downloaded successfully" << Log::Field("path", downloadPath);

    return ErrorEnum::eNone;
}

Error ImageManager::DownloadBlob(const String& digest, const String& downloadPath, const String& installPath,
    BlobInfo& blobInfo, UniquePtr<spaceallocator::SpaceItf>& downloadingSpace)
{
    LOG_DBG() << "Download blob" << Log::Field("digest", digest);

    if (mCancel) {
        return ErrorEnum::eCanceled;
    }

    if (auto err = CheckExistingBlob(installPath); !err.IsNone()) {
        if (err == ErrorEnum::eAlreadyExist) {
            return err;
        }

        return AOS_ERROR_WRAP(err);
    }

    if (auto err = GetBlobInfo(digest, blobInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    size_t partialDownloadSize = 0;

    if (auto err = PrepareDownloadSpace(downloadPath, blobInfo, partialDownloadSize, downloadingSpace); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = PerformDownload(blobInfo, downloadPath, partialDownloadSize, downloadingSpace); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::DecryptAndValidateBlob(const String& downloadPath, const String& installPath,
    const BlobInfo& blobInfo, const Array<crypto::CertificateInfo>& certificates,
    const Array<crypto::CertificateChainInfo>& certificateChains, UniquePtr<spaceallocator::SpaceItf>& installSpace)
{
    LOG_DBG() << "Decrypt and validate blob" << Log::Field("downloadPath", downloadPath)
              << Log::Field("installPath", installPath);

    Error err;

    Tie(installSpace, err) = mInstallSpaceAllocator->AllocateSpace(blobInfo.mSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cFilePathLen> installDir;

    if (err = fs::ParentPath(installPath, installDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = fs::MakeDirAll(installDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (blobInfo.mDecryptInfo.HasValue()) {
        if (err = mCryptoHelper->Decrypt(downloadPath, installPath, *blobInfo.mDecryptInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        if (err = fs::Rename(downloadPath, installPath); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    LOG_DBG() << "Decrypted successfully" << Log::Field("path", installPath);

    if (blobInfo.mSignInfo.HasValue()) {
        if (err = mCryptoHelper->ValidateSigns(installPath, *blobInfo.mSignInfo, certificateChains, certificates);
            !err.IsNone()) {
            if (auto removeErr = fs::RemoveAll(installPath); !removeErr.IsNone()) {
                LOG_ERR() << "Failed to remove install file" << Log::Field("path", installPath)
                          << Log::Field(removeErr);
            }

            return AOS_ERROR_WRAP(err);
        }
    }

    fs::FileInfo fileInfo;

    if (err = mFileInfoProvider->GetFileInfo(installPath, fileInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (fileInfo.mSHA256 != blobInfo.mSHA256) {
        return ErrorEnum::eInvalidChecksum;
    }

    LOG_DBG() << "Validated successfully" << Log::Field("path", installPath);

    return ErrorEnum::eNone;
}

bool ImageManager::StartAction()
{
    UniqueLock lock {mMutex};

    mCondVar.Wait(lock, [this]() { return !mInProgress || mCancel; });

    if (mCancel) {
        mCancel = false;

        return false;
    }

    mInProgress = true;

    return true;
}

void ImageManager::StopAction()
{
    LockGuard lock {mMutex};

    mInProgress = false;
    mCondVar.NotifyAll();
}

void ImageManager::NotifyItemsStatusesChanged(const Array<UpdateItemStatus>& statuses)
{
    LockGuard lock {mMutex};

    for (const auto& status : statuses) {
        LOG_DBG() << "Item status changed" << Log::Field("itemID", status.mItemID)
                  << Log::Field("version", status.mVersion) << Log::Field("state", ItemState(status.mState))
                  << Log::Field("error", status.mError);
    }

    for (auto* listener : mListeners) {
        listener->OnItemsStatusesChanged(statuses);
    }
}

void ImageManager::NotifyItemStatusChanged(
    const String& itemID, const String& version, ItemStateEnum state, const Error& error)
{
    StaticArray<UpdateItemStatus, 1> status;

    status.Resize(1);

    status[0].mItemID  = itemID;
    status[0].mVersion = version;
    status[0].mState   = state;
    status[0].mError   = error;

    NotifyItemsStatusesChanged(status);
}

void ImageManager::RegisterOutdatedItems(const Array<ItemInfo>& items)
{
    for (const auto& item : items) {
        if (item.mState == ItemStateEnum::eRemoved) {
            if (auto err = mInstallSpaceAllocator->AddOutdatedItem(item.mItemID, item.mVersion, item.mTimestamp);
                !err.IsNone()) {
                LOG_ERR() << "Failed to add outdated item" << Log::Field("itemID", item.mItemID)
                          << Log::Field("version", item.mVersion) << Log::Field(err);
            }
        }
    }
}

Error ImageManager::VerifyBlobIntegrity(const String& digest)
{
    StaticString<cFilePathLen> blobPath;
    if (auto err = GetBlobFilePath(mBlobsInstallPath, digest, blobPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    fs::FileInfo fileInfo;
    if (auto err = mFileInfoProvider->GetFileInfo(blobPath, fileInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = VerifyBlobChecksum(digest, fileInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::VerifyBlobChecksum(const String& digest, const fs::FileInfo& fileInfo)
{
    auto [colonPos, findErr] = digest.FindSubstr(0, ":");
    if (!findErr.IsNone()) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    StaticString<oci::cDigestLen> hash;
    if (auto err = hash.Insert(hash.end(), digest.CStr() + colonPos + 1, digest.CStr() + digest.Size());
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto expectedSHA256 = MakeUnique<StaticArray<uint8_t, crypto::cSHA256Size>>(&mAllocator);
    if (!expectedSHA256) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = hash.HexToByteArray(*expectedSHA256); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (fileInfo.mSHA256 != *expectedSHA256) {
        return ErrorEnum::eInvalidChecksum;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::VerifyItemBlobs(const String& indexDigest)
{
    LOG_DBG() << "Verify item blobs" << Log::Field("indexDigest", indexDigest);

    if (auto err = VerifyBlobIntegrity(indexDigest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cFilePathLen> indexPath;
    if (auto err = GetBlobFilePath(mBlobsInstallPath, indexDigest, indexPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);
    if (!imageIndex) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mOCISpec->LoadImageIndex(indexPath, *imageIndex); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& manifestDescriptor : imageIndex->mManifests) {
        if (auto err = VerifyBlobIntegrity(manifestDescriptor.mDigest); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        StaticString<cFilePathLen> manifestPath;
        if (auto err = GetBlobFilePath(mBlobsInstallPath, manifestDescriptor.mDigest, manifestPath); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto manifest = MakeUnique<oci::ImageManifest>(&mAllocator);
        if (!manifest) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        if (auto err = mOCISpec->LoadImageManifest(manifestPath, *manifest); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = VerifyBlobIntegrity(manifest->mConfig.mDigest); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (manifest->mItemConfig.HasValue()) {
            if (auto err = VerifyBlobIntegrity(manifest->mItemConfig->mDigest); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        for (const auto& layer : manifest->mLayers) {
            if (auto err = VerifyBlobIntegrity(layer.mDigest); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    LOG_DBG() << "Item blobs verified successfully";

    return ErrorEnum::eNone;
}

bool ImageManager::IsBlobUsedByItems(const String& blobDigest, const Array<ItemInfo>& items)
{
    for (const auto& item : items) {
        if (item.mIndexDigest == blobDigest) {
            return true;
        }

        StaticString<cFilePathLen> indexPath;
        if (auto err = GetBlobFilePath(mBlobsInstallPath, item.mIndexDigest, indexPath); !err.IsNone()) {
            continue;
        }

        auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);
        if (!imageIndex) {
            continue;
        }

        if (auto err = mOCISpec->LoadImageIndex(indexPath, *imageIndex); !err.IsNone()) {
            continue;
        }

        for (const auto& manifestDescriptor : imageIndex->mManifests) {
            if (manifestDescriptor.mDigest == blobDigest) {
                return true;
            }

            StaticString<cFilePathLen> manifestPath;
            if (auto err = GetBlobFilePath(mBlobsInstallPath, manifestDescriptor.mDigest, manifestPath);
                !err.IsNone()) {
                continue;
            }

            auto manifest = MakeUnique<oci::ImageManifest>(&mAllocator);
            if (!manifest) {
                continue;
            }

            if (auto err = mOCISpec->LoadImageManifest(manifestPath, *manifest); !err.IsNone()) {
                continue;
            }

            if (manifest->mConfig.mDigest == blobDigest) {
                return true;
            }

            if (manifest->mItemConfig.HasValue() && manifest->mItemConfig->mDigest == blobDigest) {
                return true;
            }

            for (const auto& layer : manifest->mLayers) {
                if (layer.mDigest == blobDigest) {
                    return true;
                }
            }
        }
    }

    return false;
}

RetWithError<size_t> ImageManager::CleanupOrphanedBlobs()
{
    LOG_DBG() << "Cleanup orphaned blobs";

    size_t totalSize = 0;

    auto storedItems = MakeUnique<StaticArray<ItemInfo, cMaxNumUpdateItems>>(&mAllocator);
    if (!storedItems) {
        return {0, AOS_ERROR_WRAP(ErrorEnum::eNoMemory)};
    }

    if (auto err = mStorage->GetAllItemsInfos(*storedItems); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    auto algorithmDirIterator = fs::DirIterator(mBlobsInstallPath);

    while (algorithmDirIterator.Next()) {
        auto algorithm    = algorithmDirIterator->mPath;
        auto algorithmDir = fs::JoinPath(mBlobsInstallPath, algorithm);

        auto blobIterator = fs::DirIterator(algorithmDir);

        while (blobIterator.Next()) {
            auto hash = blobIterator->mPath;

            StaticString<oci::cDigestLen> blobDigest;
            blobDigest.Append(algorithm).Append(":").Append(hash);

            if (!IsBlobUsedByItems(blobDigest, *storedItems)) {
                auto filePath = fs::JoinPath(algorithmDir, hash);

                auto [blobSize, sizeErr] = fs::CalculateSize(filePath);
                if (!sizeErr.IsNone()) {
                    LOG_WRN() << "Failed to get blob size" << Log::Field("path", filePath) << Log::Field(sizeErr);
                } else {
                    totalSize += blobSize;
                }

                LOG_DBG() << "Remove orphaned blob" << Log::Field("path", filePath) << Log::Field("size", blobSize);

                if (auto removeErr = fs::RemoveAll(filePath); !removeErr.IsNone()) {
                    LOG_ERR() << "Failed to remove orphaned blob" << Log::Field(removeErr);
                }
            }
        }
    }

    LOG_DBG() << "Cleanup orphaned blobs completed" << Log::Field("totalSize", totalSize);

    return {totalSize, ErrorEnum::eNone};
}

Error ImageManager::RemoveDifferentVersions(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems)
{
    LOG_DBG() << "Remove different versions";

    for (const auto& storedItem : storedItems) {
        if (storedItem.mState != ItemStateEnum::eInstalled) {
            continue;
        }

        auto requestedIt = itemsInfo.FindIf([&storedItem](const auto& item) {
            return item.mItemID == storedItem.mItemID && item.mVersion != storedItem.mVersion;
        });

        if (requestedIt != itemsInfo.end()) {
            LOG_DBG() << "Wiping different version" << Log::Field("itemID", storedItem.mItemID)
                      << Log::Field("storedVersion", storedItem.mVersion)
                      << Log::Field("requestedVersion", requestedIt->mVersion);

            if (auto err = mStorage->RemoveItem(storedItem.mItemID, storedItem.mVersion); !err.IsNone()) {
                LOG_ERR() << "Failed to remove item from storage" << Log::Field("itemID", storedItem.mItemID)
                          << Log::Field("version", storedItem.mVersion) << Log::Field(err);
            }

            NotifyItemStatusChanged(storedItem.mItemID, storedItem.mVersion, ItemStateEnum::eRemoved, ErrorEnum::eNone);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::VerifyBlobsIntegrity(
    const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems, Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Verify blobs integrity";

    for (size_t i = 0; i < itemsInfo.Size(); i++) {
        const auto& itemInfo = itemsInfo[i];

        auto storedIt = storedItems.FindIf([&itemInfo](const auto& stored) {
            return stored.mItemID == itemInfo.mItemID && stored.mVersion == itemInfo.mVersion;
        });

        if (storedIt == storedItems.end()) {
            LOG_WRN() << "Item not found in storage" << Log::Field("itemID", itemInfo.mItemID)
                      << Log::Field("version", itemInfo.mVersion);

            statuses[i].mState = ItemStateEnum::eFailed;
            statuses[i].mError = ErrorEnum::eNotFound;

            continue;
        }

        if (auto err = VerifyItemBlobs(storedIt->mIndexDigest); !err.IsNone()) {
            LOG_ERR() << "Item blobs verification failed" << Log::Field("itemID", itemInfo.mItemID)
                      << Log::Field("version", itemInfo.mVersion) << Log::Field(err);

            statuses[i].mState = ItemStateEnum::eFailed;
            statuses[i].mError = err;

            if (auto removeErr = mStorage->RemoveItem(storedIt->mItemID, storedIt->mVersion); !removeErr.IsNone()) {
                LOG_ERR() << "Failed to remove invalid item" << Log::Field(removeErr);
            }

            NotifyItemStatusChanged(itemInfo.mItemID, itemInfo.mVersion, ItemStateEnum::eFailed, err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SetItemsToInstalled(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems)
{
    LOG_DBG() << "Set items to installed";

    for (const auto& itemInfo : itemsInfo) {
        auto storedIt = storedItems.FindIf([&itemInfo](const auto& stored) {
            return stored.mItemID == itemInfo.mItemID && stored.mVersion == itemInfo.mVersion;
        });

        if (storedIt != storedItems.end()) {
            if (storedIt->mState != ItemStateEnum::eInstalled) {
                if (auto err
                    = mStorage->UpdateItemState(storedIt->mItemID, storedIt->mVersion, ItemStateEnum::eInstalled);
                    !err.IsNone()) {
                    LOG_ERR() << "Failed to update item state to installed" << Log::Field("itemID", storedIt->mItemID)
                              << Log::Field("version", storedIt->mVersion) << Log::Field(err);
                }

                NotifyItemStatusChanged(
                    storedIt->mItemID, storedIt->mVersion, ItemStateEnum::eInstalled, ErrorEnum::eNone);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SetItemsToRemoved(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems)
{
    LOG_DBG() << "Set items to removed";

    for (const auto& storedItem : storedItems) {
        auto requestedIt = itemsInfo.FindIf([&storedItem](const auto& item) {
            return item.mItemID == storedItem.mItemID && item.mVersion == storedItem.mVersion;
        });

        if (requestedIt == itemsInfo.end() && storedItem.mState != ItemStateEnum::eRemoved) {
            LOG_DBG() << "Setting item to removed state" << Log::Field("itemID", storedItem.mItemID)
                      << Log::Field("version", storedItem.mVersion)
                      << Log::Field("currentState", ItemState(storedItem.mState));

            auto now = Time::Now();

            if (auto err
                = mStorage->UpdateItemState(storedItem.mItemID, storedItem.mVersion, ItemStateEnum::eRemoved, now);
                !err.IsNone()) {
                LOG_ERR() << "Failed to update item state to removed" << Log::Field("itemID", storedItem.mItemID)
                          << Log::Field("version", storedItem.mVersion) << Log::Field(err);
            }

            if (auto err = mInstallSpaceAllocator->AddOutdatedItem(storedItem.mItemID, storedItem.mVersion, now);
                !err.IsNone()) {
                LOG_ERR() << "Failed to add outdated item" << Log::Field("itemID", storedItem.mItemID)
                          << Log::Field("version", storedItem.mVersion) << Log::Field(err);
            }

            NotifyItemStatusChanged(storedItem.mItemID, storedItem.mVersion, ItemStateEnum::eRemoved, ErrorEnum::eNone);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetBlobFilePath(
    const String& basePath, const String& digest, StaticString<cFilePathLen>& path) const
{
    auto [colonPos, findErr] = digest.FindSubstr(0, ":");
    if (!findErr.IsNone()) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    StaticString<cDigestAlgorithmLen> algorithm;
    if (auto err = algorithm.Insert(algorithm.end(), digest.CStr(), digest.CStr() + colonPos); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<oci::cDigestLen> hash;
    if (auto err = hash.Insert(hash.end(), digest.CStr() + colonPos + 1, digest.CStr() + digest.Size());
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    path = fs::JoinPath(basePath, algorithm);
    path = fs::JoinPath(path, hash);

    return ErrorEnum::eNone;
}

} // namespace aos::cm::imagemanager
