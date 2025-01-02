/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SERVICECONFIG_HPP_
#define AOS_SERVICECONFIG_HPP_

#include "aos/common/ocispec/common.hpp"
#include "aos/common/tools/map.hpp"
#include "aos/common/tools/optional.hpp"
#include "aos/common/types.hpp"

namespace aos::oci {

/**
 * Author len.
 */
constexpr auto cAuthorLen = AOS_CONFIG_OCISPEC_AUTHOR_LEN;

/**
 * Balancing policy len.
 */
constexpr auto cBalancingPolicyLen = AOS_CONFIG_OCISPEC_BALANCING_POLICY_LEN;

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
     * @param quotas service quotas to compare.
     * @return bool.
     */
    bool operator==(const ServiceQuotas& quotas) const
    {
        return mCPUDMIPSLimit == quotas.mCPUDMIPSLimit && mRAMLimit == quotas.mRAMLimit
            && mPIDsLimit == quotas.mPIDsLimit && mNoFileLimit == quotas.mNoFileLimit && mTmpLimit == quotas.mTmpLimit
            && mStateLimit == quotas.mStateLimit && mStorageLimit == quotas.mStorageLimit
            && mUploadSpeed == quotas.mUploadSpeed && mDownloadSpeed == quotas.mDownloadSpeed
            && mUploadLimit == quotas.mUploadLimit && mDownloadLimit == quotas.mDownloadLimit;
    }

    /**
     * Compares service quotas.
     *
     * @param quotas service quotas to compare.
     * @return bool.
     */
    bool operator!=(const ServiceQuotas& quotas) const { return !operator==(quotas); }
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
     * @param resources requested resources to compare.
     * @return bool.
     */
    bool operator==(const RequestedResources& resources) const
    {
        return mCPU == resources.mCPU && mRAM == resources.mRAM && mStorage == resources.mStorage
            && mState == resources.mState;
    }

    /**
     * Compares requested resources.
     *
     * @param resources requested resources to compare.
     * @return bool.
     */
    bool operator!=(const RequestedResources& resources) const { return !operator==(resources); }
};

/**
 * Service configuration.
 */
struct ServiceConfig {
    Time                                                                              mCreated;
    StaticString<cAuthorLen>                                                          mAuthor;
    bool                                                                              mSkipResourceLimits;
    Optional<StaticString<cHostNameLen>>                                              mHostname;
    StaticString<cBalancingPolicyLen>                                                 mBalancingPolicy;
    StaticArray<StaticString<cRunnerNameLen>, cMaxNumRunners>                         mRunners;
    RunParameters                                                                     mRunParameters;
    StaticMap<StaticString<cMaxParamLen>, StaticString<cMaxParamLen>, cMaxParamCount> mSysctl;
    Duration                                                                          mOfflineTTL;
    ServiceQuotas                                                                     mQuotas;
    Optional<RequestedResources>                                                      mRequestedResources;
    Optional<AlertRules>                                                              mAlertRules;

    /**
     * Compares service config.
     *
     * @param config service config to compare.
     * @return bool.
     */
    bool operator==(const ServiceConfig& config) const
    {
        return mCreated == config.mCreated && mAuthor == config.mAuthor
            && mSkipResourceLimits == config.mSkipResourceLimits && mHostname == config.mHostname
            && mBalancingPolicy == config.mBalancingPolicy && mRunners == config.mRunners
            && mRunParameters == config.mRunParameters && mSysctl == config.mSysctl && mOfflineTTL == config.mOfflineTTL
            && mQuotas == config.mQuotas && mRequestedResources == config.mRequestedResources
            && mAlertRules == config.mAlertRules;
    }

    /**
     * Compares service config.
     *
     * @param config service config to compare.
     * @return bool.
     */
    bool operator!=(const ServiceConfig& config) const { return !operator==(config); }
};

} // namespace aos::oci

#endif
