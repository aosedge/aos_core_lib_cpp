/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_ITF_STORAGESTATE_HPP_
#define AOS_CORE_CM_STORAGESTATE_ITF_STORAGESTATE_HPP_

#include <core/common/types/common.hpp>

namespace aos::cm::storagestate {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Setup storage state instance params.
 */
struct SetupParams {
    uid_t  mUID {};
    gid_t  mGID {};
    size_t mStateQuota {};
    size_t mStorageQuota {};

    friend Log& operator<<(Log& log, const SetupParams& params)
    {
        return log << Log::Field("uid", params.mUID) << Log::Field("gid", params.mGID)
                   << Log::Field("stateQuota", params.mStateQuota) << Log::Field("storageQuota", params.mStorageQuota);
    }
};

/**
 * Interface to manage storage partitions and states.
 */
class StorageStateItf {
public:
    /**
     * Setups storage state instance.
     *
     * @param instanceIdent instance ident.
     * @param setupParams setup parameters.
     * @param storagePath[out] storage path.
     * @param statePath[out] state path.
     * @return Error.
     */
    virtual Error Setup(
        const InstanceIdent& instanceIdent, const SetupParams& setupParams, String& storagePath, String& statePath)
        = 0;

    /**
     * Clean-ups storage state instance.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    virtual Error Cleanup(const InstanceIdent& instanceIdent) = 0;

    /**
     * Removes storage state instance.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    virtual Error Remove(const InstanceIdent& instanceIdent) = 0;

    /**
     * Returns instance's checksum.
     *
     * @param instanceIdent instance ident.
     * @param checkSum[out] checksum.
     * @return Error
     */
    virtual Error GetInstanceCheckSum(const InstanceIdent& instanceIdent, Array<uint8_t>& checkSum) = 0;

    /**
     * Returns total state size in bytes.
     *
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetTotalStateSize() const = 0;

    /**
     * Returns total storage size in bytes.
     *
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetTotalStorageSize() const = 0;

    /**
     * Checks if storage and state are on the same partition.
     *
     * @return bool.
     */
    virtual bool IsSamePartition() const = 0;

    /**
     * Destructor.
     */
    virtual ~StorageStateItf() = default;
};

/** @}*/

} // namespace aos::cm::storagestate

#endif
