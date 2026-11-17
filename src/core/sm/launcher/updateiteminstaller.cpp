/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "updateiteminstaller.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void UpdateItemInstaller::Init(AllocatorItf& allocator, imagemanager::ImageManagerItf& imageManager,
    common::imagemanager::ItemStatusProviderItf& itemStatusProvider)
{
    mAllocator          = &allocator;
    mImageManager       = &imageManager;
    mItemStatusProvider = &itemStatusProvider;
}

void UpdateItemInstaller::Start()
{
    LockGuard lock {mMutex};

    mCanceled = false;
}

void UpdateItemInstaller::Cancel()
{
    {
        LockGuard lock {mMutex};

        mCanceled = true;
    }

    // An install which passed the check in InstallOne may reach the image manager after Cancel resets its context,
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

void UpdateItemInstaller::GetUnusedItems(const InstanceRegistry& instances, const Array<InstanceIdent>& stopInstances,
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

void UpdateItemInstaller::Remove(const Array<ItemVersion>& removeItems)
{
    if (removeItems.IsEmpty()) {
        return;
    }

    if (auto err = mPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    for (const auto& updateItem : removeItems) {
        if (auto err = mPool.AddTask([this, &updateItem](void*) {
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

    if (auto err = mPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Error UpdateItemInstaller::Install(const Array<InstanceInfo>& startInstances, Array<InstallItem>& installItems)
{
    // An item version requested again may be repaired again.
    for (const auto& instance : startInstances) {
        mRepairedItems.RemoveIf([&instance](const ItemVersion& item) {
            return item.mItemID == instance.mItemID && item.mVersion == instance.mVersion;
        });
    }

    // The image manager reports all stored item versions, including the ones kept in the cache.
    auto currentItems = MakeUnique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems * 2>>(mAllocator);
    if (!currentItems) {
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

        if (installItems.ContainsIf([&startInstance](const auto& item) {
                return item.mID == startInstance.mItemID && item.mVersion == startInstance.mVersion;
            })) {
            continue;
        }

        if (auto err = installItems.EmplaceBack(InstallItem {
                {startInstance.mItemID, startInstance.mType, startInstance.mVersion, startInstance.mManifestDigest},
                {}});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    InstallInParallel(installItems, "Install update item failed");

    // The item which failed to install isn't installed again by the repair.
    for (const auto& item : installItems) {
        if (!item.mError.IsNone()) {
            SetRepaired(item.mID, item.mVersion);
        }
    }

    return ErrorEnum::eNone;
}

void UpdateItemInstaller::Repair(Array<InstallItem>& repairItems)
{
    repairItems.RemoveIf([this](const InstallItem& item) { return IsRepaired(item.mID, item.mVersion); });

    for (auto& item : repairItems) {
        LOG_WRN() << "Repair update item" << Log::Field("itemID", item.mID) << Log::Field("version", item.mVersion);

        item.mError = ErrorEnum::eNone;

        SetRepaired(item.mID, item.mVersion);
    }

    InstallInParallel(repairItems, "Repair update item failed");
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void UpdateItemInstaller::InstallInParallel(Array<InstallItem>& items, const char* message)
{
    if (items.IsEmpty()) {
        return;
    }

    if (auto err = mPool.Run(); !err.IsNone()) {
        for (auto& item : items) {
            item.mError = AOS_ERROR_WRAP(err);
        }

        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    for (auto& item : items) {
        if (auto err = mPool.AddTask([this, &item, message](void*) {
                if (auto err = InstallOne(item); !err.IsNone()) {
                    item.mError = AOS_ERROR_WRAP(err);

                    LOG_ERR() << message << Log::Field("itemID", item.mID) << Log::Field("version", item.mVersion)
                              << Log::Field(item.mError);
                }
            });
            !err.IsNone()) {
            item.mError = AOS_ERROR_WRAP(err);

            LOG_ERR() << message << Log::Field("itemID", item.mID) << Log::Field("version", item.mVersion)
                      << Log::Field(item.mError);
        }
    }

    if (auto err = mPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Error UpdateItemInstaller::InstallOne(const imagemanager::UpdateItemInfo& item)
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

bool UpdateItemInstaller::IsRepaired(const String& itemID, const String& version) const
{
    return mRepairedItems.ContainsIf(
        [&](const ItemVersion& item) { return item.mItemID == itemID && item.mVersion == version; });
}

void UpdateItemInstaller::SetRepaired(const String& itemID, const String& version)
{
    if (IsRepaired(itemID, version)) {
        return;
    }

    if (auto err = mRepairedItems.EmplaceBack(ItemVersion {itemID, version}); !err.IsNone()) {
        LOG_ERR() << "Failed to add repaired item" << Log::Field("itemID", itemID) << Log::Field("version", version)
                  << Log::Field(AOS_ERROR_WRAP(err));
    }
}

} // namespace aos::sm::launcher
