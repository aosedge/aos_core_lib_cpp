/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_ITF_INSTANCEPARAMSPROVIDER_HPP_
#define AOS_CORE_COMMON_MONITORING_ITF_INSTANCEPARAMSPROVIDER_HPP_

#include <core/common/types/common.hpp>

namespace aos::monitoring {

/**
 * Partition param.
 */
struct PartitionParam {
    StaticString<cPartitionNameLen> mName;
    StaticString<cFilePathLen>      mPath;
};

/**
 * Partition alert rule.
 */
struct PartitionAlertRule : public AlertRulePoints {
    StaticString<cPartitionNameLen> mName;

    /**
     * Compares partition alert rule.
     *
     * @param rhs partition alert rule to compare.
     * @return bool.
     */
    bool operator==(const PartitionAlertRule& rhs) const
    {
        return mName == rhs.mName && static_cast<const AlertRulePoints&>(*this) == rhs;
    }

    /**
     * Compares partition alert rule.
     *
     * @param rhs partition alert rule to compare.
     * @return bool.
     */
    bool operator!=(const PartitionAlertRule& rhs) const { return !operator==(rhs); }
};

/**
 * Alert rules.
 */
struct AlertRules {
    Optional<AlertRulePoints>                          mRAM;
    Optional<AlertRulePoints>                          mCPU;
    StaticArray<PartitionAlertRule, cMaxNumPartitions> mPartitions;
    Optional<AlertRulePoints>                          mDownload;
    Optional<AlertRulePoints>                          mUpload;

    /**
     * Compares alert rules.
     *
     * @param rhs alert rules to compare.
     * @return bool.
     */
    bool operator==(const AlertRules& rhs) const
    {
        return mRAM == rhs.mRAM && mCPU == rhs.mCPU && mPartitions == rhs.mPartitions && mDownload == rhs.mDownload
            && mUpload == rhs.mUpload;
    }

    /**
     * Compares alert rules.
     *
     * @param rhs alert rules to compare.
     * @return bool.
     */
    bool operator!=(const AlertRules& rhs) const { return !operator==(rhs); }
};

/**
 * Instance monitoring parameters.
 */
struct InstanceParams {
    InstanceIdent        mInstanceIdent;
    PartitionInfoArray   mPartitions;
    Optional<AlertRules> mAlertRules;
};

/**
 * Instance info provider interface.
 */
class InstanceParamsProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceParamsProviderItf() = default;

    /**
     * Gets instance monitoring parameters.
     *
     * @param instanceIdent instance ident.
     * @param[out] params instance monitoring parameters.
     * @return Error eNotSupported if instance monitoring is not supported.
     */
    virtual Error GetInstanceMonitoringParams(const InstanceIdent& instanceIdent, InstanceParams& params) const = 0;
};

} // namespace aos::monitoring

#endif
