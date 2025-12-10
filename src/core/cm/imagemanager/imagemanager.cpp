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

    if (auto err = mStorage->GetItemsInfo(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mBlobsInstallPath = fs::JoinPath(mConfig.mInstallPath, "/blobs/sha256/");

    if (auto err = fs::MakeDirAll(mBlobsInstallPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mBlobsDownloadPath = fs::JoinPath(mConfig.mDownloadPath, "/blobs/sha256/");

    if (auto err = fs::MakeDirAll(mBlobsDownloadPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = AllocateSpaceForPartialDownloads(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
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
            cRemovePeriod,
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
    LOG_DBG() << "Download update items" << Log::Field("count", itemsInfo.Size());

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

    if (auto err = mStorage->GetItemsInfo(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = CleanupDownloadingItems(itemsInfo, *storedItems); !err.IsNone()) {
        LOG_ERR() << "Failed to cleanup downloading items" << Log::Field(err);
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

    NotifyItemsStatusesChanged(statuses);

    return ErrorEnum::eNone;
}

Error ImageManager::InstallUpdateItems(const Array<UpdateItemInfo>& itemsInfo, Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Install update items" << Log::Field("count", itemsInfo.Size());

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

    if (auto err = mStorage->GetItemsInfo(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = RemoveDifferentVersions(itemsInfo, *storedItems); !err.IsNone()) {
        LOG_ERR() << "Failed to remove different versions" << Log::Field(err);
    }

    storedItems->Clear();

    if (auto err = mStorage->GetItemsInfo(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = VerifyBlobsIntegrity(itemsInfo, *storedItems, statuses); !err.IsNone()) {
        LOG_ERR() << "Failed to verify blobs integrity" << Log::Field(err);
    }

    storedItems->Clear();

    if (auto err = mStorage->GetItemsInfo(*storedItems); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetItemsToInstalled(itemsInfo, *storedItems); !err.IsNone()) {
        LOG_ERR() << "Failed to set items to installed" << Log::Field(err);
    }

    storedItems->Clear();

    if (auto err = mStorage->GetItemsInfo(*storedItems); !err.IsNone()) {
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

    NotifyItemsStatusesChanged(statuses);

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
    (void)statuses;

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

    path = fs::JoinPath(mBlobsInstallPath, digest);

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return exists ? ErrorEnum::eNone : ErrorEnum::eNotFound;
}

Error ImageManager::GetBlobURL(const String& digest, String& url) const
{
    (void)digest;
    (void)url;

    return ErrorEnum::eNone;
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

RetWithError<size_t> ImageManager::RemoveItem(const String& id)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove item" << Log::Field("id", id);

    auto storedItems = MakeUnique<StaticArray<ItemInfo, cMaxNumUpdateItems>>(&mAllocator);
    if (!storedItems) {
        return {0, AOS_ERROR_WRAP(ErrorEnum::eNoMemory)};
    }

    if (auto err = mStorage->GetItemsInfo(*storedItems); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    auto itemIt = storedItems->FindIf([&id](const auto& item) { return item.mItemID == id; });

    if (itemIt == storedItems->end()) {
        return {0, ErrorEnum::eNotFound};
    }

    const auto& itemToRemove = *itemIt;

    if (auto err = mStorage->RemoveItem(itemToRemove.mItemID, itemToRemove.mVersion); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
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

    if (auto err = mStorage->GetItemsInfo(*items); !err.IsNone()) {
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

    auto dirIterator = fs::DirIterator(mBlobsDownloadPath);

    while (dirIterator.Next()) {
        auto fileName = dirIterator->mPath;
        auto filePath = fs::JoinPath(mBlobsDownloadPath, fileName);

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
                auto filePath = fs::JoinPath(mBlobsInstallPath, storedItem.mIndexDigest);

                LOG_DBG() << "Remove blob" << Log::Field("path", filePath);

                if (auto err = fs::RemoveAll(filePath); !err.IsNone()) {
                    LOG_ERR() << "Failed to remove blob" << Log::Field("path", filePath) << Log::Field(err);
                }
            }
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::VerifyStoredItems(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains,
    Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Verify stored items";

    for (const auto& storedItem : storedItems) {
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

                if (err == ErrorEnum::eCanceled) {
                    return err;
                }

                if (auto updateErr
                    = mStorage->UpdateItemState(storedItem.mItemID, storedItem.mVersion, ItemStateEnum::eFailed);
                    !updateErr.IsNone()) {
                    LOG_ERR() << "Failed to update item state" << Log::Field(updateErr);
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

Error ImageManager::ProcessDownloadRequest(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems,
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

        auto existingItemIt = storedItems.FindIf([&itemInfo](const auto& stored) {
            return stored.mItemID == itemInfo.mItemID
                && (stored.mState == ItemStateEnum::eDownloading || stored.mState == ItemStateEnum::eFailed);
        });

        if (existingItemIt != storedItems.end()) {
            if (existingItemIt->mVersion != itemInfo.mVersion) {
                if (auto removeErr = mStorage->RemoveItem(existingItemIt->mItemID, existingItemIt->mVersion);
                    !removeErr.IsNone()) {
                    LOG_ERR() << "Failed to remove old item" << Log::Field(removeErr);
                }
            }
        } else {
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
        }

        auto downloadErr = DownloadItem(itemInfo, certificates, certificateChains);

        if (!downloadErr.IsNone()) {
            LOG_ERR() << "Failed to download item" << Log::Field("id", itemInfo.mItemID)
                      << Log::Field("version", itemInfo.mVersion) << Log::Field(downloadErr);

            if (downloadErr == ErrorEnum::eCanceled) {
                return downloadErr;
            }
        }

        auto finalState = downloadErr.IsNone() ? ItemStateEnum::ePending : ItemStateEnum::eFailed;
        if (auto updateErr = mStorage->UpdateItemState(itemInfo.mItemID, itemInfo.mVersion, finalState);
            !updateErr.IsNone()) {
            LOG_ERR() << "Failed to update item state" << Log::Field(updateErr);
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

    auto downloadPath = fs::JoinPath(mBlobsDownloadPath, itemInfo.mIndexDigest);
    auto installPath  = fs::JoinPath(mBlobsInstallPath, itemInfo.mIndexDigest);

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

    auto downloadPath = fs::JoinPath(mBlobsDownloadPath, digest);
    auto installPath  = fs::JoinPath(mBlobsInstallPath, digest);

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

Error ImageManager::LoadLayers(const Array<oci::ContentDescriptor>& layers,
    const Array<crypto::CertificateInfo>& certificates, const Array<crypto::CertificateChainInfo>& certificateChains)
{
    LOG_DBG() << "Load layers" << Log::Field("count", layers.Size());

    Error err;

    for (const auto& layer : layers) {
        LOG_DBG() << "Load layer" << Log::Field("digest", layer.mDigest);

        UniquePtr<spaceallocator::SpaceItf> space;
        auto                                downloadPath = fs::JoinPath(mBlobsDownloadPath, layer.mDigest);
        auto                                installPath  = fs::JoinPath(mBlobsInstallPath, layer.mDigest);

        auto releaseSpace = DeferRelease(&err, [&](const Error* err) {
            if (space && !err->IsNone()) {
                LOG_ERR() << "Failed to load layer" << Log::Field("digest", layer.mDigest) << Log::Field(*err);

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

        if (err = EnsureBlob(layer.mDigest, downloadPath, installPath, certificates, certificateChains, space);
            !err.IsNone()) {
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

            LOG_INF() << "Retrying get blobs info" << Log::Field("digest", digest);

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

Error ImageManager::CheckExistingBlob(const String& installPath, const BlobInfo& blobInfo)
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

    if (fileInfo.mSHA256 == blobInfo.mSHA256) {
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

    while (true) {
        Error err = mDownloader->Download(blobInfo.mDigest, blobInfo.mURLs[0], downloadPath);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to download" << Log::Field("url", blobInfo.mURLs[0])
                      << Log::Field("path", downloadPath) << Log::Field(err);

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

            LOG_INF() << "Retrying download" << Log::Field("url", blobInfo.mURLs[0])
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

    if (auto err = GetBlobInfo(digest, blobInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = CheckExistingBlob(installPath, blobInfo); !err.IsNone()) {
        if (err == ErrorEnum::eAlreadyExist) {
            return err;
        }

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

    if (err = mCryptoHelper->Decrypt(downloadPath, installPath, blobInfo.mDecryptInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Decrypted successfully" << Log::Field("path", installPath);

    if (err = mCryptoHelper->ValidateSigns(installPath, blobInfo.mSignInfo, certificateChains, certificates);
        !err.IsNone()) {
        if (auto removeErr = fs::RemoveAll(installPath); !removeErr.IsNone()) {
            LOG_ERR() << "Failed to remove install file" << Log::Field("path", installPath) << Log::Field(removeErr);
        }

        return AOS_ERROR_WRAP(err);
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

    for (auto* listener : mListeners) {
        listener->OnItemsStatusesChanged(statuses);
    }
}

Error ImageManager::VerifyBlobChecksum(const String& digest, const fs::FileInfo& fileInfo)
{
    auto expectedSHA256 = MakeUnique<StaticArray<uint8_t, crypto::cSHA256Size>>(&mAllocator);
    if (!expectedSHA256) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = digest.HexToByteArray(*expectedSHA256); !err.IsNone()) {
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

    auto indexPath = fs::JoinPath(mBlobsInstallPath, indexDigest);

    fs::FileInfo indexFileInfo;
    if (auto err = mFileInfoProvider->GetFileInfo(indexPath, indexFileInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = VerifyBlobChecksum(indexDigest, indexFileInfo); !err.IsNone()) {
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
        auto manifestPath = fs::JoinPath(mBlobsInstallPath, manifestDescriptor.mDigest);

        fs::FileInfo manifestFileInfo;
        if (auto err = mFileInfoProvider->GetFileInfo(manifestPath, manifestFileInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = VerifyBlobChecksum(manifestDescriptor.mDigest, manifestFileInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto manifest = MakeUnique<oci::ImageManifest>(&mAllocator);
        if (!manifest) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        if (auto err = mOCISpec->LoadImageManifest(manifestPath, *manifest); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        for (const auto& layer : manifest->mLayers) {
            auto layerPath = fs::JoinPath(mBlobsInstallPath, layer.mDigest);

            fs::FileInfo layerFileInfo;

            if (auto err = mFileInfoProvider->GetFileInfo(layerPath, layerFileInfo); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (auto err = VerifyBlobChecksum(layer.mDigest, layerFileInfo); !err.IsNone()) {
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

        auto indexPath  = fs::JoinPath(mBlobsInstallPath, item.mIndexDigest);
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

            auto manifestPath = fs::JoinPath(mBlobsInstallPath, manifestDescriptor.mDigest);
            auto manifest     = MakeUnique<oci::ImageManifest>(&mAllocator);
            if (!manifest) {
                continue;
            }

            if (auto err = mOCISpec->LoadImageManifest(manifestPath, *manifest); !err.IsNone()) {
                continue;
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

    if (auto err = mStorage->GetItemsInfo(*storedItems); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    auto dirIterator = fs::DirIterator(mBlobsInstallPath);

    while (dirIterator.Next()) {
        auto blobDigest = dirIterator->mPath;

        if (!IsBlobUsedByItems(blobDigest, *storedItems)) {
            auto filePath = fs::JoinPath(mBlobsInstallPath, blobDigest);

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

    LOG_DBG() << "Cleanup orphaned blobs completed" << Log::Field("totalSize", totalSize);

    return {totalSize, ErrorEnum::eNone};
}

Error ImageManager::RemoveDifferentVersions(const Array<UpdateItemInfo>& itemsInfo, const Array<ItemInfo>& storedItems)
{
    LOG_DBG() << "Remove different versions";

    for (const auto& storedItem : storedItems) {
        auto requestedIt
            = itemsInfo.FindIf([&storedItem](const auto& item) { return item.mItemID == storedItem.mItemID; });

        if (requestedIt != itemsInfo.end()) {
            if (requestedIt->mVersion != storedItem.mVersion) {
                LOG_DBG() << "Wiping different version" << Log::Field("itemID", storedItem.mItemID)
                          << Log::Field("storedVersion", storedItem.mVersion)
                          << Log::Field("requestedVersion", requestedIt->mVersion);

                if (auto err = mStorage->RemoveItem(storedItem.mItemID, storedItem.mVersion); !err.IsNone()) {
                    LOG_ERR() << "Failed to remove item from storage" << Log::Field("itemID", storedItem.mItemID)
                              << Log::Field("version", storedItem.mVersion) << Log::Field(err);
                }
            }
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

            if (auto err = mInstallSpaceAllocator->AddOutdatedItem(storedItem.mItemID, now); !err.IsNone()) {
                LOG_ERR() << "Failed to add outdated item" << Log::Field("itemID", storedItem.mItemID)
                          << Log::Field(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::imagemanager
