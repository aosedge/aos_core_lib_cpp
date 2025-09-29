/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_LAUNCHER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_LAUNCHER_HPP_

#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/*
 * Instance info.
 */
struct RunInstanceRequest {
    StaticString<cIDLen> mItemID;
    StaticString<cIDLen> mProviderID;
    UpdateItemType       mItemType;
    StaticString<cIDLen> mSubjectID;
    size_t               mPriority {};
    size_t               mNumInstances {};
    LabelsArray          mLabels;

    /**
     * Compares instance info.
     *
     * @param other instance info to compare.
     * @return bool.
     */
    bool operator==(const RunInstanceRequest& other) const
    {
        return mItemID == other.mItemID && mProviderID == other.mProviderID && mItemType == other.mItemType
            && mSubjectID == other.mSubjectID && mPriority == other.mPriority && mNumInstances == other.mNumInstances
            && mLabels == other.mLabels;
    }

    /**
     * Compares instance info.
     *
     * @param other instance info to compare.
     * @return bool.
     */
    bool operator!=(const RunInstanceRequest& other) const { return !operator==(other); }
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
     * Schedules and run instances.
     *
     * @param instances instances info.
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    virtual Error RunInstances(const Array<RunInstanceRequest>& instances) = 0;

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
