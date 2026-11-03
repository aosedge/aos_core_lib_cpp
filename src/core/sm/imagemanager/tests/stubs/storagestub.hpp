/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_SM_IMAGE_MANAGER_TESTS_STUBS_STORAGESTUB_HPP_
#define AOS_SM_IMAGE_MANAGER_TESTS_STUBS_STORAGESTUB_HPP_

#include <memory>

#include <core/common/imagemanager/tests/stubs/storagestub.hpp>
#include <core/sm/imagemanager/itf/storage.hpp>

namespace aos::sm::imagemanager {

/**
 * Storage stub.
 */
class StorageStub : public StorageItf {
public:
    Error AddDeployableItem(const common::imagemanager::DeployableItemInfo& item) override
    {
        return mStorage.AddDeployableItem(item);
    }

    Error UpdateDeployableItem(const common::imagemanager::DeployableItemInfo& item) override
    {
        return mStorage.UpdateDeployableItem(item);
    }

    Error RemoveDeployableItem(const String& itemID, const String& version) override
    {
        return mStorage.RemoveDeployableItem(itemID, version);
    }

    Error GetDeployableItem(
        const String& itemID, const String& version, common::imagemanager::DeployableItemInfo& item) override
    {
        return mStorage.GetDeployableItem(itemID, version, item);
    }

    Error GetAllDeployableItems(Array<common::imagemanager::DeployableItemInfo>& items) override
    {
        return mStorage.GetAllDeployableItems(items);
    }

    RetWithError<size_t> GetDeployableItemsCount() override { return mStorage.GetDeployableItemsCount(); }

    Error AddItemBlob(
        const String& itemID, const String& version, const common::imagemanager::ItemBlobInfo& blob) override
    {
        return mStorage.AddItemBlob(itemID, version, blob);
    }

    Error GetItemBlobs(
        const String& itemID, const String& version, Array<common::imagemanager::ItemBlobInfo>& blobs) override
    {
        return mStorage.GetItemBlobs(itemID, version, blobs);
    }

    RetWithError<bool> IsBlobUsed(const String& digest) override { return mStorage.IsBlobUsed(digest); }

    RetWithError<bool> IsLayerUsed(const String& diffID) override
    {
        auto items = std::make_unique<StaticArray<common::imagemanager::DeployableItemInfo, cMaxNumUpdateItems * 2>>();

        if (auto err = mStorage.GetAllDeployableItems(*items); !err.IsNone()) {
            return {false, err};
        }

        for (const auto& item : *items) {
            auto blobs = std::make_unique<StaticArray<common::imagemanager::ItemBlobInfo, 16>>();

            if (auto err = mStorage.GetItemBlobs(item.mItemID, item.mVersion, *blobs); !err.IsNone()) {
                return {false, err};
            }

            for (const auto& blob : *blobs) {
                if (blob.mIsLayer && blob.mDiffID == diffID) {
                    return true;
                }
            }
        }

        return false;
    }

private:
    common::imagemanager::StorageStub mStorage;
};

} // namespace aos::sm::imagemanager

#endif
