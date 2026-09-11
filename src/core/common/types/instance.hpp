/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_INSTANCE_HPP_
#define AOS_CORE_COMMON_TYPES_INSTANCE_HPP_

#include <core/common/crypto/itf/hash.hpp>
#include <core/common/ocispec/itf/imagespec.hpp>

#include "envvars.hpp"
#include "monitoring.hpp"

namespace aos {

/**
 * Instance info data.
 */
struct InstanceInfoData {
    StaticString<cVersionLen>          mVersion;
    StaticString<oci::cDigestLen>      mManifestDigest;
    StaticString<cIDLen>               mRuntimeID;
    StaticString<cIDLen>               mOwnerID;
    SubjectType                        mSubjectType;
    uid_t                              mUID {};
    gid_t                              mGID {};
    uint64_t                           mPriority {};
    StaticString<cFilePathLen>         mStoragePath;
    StaticString<cFilePathLen>         mStatePath;
    EnvVarArray                        mEnvVars;
    Optional<InstanceMonitoringParams> mMonitoringParams;

    /**
     * Compares instance info data.
     *
     * @param rhs data to compare.
     * @return bool.
     */
    friend bool operator==(const InstanceInfoData& lhs, const InstanceInfoData& rhs)
    {
        return lhs.mVersion == rhs.mVersion && lhs.mManifestDigest == rhs.mManifestDigest
            && lhs.mRuntimeID == rhs.mRuntimeID && lhs.mOwnerID == rhs.mOwnerID && lhs.mSubjectType == rhs.mSubjectType
            && lhs.mUID == rhs.mUID && lhs.mGID == rhs.mGID && lhs.mPriority == rhs.mPriority
            && lhs.mStoragePath == rhs.mStoragePath && lhs.mStatePath == rhs.mStatePath && lhs.mEnvVars == rhs.mEnvVars
            && lhs.mMonitoringParams == rhs.mMonitoringParams;
    };

    /**
     * Compares instance info data.
     *
     * @param rhs data to compare.
     * @return bool.
     */
    friend bool operator!=(const InstanceInfoData& lhs, const InstanceInfoData& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const InstanceInfo& lhs, const InstanceInfo& rhs)
    {
        return (static_cast<const InstanceIdent&>(lhs) == rhs) && (static_cast<const InstanceInfoData&>(lhs) == rhs);
    };

    /**
     * Compares instance info.
     *
     * @param rhs info to compare.
     * @return bool.
     */
    friend bool operator!=(const InstanceInfo& lhs, const InstanceInfo& rhs) { return !(lhs == rhs); };
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
    EnvVarStatusArray                         mEnvVarsStatuses;
    InstanceState                             mState;
    Error                                     mError;

    /**
     * Compares instance status data.
     *
     * @param rhs instance status data to compare.
     * @return bool.
     */
    friend bool operator==(const InstanceStatusData& lhs, const InstanceStatusData& rhs)
    {
        return lhs.mNodeID == rhs.mNodeID && lhs.mRuntimeID == rhs.mRuntimeID
            && lhs.mManifestDigest == rhs.mManifestDigest && lhs.mStateChecksum == rhs.mStateChecksum
            && lhs.mEnvVarsStatuses == rhs.mEnvVarsStatuses && lhs.mState == rhs.mState && lhs.mError == rhs.mError;
    };

    /**
     * Compares instance status data.
     *
     * @param rhs instance status data to compare.
     * @return bool.
     */
    friend bool operator!=(const InstanceStatusData& lhs, const InstanceStatusData& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const InstanceStatus& lhs, const InstanceStatus& rhs)
    {
        return (static_cast<const InstanceIdent&>(lhs) == rhs) && (static_cast<const InstanceStatusData&>(lhs) == rhs)
            && lhs.mVersion == rhs.mVersion;
    };

    /**
     * Compares instance status.
     *
     * @param rhs status to compare.
     * @return bool.
     */
    friend bool operator!=(const InstanceStatus& lhs, const InstanceStatus& rhs) { return !(lhs == rhs); };
};

/**
 * Instance status array.
 */
using InstanceStatusArray = StaticArray<InstanceStatus, cMaxNumInstances>;

} // namespace aos

#endif
