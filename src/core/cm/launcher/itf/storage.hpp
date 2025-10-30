/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STORAGE_HPP_
#define AOS_CM_LAUNCHER_STORAGE_HPP_

#include <core/common/types/common.hpp>

namespace aos::cm::launcher {

/** @addtogroup CM Storage
 *  @{
 */

/**
 * Persisted instance information.
 */
struct InstanceInfo {
    /**
     * Instance identifier.
     */
    InstanceIdent mInstanceIdent;

    /**
     * Image identifier.
     */
    StaticString<cIDLen> mImageID;

    /**
     * Instance type.
     */
    UpdateItemType mUpdateItemType;

    /**
     * ID of the node hosting the instance.
     */
    StaticString<cIDLen> mNodeID;

    /**
     * ID of the node that hosted the instance earlier.
     */
    StaticString<cIDLen> mPrevNodeID;

    /**
     * Runtime identifier.
     */
    StaticString<cIDLen> mRuntimeID;

    /**
     * User ID.
     */
    uid_t mUID {};

    /**
     * Timestamp.
     */
    Time mTimestamp;

    /**
     * Cached indicator.
     */
    bool mCached {};

    /**
     * Compares instance info.
     *
     * @param other instance info to compare with.
     * @return bool.
     */
    bool operator==(const InstanceInfo& rhs) const
    {
        return mInstanceIdent == rhs.mInstanceIdent && mImageID == rhs.mImageID
            && mUpdateItemType == rhs.mUpdateItemType && mNodeID == rhs.mNodeID && mPrevNodeID == rhs.mPrevNodeID
            && mRuntimeID == rhs.mRuntimeID && mUID == rhs.mUID && mTimestamp == rhs.mTimestamp
            && mCached == rhs.mCached;
    }

    /**
     * Compares instance info.
     *
     * @param rhs instance info to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& rhs) const { return !operator==(rhs); }
};

/**
 * Interface for service instance storage.
 */
class StorageItf {
public:
    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;

    /**
     * Adds a new instance to the storage.
     *
     * @param info instance information.
     * @return Error.
     */
    virtual Error AddInstance(const InstanceInfo& info) = 0;

    /**
     * Updates an existing instance in the storage.
     *
     * @param info updated instance information.
     * @return Error.
     */
    virtual Error UpdateInstance(const InstanceInfo& info) = 0;

    /**
     * Removes an instance from the storage.
     *
     * @param instanceID instance identifier.
     * @return Error.
     */
    virtual Error RemoveInstance(const InstanceIdent& instanceID) = 0;

    /**
     * Get information about a stored instance.
     *
     * @param instanceID instance identifier.
     * @param[out] info instance info.
     * @return Error.
     */
    virtual Error GetInstance(const InstanceIdent& instanceID, InstanceInfo& info) const = 0;

    /**
     * Get all stored instances.
     *
     * @param[out] instances all stored instances.
     * @return Error.
     */
    virtual Error GetActiveInstances(Array<InstanceInfo>& instances) const = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
