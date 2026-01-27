/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_SM_IMAGE_MANAGER_TESTS_STUBS_STORAGESTUB_HPP_
#define AOS_SM_IMAGE_MANAGER_TESTS_STUBS_STORAGESTUB_HPP_

#include <list>

#include <core/sm/imagemanager/itf/storage.hpp>

namespace aos::sm::imagemanager {

class StorageStub : public StorageItf {
public:
    /**
     * Adds update item to storage.
     *
     * @param updateItem update item to add.
     * @return Error.
     */
    Error AddUpdateItem(const UpdateItemData& updateItem) override
    {
        auto it = std::find_if(mItemsList.begin(), mItemsList.end(), [&](const UpdateItemData& item) {
            return item.mID == updateItem.mID && item.mVersion == updateItem.mVersion;
        });
        if (it != mItemsList.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mItemsList.push_back(updateItem);

        return ErrorEnum::eNone;
    }

    /**
     * Updates update item in storage.
     *
     * @param updateItem update item to update.
     * @return Error.
     */
    Error UpdateUpdateItem(const UpdateItemData& updateItem) override
    {
        auto it = std::find_if(mItemsList.begin(), mItemsList.end(), [&](const UpdateItemData& item) {
            return item.mID == updateItem.mID && item.mVersion == updateItem.mVersion;
        });
        if (it != mItemsList.end()) {
            *it = updateItem;

            return ErrorEnum::eNone;
        }

        mItemsList.push_back(updateItem);

        return ErrorEnum::eNone;
    }

    /**
     * Removes previously stored update item.
     *
     * @param itemID update item ID to remove.
     * @param version update item version.
     * @return Error.
     */
    Error RemoveUpdateItem(const String& itemID, const String& version) override
    {
        auto it = std::find_if(mItemsList.begin(), mItemsList.end(),
            [&](const UpdateItemData& item) { return item.mID == itemID && item.mVersion == version; });
        if (it == mItemsList.end()) {
            return ErrorEnum::eNotFound;
        }

        mItemsList.erase(it);

        return ErrorEnum::eNone;
    }

    /**
     * Returns update item versions by item ID.
     *
     * @param itemID update item ID.
     * @param[out] itemData array to store update item data.
     * @return Error.
     */
    Error GetUpdateItem(const String& itemID, Array<UpdateItemData>& itemData) override
    {
        for (const auto& item : mItemsList) {
            if (item.mID == itemID) {
                if (auto err = itemData.PushBack(item); !err.IsNone()) {
                    return err;
                }
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns all update items.
     *
     * @param itemsData[out] array to store update items data.
     * @return Error.
     */
    Error GetAllUpdateItems(Array<UpdateItemData>& itemsData) override
    {
        for (const auto& item : mItemsList) {
            if (auto err = itemsData.PushBack(item); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns count of stored update items.
     *
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> GetUpdateItemsCount() override { return RetWithError<size_t>(mItemsList.size()); }

private:
    std::list<UpdateItemData>                mItemsList;
    std::queue<std::promise<UpdateItemData>> mRemovePromises;
};

} // namespace aos::sm::imagemanager

#endif
