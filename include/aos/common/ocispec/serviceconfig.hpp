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
};

/**
 * Requested resources.
 */
struct RequestedResources {
    Optional<uint64_t> mCPU;
    Optional<uint64_t> mRAM;
    Optional<uint64_t> mStorage;
    Optional<uint64_t> mState;
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
};

} // namespace aos::oci

#endif
