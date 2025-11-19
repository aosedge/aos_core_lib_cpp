/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_OCISPEC_SERVICECONFIG_HPP_
#define AOS_CORE_COMMON_OCISPEC_SERVICECONFIG_HPP_

#include <core/common/tools/map.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/types/permissions.hpp>

#include "common.hpp"

namespace aos::oci {

/**
 * Max num runtimes.
 */
constexpr auto cMaxNumRunners = AOS_CONFIG_OCISPEC_MAX_NUM_RUNTIMES;

/**
 * Service quotas.
 */
struct ServiceQuotas {
    Optional<uint64_t> mCPUDMIPSLimit;
    Optional<uint64_t> mRAMLimit;
    Optional<uint64_t> mPIDsLimit;
    Optional<uint64_t> mNoFileLimit;
    Optional<uint64_t> mTmpLimit;
    Optional<uint64_t> mStateLimit;
    Optional<uint64_t> mStorageLimit;
    Optional<uint64_t> mUploadSpeed;
    Optional<uint64_t> mDownloadSpeed;
    Optional<uint64_t> mUploadLimit;
    Optional<uint64_t> mDownloadLimit;

    /**
     * Compares service quotas.
     *
     * @param rhs service quotas to compare.
     * @return bool.
     */
    bool operator==(const ServiceQuotas& rhs) const
    {
        return mCPUDMIPSLimit == rhs.mCPUDMIPSLimit && mRAMLimit == rhs.mRAMLimit && mPIDsLimit == rhs.mPIDsLimit
            && mNoFileLimit == rhs.mNoFileLimit && mTmpLimit == rhs.mTmpLimit && mStateLimit == rhs.mStateLimit
            && mStorageLimit == rhs.mStorageLimit && mUploadSpeed == rhs.mUploadSpeed
            && mDownloadSpeed == rhs.mDownloadSpeed && mUploadLimit == rhs.mUploadLimit
            && mDownloadLimit == rhs.mDownloadLimit;
    }

    /**
     * Compares service quotas.
     *
     * @param rhs service quotas to compare.
     * @return bool.
     */
    bool operator!=(const ServiceQuotas& rhs) const { return !operator==(rhs); }
};

/**
 * Requested resources.
 */
struct RequestedResources {
    Optional<uint64_t> mCPU;
    Optional<uint64_t> mRAM;
    Optional<uint64_t> mStorage;
    Optional<uint64_t> mState;

    /**
     * Compares requested resources.
     *
     * @param rhs requested resources to compare.
     * @return bool.
     */
    bool operator==(const RequestedResources& rhs) const
    {
        return mCPU == rhs.mCPU && mRAM == rhs.mRAM && mStorage == rhs.mStorage && mState == rhs.mState;
    }

    /**
     * Compares requested resources.
     *
     * @param rhs requested resources to compare.
     * @return bool.
     */
    bool operator!=(const RequestedResources& rhs) const { return !operator==(rhs); }
};

/**
 * Balancing policy.
 */
class BalancingPolicyType {
public:
    enum class Enum { eNone, eBalancingDisabled };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sBalancingPolicyStrings[] = {
            "none",
            "disabled",
        };

        return Array<const char* const>(sBalancingPolicyStrings, ArraySize(sBalancingPolicyStrings));
    };
};

using BalancingPolicyEnum = BalancingPolicyType::Enum;
using BalancingPolicy     = EnumStringer<BalancingPolicyType>;

/**
 * Service configuration.
 */
struct ServiceConfig {
    Time                                                                           mCreated;
    StaticString<cAuthorLen>                                                       mAuthor;
    bool                                                                           mSkipResourceLimits;
    Optional<StaticString<cHostNameLen>>                                           mHostname;
    BalancingPolicy                                                                mBalancingPolicy;
    StaticArray<StaticString<cRuntimeTypeLen>, cMaxNumRunners>                     mRuntimes;
    RunParameters                                                                  mRunParameters;
    StaticMap<StaticString<cSysctlLen>, StaticString<cSysctlLen>, cSysctlMaxCount> mSysctl;
    Duration                                                                       mOfflineTTL;
    ServiceQuotas                                                                  mQuotas;
    Optional<RequestedResources>                                                   mRequestedResources;
    StaticArray<StaticString<cConnectionNameLen>, cMaxNumConnections>              mAllowedConnections;
    StaticArray<StaticString<cResourceNameLen>, cMaxNumNodeResources>              mResources;
    StaticArray<FunctionServicePermissions, cFuncServiceMaxCount>                  mPermissions;
    Optional<AlertRules>                                                           mAlertRules;

    /**
     * Compares service config.
     *
     * @param rhs service config to compare.
     * @return bool.
     */
    bool operator==(const ServiceConfig& rhs) const
    {
        return mCreated == rhs.mCreated && mAuthor == rhs.mAuthor && mSkipResourceLimits == rhs.mSkipResourceLimits
            && mHostname == rhs.mHostname && mBalancingPolicy == rhs.mBalancingPolicy && mRuntimes == rhs.mRuntimes
            && mRunParameters == rhs.mRunParameters && mSysctl == rhs.mSysctl && mOfflineTTL == rhs.mOfflineTTL
            && mQuotas == rhs.mQuotas && mAllowedConnections == rhs.mAllowedConnections
            && mRequestedResources == rhs.mRequestedResources && mAlertRules == rhs.mAlertRules;
    }

    /**
     * Compares service config.
     *
     * @param rhs service config to compare.
     * @return bool.
     */
    bool operator!=(const ServiceConfig& rhs) const { return !operator==(rhs); }
};

} // namespace aos::oci

#endif
