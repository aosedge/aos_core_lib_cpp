/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_LAUNCHER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_LAUNCHER_HPP_

#include <core/common/types/types.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/*
 * Instance info.
 */
struct InstanceInfo {
    StaticString<cIDLen> mItemID;
    UpdateItemType       mItemType;
    StaticString<cIDLen> mSubjectID;
    uint64_t             mPriority {0};
    size_t               mNumInstances {0};
    LabelsStaticArray    mLabels;

    /**
     * Compares instance info.
     *
     * @param other instance info to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfo& other) const
    {
        return mItemID == other.mItemID && mItemType == other.mItemType && mSubjectID == other.mSubjectID
            && mPriority == other.mPriority && mNumInstances == other.mNumInstances && mLabels == other.mLabels;
    }

    /**
     * Compares instance info.
     *
     * @param other instance info to compare.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& other) const { return !operator==(other); }
};

/**
 * Instance launcher interface.
 */
class LauncherItf {
public:
    /**
     * Destructor.
     */
    virtual ~LauncherItf() = default;

    /**
     * Returns current statuses of running instances.
     *
     * @param statuses instances statuses.
     * @return Error.
     */
    virtual Error GetInstancesStatuses(Array<InstanceStatus>& statuses) = 0;

    /**
     * Schedules and run instances.
     *
     * @param instances instances info.
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    virtual Error RunInstances(const Array<InstanceInfo>& instances) = 0;

    /**
     * Rebalances instances.
     *
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    virtual Error Rebalance() = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
