/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_STUBS_IMAGEMANAGERSTORAGESTUB_HPP_
#define AOS_CORE_COMMON_TESTS_STUBS_IMAGEMANAGERSTORAGESTUB_HPP_

#include <algorithm>
#include <mutex>
#include <vector>

#include <core/common/imagemanager/itf/storage.hpp>

namespace aos::common::imagemanager {

/**
 * Storage stub.
 */
class StorageStub : public StorageItf {
public:
    Error AddDeployableItem(const DeployableItemInfo& item) override
    {
        std::lock_guard lock {mMutex};

        if (Find(item.mItemID, item.mVersion) != mItems.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mItems.push_back({item, {}});

        return ErrorEnum::eNone;
    }

    Error UpdateDeployableItem(const DeployableItemInfo& item) override
    {
        std::lock_guard lock {mMutex};

        auto it = Find(item.mItemID, item.mVersion);
        if (it == mItems.end()) {
            return ErrorEnum::eNotFound;
        }

        it->mInfo = item;

        return ErrorEnum::eNone;
    }

    Error RemoveDeployableItem(const String& itemID, const String& version) override
    {
        std::lock_guard lock {mMutex};

        auto it = Find(itemID, version);
        if (it == mItems.end()) {
            return ErrorEnum::eNotFound;
        }

        mItems.erase(it);

        return ErrorEnum::eNone;
    }

    Error GetDeployableItem(const String& itemID, const String& version, DeployableItemInfo& item) override
    {
        std::lock_guard lock {mMutex};

        auto it = Find(itemID, version);
        if (it == mItems.end()) {
            return ErrorEnum::eNotFound;
        }

        item = it->mInfo;

        return ErrorEnum::eNone;
    }

    Error GetAllDeployableItems(Array<DeployableItemInfo>& items) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& item : mItems) {
            if (auto err = items.PushBack(item.mInfo); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    RetWithError<size_t> GetDeployableItemsCount() override
    {
        std::lock_guard lock {mMutex};

        return mItems.size();
    }

    Error AddItemBlob(const String& itemID, const String& version, const ItemBlobInfo& blob) override
    {
        std::lock_guard lock {mMutex};

        auto it = Find(itemID, version);
        if (it == mItems.end()) {
            return ErrorEnum::eNotFound;
        }

        if (std::none_of(it->mBlobs.begin(), it->mBlobs.end(),
                [&](const ItemBlobInfo& itemBlob) { return itemBlob.mDigest == blob.mDigest; })) {
            it->mBlobs.push_back(blob);
        }

        return ErrorEnum::eNone;
    }

    Error GetItemBlobs(const String& itemID, const String& version, Array<ItemBlobInfo>& blobs) override
    {
        std::lock_guard lock {mMutex};

        auto it = Find(itemID, version);
        if (it == mItems.end()) {
            return ErrorEnum::eNotFound;
        }

        for (const auto& blob : it->mBlobs) {
            if (auto err = blobs.PushBack(blob); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    RetWithError<bool> IsBlobUsed(const String& digest) override
    {
        std::lock_guard lock {mMutex};

        return std::any_of(mItems.begin(), mItems.end(), [&](const Item& item) {
            return std::any_of(item.mBlobs.begin(), item.mBlobs.end(),
                [&](const ItemBlobInfo& blob) { return blob.mDigest == digest; });
        });
    }

    std::vector<DeployableItemInfo> GetItems()
    {
        std::lock_guard lock {mMutex};

        std::vector<DeployableItemInfo> items;

        for (const auto& item : mItems) {
            items.push_back(item.mInfo);
        }

        return items;
    }

private:
    struct Item {
        DeployableItemInfo        mInfo;
        std::vector<ItemBlobInfo> mBlobs;
    };

    std::vector<Item>::iterator Find(const String& itemID, const String& version)
    {
        return std::find_if(mItems.begin(), mItems.end(),
            [&](const Item& item) { return item.mInfo.mItemID == itemID && item.mInfo.mVersion == version; });
    }

    std::mutex        mMutex;
    std::vector<Item> mItems;
};

} // namespace aos::common::imagemanager

#endif
