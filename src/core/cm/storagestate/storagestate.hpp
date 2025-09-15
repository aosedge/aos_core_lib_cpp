/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_HPP_
#define AOS_CORE_CM_STORAGESTATE_HPP_

#include <core/common/cloudprotocol/state.hpp>
#include <core/common/crypto/crypto.hpp>
#include <core/common/tools/noncopyable.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/types.hpp>

namespace aos::cm::storagestate {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Setup storage state instance params.
 */
struct SetupParams {
    InstanceIdent mInstanceIdent;
    size_t        mUID {};
    size_t        mGID {};
    size_t        mStateQuota {};
    size_t        mStorageQuota {};
};

/**
 * Storage state instance info.
 */
struct StorageStateInstanceInfo {
    InstanceIdent                         mInstanceIdent;
    StaticString<cIDLen>                  mInstanceID;
    size_t                                mStorageQuota {};
    size_t                                mStateQuota {};
    StaticString<crypto::cSHA2DigestSize> mStateChecksum;

    /**
     * Compares storage state instance info.
     *
     * @param instance instance to compare with.
     * @return bool.
     */
    bool operator==(const StorageStateInstanceInfo& instance) const
    {
        return mInstanceIdent == instance.mInstanceIdent && mInstanceID == instance.mInstanceID
            && mStorageQuota == instance.mStorageQuota && mStateQuota == instance.mStateQuota
            && mStateChecksum == instance.mStateChecksum;
    }

    /**
     * Compares storage state instance info.
     *
     * @param instance instance to compare with.
     * @return bool.
     */
    bool operator!=(const StorageStateInstanceInfo& instance) const { return !operator==(instance); }
};

/**
 * StorageState storage interface.
 */
class StorageItf {
public:
    /**
     * Adds storage state instance info.
     *
     * @param storageStateInfo storage state instance info to add.
     * @return Error.
     */
    virtual Error AddStorageStateInfo(const StorageStateInstanceInfo& storageStateInfo) = 0;

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
     * @param storageStateInfos[out] array to return storage state instance infos.
     * @return Error.
     */
    virtual Error GetAllStorageStateInfo(Array<StorageStateInstanceInfo>& storageStateInfos) = 0;

    /**
     * Returns storage state instance info by instance ident.
     *
     * @param instanceIdent instance ident to get storage state info for.
     * @param storageStateInfo[out] storage state instance info result.
     * @return Error.
     */
    virtual Error GetStorageStateInfo(const InstanceIdent& instanceIdent, StorageStateInstanceInfo& storageStateInfo)
        = 0;

    /**
     * Updates storage state instance info.
     *
     * @param storageStateInfo storage state instance info to update.
     * @return Error.
     */
    virtual Error UpdateStorageStateInfo(const StorageStateInstanceInfo& storageStateInfo) = 0;

    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;
};

/**
 * Interface to manage storage and state partitions.
 */
class StorageStateItf : public NonCopyable {
public:
    /**
     * Setups storage state instance.
     *
     * @param setupParams setup parameters.
     * @param storagePath[out] storage path.
     * @param statePath[out] state path.
     * @return Error.
     */
    virtual Error Setup(const SetupParams& setupParams, String& storagePath, String& statePath) = 0;

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
     * Updates storage state with new state.
     *
     * @param state new state.
     * @return Error.
     */
    virtual Error UpdateState(const cloudprotocol::UpdateState& state) = 0;

    /**
     * Accepts state from storage.
     *
     * @param state state to accept.
     * @return Error.
     */
    virtual Error AcceptState(const cloudprotocol::StateAcceptance& state) = 0;

    /**
     * Returns instance's checksum.
     *
     * @param instanceIdent instance ident.
     * @param checkSum[out] checksum.
     * @return Error
     */
    virtual Error GetInstanceCheckSum(const InstanceIdent& instanceIdent, String& checkSum) = 0;

    /**
     * Destructor.
     */
    virtual ~StorageStateItf() = default;
};

/** @}*/

} // namespace aos::cm::storagestate

#endif
