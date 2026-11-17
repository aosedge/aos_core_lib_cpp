/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_UPDATEITEMINSTALLER_HPP_
#define AOS_CORE_SM_LAUNCHER_UPDATEITEMINSTALLER_HPP_

#include <core/common/imagemanager/itf/itemstatusprovider.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/instance.hpp>
#include <core/sm/imagemanager/itf/imagemanager.hpp>

#include "instanceregistry.hpp"
#include "launchpool.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Update item version.
 */
struct ItemVersion {
    StaticString<cIDLen>      mItemID;
    StaticString<cVersionLen> mVersion;
};

/**
 * Update item to install with its install result.
 */
struct InstallItem : imagemanager::UpdateItemInfo {
    Error mError;
};

/**
 * Installs, repairs and removes the update items of the node instances.
 *
 * An item version is repaired at most once until the next request for it: an item which failed to install or was
 * already repaired isn't installed again by the repair.
 */
class UpdateItemInstaller {
public:
    /**
     * Initializes update item installer.
     *
     * @param allocator allocator to use for temporary objects.
     * @param imageManager image manager.
     * @param itemStatusProvider item status provider.
     */
    void Init(AllocatorItf& allocator, imagemanager::ImageManagerItf& imageManager,
        common::imagemanager::ItemStatusProviderItf& itemStatusProvider);

    /**
     * Allows installs after they were canceled.
     */
    void Start();

    /**
     * Cancels the installs in progress and the new ones, and waits for the installs in progress to return.
     */
    void Cancel();

    /**
     * Returns the update items used by the stopped instances and not used by the started ones.
     *
     * @param instances node instances.
     * @param stopInstances instances to stop.
     * @param startInstances instances to start.
     * @param[out] removeItems update items to remove.
     */
    void GetUnusedItems(const InstanceRegistry& instances, const Array<InstanceIdent>& stopInstances,
        const Array<InstanceInfo>& startInstances, Array<ItemVersion>& removeItems) const;

    /**
     * Removes update items in parallel. The failures are logged.
     *
     * @param removeItems update items to remove.
     */
    void Remove(const Array<ItemVersion>& removeItems);

    /**
     * Installs in parallel the update items of the instances which aren't installed yet, once per item ID and
     * version. The install result of each item is set to its error. The requested item versions may be repaired
     * again, except the ones which failed to install.
     *
     * @param startInstances instances to start.
     * @param[out] installItems installed update items with their install results.
     * @return Error if the items to install can't be collected or waited for.
     */
    Error Install(const Array<InstanceInfo>& startInstances, Array<InstallItem>& installItems);

    /**
     * Installs again in parallel the broken update items. The items which can't be repaired anymore are removed from
     * the list, the repair result of the others is set to their error.
     *
     * @param[in,out] repairItems update items to repair.
     */
    void Repair(Array<InstallItem>& repairItems);

private:
    static constexpr auto cCancelPeriod = Time::cMilliseconds * 100;

    void  InstallInParallel(Array<InstallItem>& items, const char* message);
    Error InstallOne(const imagemanager::UpdateItemInfo& item);
    bool  IsRepaired(const String& itemID, const String& version) const;
    void  SetRepaired(const String& itemID, const String& version);

    AllocatorItf*                                mAllocator {};
    imagemanager::ImageManagerItf*               mImageManager {};
    common::imagemanager::ItemStatusProviderItf* mItemStatusProvider {};
    LaunchPool                                   mPool;
    StaticArray<ItemVersion, cMaxNumUpdateItems> mRepairedItems;
    Mutex                                        mMutex;
    ConditionalVariable                          mCondVar;
    size_t                                       mNumInstalls {};
    bool                                         mCanceled {};
};

/** @}*/

} // namespace aos::sm::launcher

#endif
