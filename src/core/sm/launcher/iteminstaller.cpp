/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "iteminstaller.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void ItemInstaller::Init(AllocatorItf& allocator, LaunchPool& launchPool, imagemanager::ImageManagerItf& imageManager,
    common::imagemanager::ItemStatusProviderItf& itemStatusProvider)
{
    mAllocator          = &allocator;
    mLaunchPool         = &launchPool;
    mImageManager       = &imageManager;
    mItemStatusProvider = &itemStatusProvider;
}

void ItemInstaller::Start()
{
    LockGuard lock {mMutex};

    mCanceled = false;
}

void ItemInstaller::Cancel()
{
    {
        LockGuard lock {mMutex};

        mCanceled = true;
    }

    // An install which passed the check in Install may reach the image manager after Cancel resets its context,
    // so cancel again until all of them return.
    while (true) {
        if (auto err = mImageManager->Cancel(); !err.IsNone()) {
            LOG_ERR() << "Can't cancel installs" << Log::Field(AOS_ERROR_WRAP(err));
        }

        UniqueLock lock {mMutex};

        if (auto err = mCondVar.Wait(lock, cCancelPeriod, [this]() { return mNumInstalls == 0; });
            err.IsNone() || !err.Is(ErrorEnum::eTimeout)) {
            return;
        }
    }
}

void ItemInstaller::GetUnusedItems(const InstanceRegistry& instances, const Array<InstanceIdent>& stopInstances,
    const Array<InstanceInfo>& startInstances, Array<ItemVersion>& removeItems) const
{
    for (const auto& instanceIdent : stopInstances) {
        const auto* const instanceData = instances.Find(instanceIdent);
        if (!instanceData) {
            LOG_ERR() << "Instance not found, skip removing update item" << Log::Field("instance", instanceIdent);

            continue;
        }

        if (auto it = startInstances.FindIf([&instanceData](const auto& item) {
                return item.mItemID == instanceData->mInfo.mItemID && item.mVersion == instanceData->mInfo.mVersion;
            });
            it != startInstances.end()) {
            continue;
        }

        if (auto it = removeItems.FindIf([&instanceData](const auto& item) {
                return item.mItemID == instanceData->mInfo.mItemID && item.mVersion == instanceData->mInfo.mVersion;
            });
            it == removeItems.end()) {
            (void)removeItems.EmplaceBack(ItemVersion {instanceData->mInfo.mItemID, instanceData->mInfo.mVersion});
        }
    }
}

void ItemInstaller::RemoveItems(const Array<ItemVersion>& removeItems)
{
    for (const auto& updateItem : removeItems) {
        if (auto err = mLaunchPool->AddTask([this, &updateItem](void*) {
                if (auto err = mImageManager->RemoveItem(updateItem.mItemID, updateItem.mVersion);
                    !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
                    LOG_ERR() << "Remove update item failed" << Log::Field("itemID", updateItem.mItemID)
                              << Log::Field("version", updateItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
            LOG_ERR() << "Remove update item failed" << Log::Field("itemID", updateItem.mItemID)
                      << Log::Field("version", updateItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool->Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Error ItemInstaller::InstallItems(const Array<InstanceInfo>& startInstances, Array<InstallItem>& installItems)
{
    // The image manager reports all stored item versions, including the ones kept in the cache.
    auto currentItems = MakeUnique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems * 2>>(mAllocator);
    if (!currentItems) {
        LOG_ERR() << "Failed to allocate current items" << Log::Field(ErrorEnum::eNoMemory);

        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mItemStatusProvider->GetItemsStatuses(*currentItems); !err.IsNone()) {
        LOG_ERR() << "Get update items statuses failed" << Log::Field(AOS_ERROR_WRAP(err));

        currentItems->Clear();
    }

    for (const auto& startInstance : startInstances) {
        if (auto it = currentItems->FindIf([&startInstance](const auto& item) {
                return item.mItemID == startInstance.mItemID && item.mVersion == startInstance.mVersion;
            });
            it != currentItems->end() && it->mState == ItemStateEnum::eInstalled) {
            continue;
        }

        if (auto it = installItems.FindIf([&startInstance](const auto& item) {
                return item.mID == startInstance.mItemID && item.mVersion == startInstance.mVersion;
            });
            it == installItems.end()) {
            if (auto err = installItems.EmplaceBack(InstallItem {
                    {startInstance.mItemID, startInstance.mType, startInstance.mVersion, startInstance.mManifestDigest},
                    {}});
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    for (auto& installItem : installItems) {
        if (auto err = mLaunchPool->AddTask([this, &installItem](void*) {
                if (auto err = Install(installItem); !err.IsNone()) {
                    installItem.mError = AOS_ERROR_WRAP(err);

                    LOG_ERR() << "Install update item failed" << Log::Field("itemID", installItem.mID)
                              << Log::Field("version", installItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
                }
            });
            !err.IsNone()) {
            installItem.mError = AOS_ERROR_WRAP(err);

            LOG_ERR() << "Install update item failed" << Log::Field("itemID", installItem.mID)
                      << Log::Field("version", installItem.mVersion) << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mLaunchPool->Wait(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ItemInstaller::Install(const imagemanager::UpdateItemInfo& item)
{
    {
        LockGuard lock {mMutex};

        if (mCanceled) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eCanceled, "launcher is stopping"));
        }

        mNumInstalls++;
    }

    auto err = mImageManager->InstallItem(item);

    LockGuard lock {mMutex};

    mNumInstalls--;

    if (auto notifyErr = mCondVar.NotifyAll(); !notifyErr.IsNone()) {
        LOG_ERR() << "Can't notify installs" << Log::Field(AOS_ERROR_WRAP(notifyErr));
    }

    return err;
}

} // namespace aos::sm::launcher
