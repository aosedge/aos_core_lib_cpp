/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_ITF_STORAGE_HPP_
#define AOS_CORE_CM_STORAGESTATE_ITF_STORAGE_HPP_

#include <core/common/crypto/itf/hash.hpp>

namespace aos::cm::storagestate {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Instance info.
 */
struct InstanceInfo {
    InstanceIdent                             mInstanceIdent;
    size_t                                    mStorageQuota {};
    size_t                                    mStateQuota {};
    StaticArray<uint8_t, crypto::cSHA256Size> mStateChecksum;

    /**
     * Compares instance info.
     *
     * @param instance instance to compare with.
     * @return bool.
     */
    bool operator==(const InstanceInfo& instance) const
    {
        return mInstanceIdent == instance.mInstanceIdent && mStorageQuota == instance.mStorageQuota
            && mStateQuota == instance.mStateQuota && mStateChecksum == instance.mStateChecksum;
    }

    /**
     * Compares instance info.
     *
     * @param instance instance to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& instance) const { return !operator==(instance); }
};

using InstanceInfoArray = StaticArray<InstanceInfo, cMaxNumInstances>;

/**
 * StorageState storage interface.
 */
class StorageItf {
public:
    /**
     * Adds storage state instance info.
     *
     * @param info storage state instance info to add.
     * @return Error.
     */
    virtual Error AddStorageStateInfo(const InstanceInfo& info) = 0;

    /**
     * Removes storage state instance info.
     *
     * @param instanceIdent instance ident to remove.
     * @return Error.
     */
    virtual Error RemoveStorageStateInfo(const InstanceIdent& instanceIdent) = 0;

    /**
     * Returns all storage state instance infos.
     *
     * @param infos[out] array to return storage state instance infos.
     * @return Error.
     */
    virtual Error GetAllStorageStateInfo(Array<InstanceInfo>& infos) = 0;

    /**
     * Returns storage state instance info by instance ident.
     *
     * @param instanceIdent instance ident to get storage state info for.
     * @param info[out] storage state instance info result.
     * @return Error.
     */
    virtual Error GetStorageStateInfo(const InstanceIdent& instanceIdent, InstanceInfo& info) = 0;

    /**
     * Updates storage state instance info.
     *
     * @param info storage state instance info to update.
     * @return Error.
     */
    virtual Error UpdateStorageStateInfo(const InstanceInfo& info) = 0;

    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;
};

/** @}*/

} // namespace aos::cm::storagestate

#endif
