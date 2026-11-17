/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITEMINSTALLER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITEMINSTALLER_HPP_

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
 * Installs and removes the update items used by the node instances.
 */
class ItemInstaller {
public:
    /**
     * Initializes item installer.
     *
     * @param allocator allocator to use for temporary objects.
     * @param launchPool thread pool which runs the install and remove tasks.
     * @param imageManager image manager.
     * @param itemStatusProvider item status provider.
     */
    void Init(AllocatorItf& allocator, LaunchPool& launchPool, imagemanager::ImageManagerItf& imageManager,
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
    void RemoveItems(const Array<ItemVersion>& removeItems);

    /**
     * Installs in parallel the update items of the instances which aren't installed yet, once per item ID and
     * version. The install result of each item is set to its error.
     *
     * @param startInstances instances to start.
     * @param[out] installItems installed update items with their install results.
     * @return Error if the items to install can't be collected or waited for.
     */
    Error InstallItems(const Array<InstanceInfo>& startInstances, Array<InstallItem>& installItems);

    /**
     * Installs update item.
     *
     * @param item update item.
     * @return Error eCanceled if the installs are canceled.
     */
    Error Install(const imagemanager::UpdateItemInfo& item);

private:
    static constexpr auto cCancelPeriod = Time::cMilliseconds * 100;

    AllocatorItf*                                mAllocator {};
    LaunchPool*                                  mLaunchPool {};
    imagemanager::ImageManagerItf*               mImageManager {};
    common::imagemanager::ItemStatusProviderItf* mItemStatusProvider {};
    Mutex                                        mMutex;
    ConditionalVariable                          mCondVar;
    size_t                                       mNumInstalls {};
    bool                                         mCanceled {};
};

/** @}*/

} // namespace aos::sm::launcher

#endif
