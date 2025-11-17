/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_INSTANCE_HPP_
#define AOS_CORE_COMMON_TYPES_INSTANCE_HPP_

#include <core/common/crypto/itf/hash.hpp>
#include <core/common/ocispec/imagespec.hpp>

#include "common.hpp"
#include "network.hpp"

namespace aos {

/**
 * Instance info data.
 */
struct InstanceInfoData {
    StaticString<cIDLen>       mImageID;
    StaticString<cIDLen>       mRuntimeID;
    uid_t                      mUID {};
    gid_t                      mGID {};
    uint64_t                   mPriority {};
    StaticString<cFilePathLen> mStoragePath;
    StaticString<cFilePathLen> mStatePath;
    InstanceNetworkParameters  mNetworkParameters;

    /**
     * Compares instance info data.
     *
     * @param rhs data to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfoData& rhs) const
    {
        return mRuntimeID == rhs.mRuntimeID && mUID == rhs.mUID && mPriority == rhs.mPriority
            && mStoragePath == rhs.mStoragePath && mStatePath == rhs.mStatePath
            && mNetworkParameters == rhs.mNetworkParameters;
    }

    /**
     * Compares instance info data.
     *
     * @param rhs data to compare.
     * @return bool.
     */
    bool operator!=(const InstanceInfoData& rhs) const { return !operator==(rhs); }
};

/**
 * Instance info.
 */
struct InstanceInfo : public InstanceIdent, public InstanceInfoData {
    /**
     * Compares instance info.
     *
     * @param rhs info to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfo& rhs) const
    {
        return InstanceIdent::operator==(rhs) && InstanceInfoData::operator==(rhs);
    }

    /**
     * Compares instance info.
     *
     * @param rhs info to compare.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& rhs) const { return !operator==(rhs); }
};

/**
 * Instance info array.
 */
using InstanceInfoArray = StaticArray<InstanceInfo, cMaxNumInstances>;

/**
 * Instance status data.
 */
struct InstanceStatusData {
    StaticString<cIDLen>                      mNodeID;
    StaticString<cIDLen>                      mRuntimeID;
    StaticString<oci::cDigestLen>             mManifestDigest;
    StaticArray<uint8_t, crypto::cSHA256Size> mStateChecksum;
    InstanceState                             mState;
    Error                                     mError;

    /**
     * Compares instance status data.
     *
     * @param rhs instance status data to compare.
     * @return bool.
     */
    bool operator==(const InstanceStatusData& rhs) const
    {
        return mNodeID == rhs.mNodeID && mRuntimeID == rhs.mRuntimeID && mManifestDigest == rhs.mManifestDigest
            && mStateChecksum == rhs.mStateChecksum && mState == rhs.mState && mError == rhs.mError;
    }

    /**
     * Compares instance status data.
     *
     * @param rhs instance status data to compare.
     * @return bool.
     */
    bool operator!=(const InstanceStatusData& rhs) const { return !operator==(rhs); }
};

/**
 * Instance status.
 */
struct InstanceStatus : public InstanceIdent, public InstanceStatusData {
    StaticString<cVersionLen> mVersion;

    /**
     * Compares instance status.
     *
     * @param rhs status to compare.
     * @return bool.
     */
    bool operator==(const InstanceStatus& rhs) const
    {
        return InstanceIdent::operator==(rhs) && InstanceStatusData::operator==(rhs) && mVersion == rhs.mVersion;
    }

    /**
     * Compares instance status.
     *
     * @param rhs status to compare.
     * @return bool.
     */
    bool operator!=(const InstanceStatus& rhs) const { return !operator==(rhs); }
};

/**
 * Instance status array.
 */
using InstanceStatusArray = StaticArray<InstanceStatus, cMaxNumInstances>;

} // namespace aos

#endif
