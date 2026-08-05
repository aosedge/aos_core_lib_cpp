/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_OCISPEC_ITEMCONFIG_HPP_
#define AOS_CORE_COMMON_OCISPEC_ITEMCONFIG_HPP_

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
    friend bool operator==(const ServiceQuotas& lhs, const ServiceQuotas& rhs)
    {
        return lhs.mCPUDMIPSLimit == rhs.mCPUDMIPSLimit && lhs.mRAMLimit == rhs.mRAMLimit
            && lhs.mPIDsLimit == rhs.mPIDsLimit && lhs.mNoFileLimit == rhs.mNoFileLimit
            && lhs.mTmpLimit == rhs.mTmpLimit && lhs.mStateLimit == rhs.mStateLimit
            && lhs.mStorageLimit == rhs.mStorageLimit && lhs.mUploadSpeed == rhs.mUploadSpeed
            && lhs.mDownloadSpeed == rhs.mDownloadSpeed && lhs.mUploadLimit == rhs.mUploadLimit
            && lhs.mDownloadLimit == rhs.mDownloadLimit;
    };

    /**
     * Compares service quotas.
     *
     * @param rhs service quotas to compare.
     * @return bool.
     */
    friend bool operator!=(const ServiceQuotas& lhs, const ServiceQuotas& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const RequestedResources& lhs, const RequestedResources& rhs)
    {
        return lhs.mCPU == rhs.mCPU && lhs.mRAM == rhs.mRAM && lhs.mStorage == rhs.mStorage && lhs.mState == rhs.mState;
    };

    /**
     * Compares requested resources.
     *
     * @param rhs requested resources to compare.
     * @return bool.
     */
    friend bool operator!=(const RequestedResources& lhs, const RequestedResources& rhs) { return !(lhs == rhs); };
};

/**
 * Resource info.
 */
struct ResourceInfo {
    StaticString<cResourceNameLen> mName;
    StaticString<cPermissionsLen>  mMode;

    /**
     * Compares resource info.
     *
     * @param rhs resource info to compare.
     * @return bool.
     */
    friend bool operator==(const ResourceInfo& lhs, const ResourceInfo& rhs)
    {
        return lhs.mName == rhs.mName && lhs.mMode == rhs.mMode;
    };

    /**
     * Compares resource info.
     *
     * @param rhs resource info to compare.
     * @return bool.
     */
    friend bool operator!=(const ResourceInfo& lhs, const ResourceInfo& rhs) { return !(lhs == rhs); };
};

using ResourceInfos = StaticArray<ResourceInfo, cMaxNumNodeResources>;

/**
 * Balancing policy.
 */
class BalancingPolicyType {
public:
    enum class Enum {
        eEnabled,
        eDisabled,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sBalancingPolicyStrings[] = {
            "enabled",
            "disabled",
        };

        return Array<const char* const>(sBalancingPolicyStrings, ArraySize(sBalancingPolicyStrings));
    };
};

using BalancingPolicyEnum = BalancingPolicyType::Enum;
using BalancingPolicy     = EnumStringer<BalancingPolicyType>;

/**
 * Item configuration.
 */
struct ItemConfig {
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
    ResourceInfos                                                                  mResources;
    StaticArray<FunctionServicePermissions, cFuncServiceMaxCount>                  mPermissions;
    Optional<AlertRules>                                                           mAlertRules;

    /**
     * Compares item config.
     *
     * @param rhs item config to compare.
     * @return bool.
     */
    friend bool operator==(const ItemConfig& lhs, const ItemConfig& rhs)
    {
        return lhs.mCreated == rhs.mCreated && lhs.mAuthor == rhs.mAuthor
            && lhs.mSkipResourceLimits == rhs.mSkipResourceLimits && lhs.mHostname == rhs.mHostname
            && lhs.mBalancingPolicy == rhs.mBalancingPolicy && lhs.mRuntimes == rhs.mRuntimes
            && lhs.mRunParameters == rhs.mRunParameters && lhs.mSysctl == rhs.mSysctl
            && lhs.mOfflineTTL == rhs.mOfflineTTL && lhs.mPermissions == rhs.mPermissions
            && lhs.mResources == rhs.mResources && lhs.mQuotas == rhs.mQuotas
            && lhs.mAllowedConnections == rhs.mAllowedConnections && lhs.mRequestedResources == rhs.mRequestedResources
            && lhs.mAlertRules == rhs.mAlertRules;
    };

    /**
     * Compares item config.
     *
     * @param rhs item config to compare.
     * @return bool.
     */
    friend bool operator!=(const ItemConfig& lhs, const ItemConfig& rhs) { return !(lhs == rhs); };
};

} // namespace aos::oci

#endif
