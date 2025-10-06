/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_COMMON_HPP_
#define AOS_CORE_COMMON_TYPES_COMMON_HPP_

#include <core/common/config.hpp>
#include <core/common/tools/enum.hpp>
#include <core/common/tools/log.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/time.hpp>

namespace aos {

/*
 *  ID len.
 */
constexpr auto cIDLen = AOS_CONFIG_TYPES_ID_LEN;

/**
 * Version max len.
 */
constexpr auto cVersionLen = AOS_CONFIG_TYPES_VERSION_LEN;

/**
 * Max number of nodes.
 */
constexpr auto cMaxNumNodes = AOS_CONFIG_TYPES_MAX_NUM_NODES;

/*
 * Partition name len.
 */
constexpr auto cPartitionNameLen = AOS_CONFIG_TYPES_PARTITION_NAME_LEN;

/*
 * Max number of partitions.
 */
constexpr auto cMaxNumPartitions = AOS_CONFIG_TYPES_MAX_NUM_PARTITIONS;

/**
 * Partition type len.
 */
constexpr auto cPartitionTypeLen = AOS_CONFIG_TYPES_PARTITION_TYPES_LEN;

/*
 * Max number of partition types.
 */
constexpr auto cMaxNumPartitionTypes = AOS_CONFIG_TYPES_MAX_NUM_PARTITION_TYPES;

/**
 * Resource name len.
 */
constexpr auto cResourceNameLen = AOS_CONFIG_TYPES_RESOURCE_NAME_LEN;

/*
 * URL len.
 */
constexpr auto cURLLen = AOS_CONFIG_TYPES_URL_LEN;

/*
 * Maximum number of URLs.
 */
constexpr auto cMaxNumURLs = AOS_CONFIG_TYPES_MAX_NUM_URLS;

/*
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_TYPES_FILE_PATH_LEN;

/**
 * Certificate secret size.
 */
static constexpr auto cCertSecretSize = AOS_CONFIG_TYPES_CERT_SECRET_SIZE;

/**
 * Max number of concurrent items.
 */
constexpr auto cMaxNumConcurrentItems = AOS_CONFIG_TYPES_MAX_CONCURRENT_ITEMS;

/**
 * Core component type.
 */
class CoreComponentType {
public:
    enum class Enum {
        eCM,
        eSM,
        eIAM,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sTargetTypeStrings[] = {
            "CM",
            "SM",
            "IAM",
        };

        return Array<const char* const>(sTargetTypeStrings, ArraySize(sTargetTypeStrings));
    };
};

using CoreComponentEnum = CoreComponentType::Enum;
using CoreComponent     = EnumStringer<CoreComponentType>;

/**
 * Update item type.
 */
class UpdateItemTypeType {
public:
    enum class Enum {
        eComponent,
        eService,
        eLayer,
        eNode,
        eRuntime,
        eSubject,
        eOEM,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "component",
            "service",
            "layer",
            "subject",
            "OEM",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using UpdateItemTypeEnum = UpdateItemTypeType::Enum;
using UpdateItemType     = EnumStringer<UpdateItemTypeType>;

/**
 * Cert type type.
 */
class CertTypeType {
public:
    enum class Enum {
        eOffline,
        eOnline,
        eSM,
        eCM,
        eIAM,
        eNumCertificates,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "offline",
            "online",
            "sm",
            "cm",
            "iam",
            "unknown",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using CertTypeEnum = CertTypeType::Enum;
using CertType     = EnumStringer<CertTypeType>;

/**
 * Instance identification.
 */
struct InstanceIdent {
    StaticString<cIDLen> mItemID;
    StaticString<cIDLen> mSubjectID;
    uint64_t             mInstance {};

    /**
     * Compares instance ident.
     *
     * @param rhs ident to compare.
     * @return bool.
     */
    bool operator<(const InstanceIdent& rhs) const
    {
        return mItemID <= rhs.mItemID && mSubjectID <= rhs.mSubjectID && mInstance < rhs.mInstance;
    }

    /**
     * Compares instance ident.
     *
     * @param rhs ident to compare.
     * @return bool.
     */
    bool operator==(const InstanceIdent& rhs) const
    {
        return mItemID == rhs.mItemID && mSubjectID == rhs.mSubjectID && mInstance == rhs.mInstance;
    }

    /**
     * Compares instance ident.
     *
     * @param rhs ident to compare.
     * @return bool.
     */
    bool operator!=(const InstanceIdent& rhs) const { return !operator==(rhs); }

    /**
     * Outputs instance ident to log.
     *
     * @param log log to output.
     * @param instanceIdent instance ident.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const InstanceIdent& instanceIdent)
    {
        return log << "{" << instanceIdent.mItemID << ":" << instanceIdent.mSubjectID << ":" << instanceIdent.mInstance
                   << "}";
    }
};

/**
 * Alert rule percents.
 */
struct AlertRulePercents {
    Duration mMinTimeout {};
    double   mMinThreshold {};
    double   mMaxThreshold {};

    /**
     * Compares alert rule percents.
     *
     * @param rhs alert rule percents to compare.
     * @return bool.
     */
    bool operator==(const AlertRulePercents& rhs) const
    {
        return mMinTimeout == rhs.mMinTimeout && mMinThreshold == rhs.mMinThreshold
            && mMaxThreshold == rhs.mMaxThreshold;
    }

    /**
     * Compares alert rule percents.
     *
     * @param rhs alert rule percents to compare.
     * @return bool.
     */
    bool operator!=(const AlertRulePercents& rhs) const { return !operator==(rhs); }
};

struct AlertRulePoints {
    Duration mMinTimeout {};
    uint64_t mMinThreshold {};
    uint64_t mMaxThreshold {};

    /**
     * Compares alert rule points.
     *
     * @param rhs alert rule points to compare.
     * @return bool.
     */
    bool operator==(const AlertRulePoints& rhs) const
    {
        return mMinTimeout == rhs.mMinTimeout && mMinThreshold == rhs.mMinThreshold
            && mMaxThreshold == rhs.mMaxThreshold;
    }

    /**
     * Compares alert rule points.
     *
     * @param rhs alert rule points to compare.
     * @return bool.
     */
    bool operator!=(const AlertRulePoints& rhs) const { return !operator==(rhs); }
};

/**
 * Partition alert rule.
 */
struct PartitionAlertRule : public AlertRulePercents {
    StaticString<cPartitionNameLen> mName;

    /**
     * Compares partition alert rule.
     *
     * @param rhs partition alert rule to compare.
     * @return bool.
     */
    bool operator==(const PartitionAlertRule& rhs) const
    {
        return mName == rhs.mName && static_cast<const AlertRulePercents&>(*this) == rhs;
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
    Optional<AlertRulePercents>                        mRAM;
    Optional<AlertRulePercents>                        mCPU;
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
 * Resource ratios.
 */
struct ResourceRatios {
    Optional<double> mCPU;
    Optional<double> mRAM;
    Optional<double> mStorage;
    Optional<double> mState;

    /**
     * Compares resource ratios.
     *
     * @param rhs resource ratios to compare.
     * @return bool.
     */
    bool operator==(const ResourceRatios& rhs) const
    {
        return mCPU == rhs.mCPU && mRAM == rhs.mRAM && mStorage == rhs.mStorage && mState == rhs.mState;
    }

    /**
     * Compares resource ratios.
     *
     * @param rhs resource ratios to compare.
     * @return bool.
     */
    bool operator!=(const ResourceRatios& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
