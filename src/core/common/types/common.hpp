/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_COMMON_HPP_
#define AOS_CORE_COMMON_TYPES_COMMON_HPP_

#include <core/common/config.hpp>
#include <core/common/consts.hpp>
#include <core/common/crypto/itf/x509.hpp>
#include <core/common/tools/enum.hpp>
#include <core/common/tools/log.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/time.hpp>
#include <core/common/tools/uuid.hpp>

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

/**
 * Max number of update items.
 */
constexpr auto cMaxNumUpdateItems = AOS_CONFIG_TYPES_MAX_NUM_UPDATE_ITEMS;

/**
 * Max number of blobs.
 */
constexpr auto cMaxNumBlobs = AOS_CONFIG_TYPES_MAX_NUM_BLOBS;

/**
 * Max number of instances per update item.
 */
constexpr auto cMaxNumUpdateItemInstances = AOS_CONFIG_TYPES_MAX_NUM_UPDATE_ITEM_INSTANCES;

/**
 * Max number of instances.
 */
constexpr auto cMaxNumInstances = AOS_CONFIG_TYPES_MAX_NUM_INSTANCES;

/**
 * Node type len.
 */
constexpr auto cNodeTypeLen = AOS_CONFIG_TYPES_NODE_TYPE_LEN;

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

/**
 * Label name len.
 */
constexpr auto cLabelNameLen = AOS_CONFIG_TYPES_LABEL_NAME_LEN;

/**
 * Max number of node's labels.
 */
constexpr auto cMaxNumNodeLabels = AOS_CONFIG_TYPES_MAX_NUM_NODE_LABELS;

/**
 * Secret len.
 */
constexpr auto cSecretLen = AOS_CONFIG_TYPES_SECRET_LEN;

/*
 * OS type len.
 */
constexpr auto cOSTypeLen = AOS_CONFIG_TYPES_OS_TYPE_LEN;

/**
 * OS feature len.
 */
constexpr auto cOSFeatureLen = AOS_CONFIG_TYPES_OS_FEATURE_LEN;

/**
 * OS features count.
 */
constexpr auto cOSFeaturesCount = AOS_CONFIG_TYPES_OS_FEATURES_COUNT;

/*
 * CPU arch len.
 */
constexpr auto cCPUArchLen = AOS_CONFIG_TYPES_CPU_ARCH_LEN;

/*
 * CPU variant len.
 */
constexpr auto cCPUVariantLen = AOS_CONFIG_TYPES_CPU_VARIANT_LEN;

/*
 * CPU model name len.
 */
constexpr auto cCPUModelNameLen = AOS_CONFIG_TYPES_CPU_MODEL_NAME_LEN;

/*
 * Max number of CPUs.
 */
constexpr auto cMaxNumCPUs = AOS_CONFIG_TYPES_MAX_NUM_CPUS;

/**
 * Runtime type len.
 */
constexpr auto cRuntimeTypeLen = AOS_CONFIG_TYPES_RUNTIME_TYPE_LEN;

/**
 * Max number of node's resources.
 */
constexpr auto cMaxNumNodeResources = AOS_CONFIG_TYPES_MAX_NUM_NODE_RESOURCES;

/**
 * Max number of runtimes per node.
 */
constexpr auto cMaxNumNodeRuntimes = AOS_CONFIG_TYPES_MAX_NUM_NODE_RUNTIMES;

/*
 * Node attribute name len.
 */
constexpr auto cNodeAttributeNameLen = AOS_CONFIG_TYPES_NODE_ATTRIBUTE_NAME_LEN;

/*
 * Node attribute value len.
 */
constexpr auto cNodeAttributeValueLen = AOS_CONFIG_TYPES_NODE_ATTRIBUTE_VALUE_LEN;

/*
 * Max number of node attributes.
 */
constexpr auto cMaxNumNodeAttributes = AOS_CONFIG_TYPES_MAX_NUM_NODE_ATTRIBUTES;

/*
 * Node title len.
 */
constexpr auto cNodeTitleLen = AOS_CONFIG_TYPES_NODE_TITLE_LEN;

/*
 * Max number of subjects.
 */
constexpr auto cMaxNumSubjects = AOS_CONFIG_TYPES_MAX_NUM_SUBJECTS;

/**
 * Max number of update item owners.
 */
static constexpr auto cMaxNumOwners = AOS_CONFIG_TYPES_MAX_NUM_OWNERS;

/*
 * Unit model len.
 */
constexpr auto cUnitModelLen = AOS_CONFIG_TYPES_UNIT_MODEL_LEN;

/**
 * Error message len.
 */
constexpr auto cErrorMessageLen = AOS_CONFIG_TYPES_ERROR_MESSAGE_LEN;

/**
 * File chunk size.
 */
constexpr auto cFileChunkSize = AOS_CONFIG_TYPES_FILE_CHUNK_SIZE;

/*
 * File system mount type len.
 */
constexpr auto cFSMountTypeLen = AOS_CONFIG_TYPES_FS_MOUNT_TYPE_LEN;

/**
 * File system mount option len.
 */
constexpr auto cFSMountOptionLen = AOS_CONFIG_TYPES_FS_MOUNT_OPTION_LEN;

/**
 * File system mount max number of options.
 */
constexpr auto cFSMountMaxNumOptions = AOS_CONFIG_TYPES_MAX_NUM_FS_MOUNT_OPTIONS;

/**
 * Max number of file system mounts.
 */
constexpr auto cMaxNumFSMounts = AOS_CONFIG_TYPES_MAX_NUM_FS_MOUNTS;

/**
 * Device name len.
 */
constexpr auto cDeviceNameLen = AOS_CONFIG_TYPES_DEVICE_NAME_LEN;

/**
 * Max number of host devices.
 */
constexpr auto cMaxNumHostDevices = AOS_CONFIG_TYPES_MAX_NUM_HOST_DEVICES;

/**
 * Group name len.
 */
constexpr auto cGroupNameLen = AOS_CONFIG_TYPES_GROUP_NAME_LEN;

/**
 * Max number of groups.
 */
constexpr auto cMaxNumGroups = AOS_CONFIG_TYPES_MAX_NUM_GROUPS;

/**
 * Max length of JSON.
 */
constexpr auto cJSONMaxLen = AOS_CONFIG_TYPES_JSON_MAX_LEN;

/**
 * Certificate type name length.
 */
constexpr auto cCertTypeLen = AOS_CONFIG_TYPES_CERT_TYPE_NAME_LEN;

/**
 * Bearer token len.
 */
constexpr auto cBearerTokenLen = AOS_CONFIG_TYPES_BEARER_TOKEN_LEN;

/**
 * Main node attribute.
 */
static constexpr auto cAttrMainNode = "MainNode";

/**
 * Aos components attribute.
 */
static constexpr auto cAttrAosComponents = "AosComponents";

/**
 * System info.
 */
struct SystemInfo {
    StaticString<cIDLen>        mSystemID;
    StaticString<cUnitModelLen> mUnitModel;
    StaticString<cVersionLen>   mVersion;
};

/**
 * Core component type.
 */
class CoreComponentType {
public:
    enum class Enum {
        eCM,
        eSM,
        eIAM,
        eMP,
        eNumComponents,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sTargetTypeStrings[] = {
            "CM",
            "SM",
            "IAM",
            "MP",
            "unknown",
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
        eService,
        eComponent,
        eLayer,
        eSubject,
        eOEM,
        eSP,
        eNode,
        eRuntime,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "service",
            "component",
            "layer",
            "subject",
            "oem",
            "sp",
            "node",
            "runtime",
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
 * Item state type.
 */
class ItemStateType {
public:
    enum class Enum {
        eUnknown,
        eDownloading,
        ePending,
        eInstalling,
        eInstalled,
        eRemoving,
        eRemoved,
        eFailed,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "unknown",
            "downloading",
            "pending",
            "installing",
            "installed",
            "removing",
            "removed",
            "failed",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using ItemStateEnum = ItemStateType::Enum;
using ItemState     = EnumStringer<ItemStateType>;

/**
 * Instance state type.
 */
class InstanceStateType {
public:
    enum class Enum { eActivating, eActive, eInactive, eFailed };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {"activating", "active", "inactive", "failed"};

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using InstanceStateEnum = InstanceStateType::Enum;
using InstanceState     = EnumStringer<InstanceStateType>;

/**
 * Node state.
 */
class NodeStateType {
public:
    enum class Enum {
        eUnprovisioned,
        eProvisioned,
        ePaused,
        eError,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "unprovisioned",
            "provisioned",
            "paused",
            "error",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using NodeStateEnum = NodeStateType::Enum;
using NodeState     = EnumStringer<NodeStateType>;

/**
 * Node attribute enum.
 */
class NodeAttributeType {
public:
    enum class Enum {
        eMainNode,
        eAosComponents,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "MainNode",
            "AosComponents",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using NodeAttributeEnum = NodeAttributeType::Enum;
using NodeAttributeName = EnumStringer<NodeAttributeType>;

/**
 * Subject type.
 */
class SubjectTypeType {
public:
    enum class Enum {
        eGroup,
        eUser,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "group",
            "user",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using SubjectTypeEnum = SubjectTypeType::Enum;
using SubjectType     = EnumStringer<SubjectTypeType>;

/**
 * Instance identification.
 */
struct InstanceIdent {
    StaticString<cIDLen> mItemID;
    StaticString<cIDLen> mSubjectID;
    uint64_t             mInstance {};
    UpdateItemType       mType;

    /**
     * Compares instance ident.
     *
     * @param rhs ident to compare.
     * @return bool.
     */
    bool operator<(const InstanceIdent& rhs) const
    {
        if (mItemID != rhs.mItemID) {
            return mItemID < rhs.mItemID;
        }

        if (mSubjectID != rhs.mSubjectID) {
            return mSubjectID < rhs.mSubjectID;
        }

        if (mInstance != rhs.mInstance) {
            return mInstance < rhs.mInstance;
        }

        return mType.GetValue() < rhs.mType.GetValue();
    }

    /**
     * Compares instance ident.
     *
     * @param rhs ident to compare.
     * @return bool.
     */
    bool operator==(const InstanceIdent& rhs) const
    {
        return mItemID == rhs.mItemID && mSubjectID == rhs.mSubjectID && mInstance == rhs.mInstance
            && mType == rhs.mType;
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
        return log << "{" << instanceIdent.mType << ":" << instanceIdent.mItemID << ":" << instanceIdent.mSubjectID
                   << ":" << instanceIdent.mInstance << "}";
    }
};

/**
 * Instance filter.
 */
struct InstanceFilter {
    Optional<StaticString<cIDLen>> mItemID;
    Optional<StaticString<cIDLen>> mSubjectID;
    Optional<uint64_t>             mInstance;

    /**
     * Returns true if instance ident matches filter.
     *
     * @param instanceIdent instance ident to match.
     * @return bool.
     */
    bool Match(const InstanceIdent& instanceIdent) const
    {
        return (!mItemID.HasValue() || *mItemID == instanceIdent.mItemID)
            && (!mSubjectID.HasValue() || *mSubjectID == instanceIdent.mSubjectID)
            && (!mInstance.HasValue() || *mInstance == instanceIdent.mInstance);
    }

    /**
     * Compares instance filter.
     *
     * @param rhs instance filter to compare with.
     * @return bool.
     */
    bool operator==(const InstanceFilter& rhs) const
    {
        return mItemID == rhs.mItemID && mSubjectID == rhs.mSubjectID && mInstance == rhs.mInstance;
    }

    /**
     * Compares instance filter.
     *
     * @param rhs instance filter to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceFilter& rhs) const { return !operator==(rhs); }

    /**
     * Outputs instance filter to log.
     *
     * @param log log to output.
     * @param instanceFilter instance filter.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const InstanceFilter& instanceFilter)
    {
        StaticString<32> instanceStr = "*";

        if (instanceFilter.mInstance.HasValue()) {
            instanceStr.Convert(*instanceFilter.mInstance);
        }

        return log << "{" << (instanceFilter.mItemID.HasValue() ? *instanceFilter.mItemID : "*") << ":"
                   << (instanceFilter.mSubjectID.HasValue() ? *instanceFilter.mSubjectID : "*") << ":" << instanceStr
                   << "}";
    }
};

/**
 * Subject info.
 */
struct SubjectInfo {
    StaticString<cIDLen> mSubjectID;
    SubjectType          mSubjectType;
    bool                 mIsUnitSubject {};

    /**
     * Compares subject info.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const SubjectInfo& rhs) const
    {
        return mSubjectID == rhs.mSubjectID && mSubjectType == rhs.mSubjectType && mIsUnitSubject == rhs.mIsUnitSubject;
    }

    /**
     * Compares subject info.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const SubjectInfo& rhs) const { return !operator==(rhs); }
};

using SubjectInfoArray = StaticArray<SubjectInfo, cMaxNumSubjects>;

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

/**
 * Architecture info.
 */
struct ArchInfo {
    StaticString<cCPUArchLen>              mArchitecture;
    Optional<StaticString<cCPUVariantLen>> mVariant;

    /**
     * Compares architecture info.
     *
     * @param rhs architecture info to compare with.
     * @return bool.
     */
    bool operator==(const ArchInfo& rhs) const
    {
        return mArchitecture == rhs.mArchitecture && mVariant == rhs.mVariant;
    }

    /**
     * Compares architecture info.
     *
     * @param rhs architecture info to compare with.
     * @return bool.
     */
    bool operator!=(const ArchInfo& rhs) const { return !operator==(rhs); }
};

/**
 * OS info.
 */
struct OSInfo {
    StaticString<cOSTypeLen>                                   mOS;
    Optional<StaticString<cVersionLen>>                        mVersion;
    StaticArray<StaticString<cOSFeatureLen>, cOSFeaturesCount> mFeatures;

    /**
     * Compares OS info.
     *
     * @param rhs OS info to compare with.
     * @return bool.
     */
    bool operator==(const OSInfo& rhs) const
    {
        return mOS == rhs.mOS && mVersion == rhs.mVersion && mFeatures == rhs.mFeatures;
    }

    /**
     * Compares OS info.
     *
     * @param rhs OS info to compare with.
     * @return bool.
     */
    bool operator!=(const OSInfo& rhs) const { return !operator==(rhs); }
};

/**
 * Platform info.
 */
struct PlatformInfo {
    ArchInfo mArchInfo;
    OSInfo   mOSInfo;

    /**
     * Compares platform info.
     *
     * @param rhs platform info to compare with.
     * @return bool.
     */
    bool operator==(const PlatformInfo& rhs) const { return mArchInfo == rhs.mArchInfo && mOSInfo == rhs.mOSInfo; }

    /**
     * Compares platform info.
     *
     * @param rhs platform info to compare with.
     * @return bool.
     */
    bool operator!=(const PlatformInfo& rhs) const { return !operator==(rhs); }
};

/**
 * CPU info.
 */
struct CPUInfo {
    StaticString<cCPUModelNameLen> mModelName;
    size_t                         mNumCores {};
    size_t                         mNumThreads {};
    ArchInfo                       mArchInfo;
    Optional<size_t>               mMaxDMIPS;

    /**
     * Compares CPU info.
     *
     * @param rhs cpu info to compare with.
     * @return bool.
     */
    bool operator==(const CPUInfo& rhs) const
    {
        return mModelName == rhs.mModelName && mNumCores == rhs.mNumCores && mNumThreads == rhs.mNumThreads
            && mArchInfo == rhs.mArchInfo && mMaxDMIPS == rhs.mMaxDMIPS;
    }

    /**
     * Compares CPU info.
     *
     * @param rhs cpu info to compare with.
     * @return bool.
     */
    bool operator!=(const CPUInfo& rhs) const { return !operator==(rhs); }
};

/**
 * CPU info array.
 */
using CPUInfoArray = StaticArray<CPUInfo, cMaxNumCPUs>;

/**
 * Partition info.
 */
struct PartitionInfo {
    StaticString<cPartitionNameLen>                                     mName;
    StaticArray<StaticString<cPartitionTypeLen>, cMaxNumPartitionTypes> mTypes;
    StaticString<cFilePathLen>                                          mPath;
    size_t                                                              mTotalSize {};

    /**
     * Compares partition info.
     *
     * @param rhs partition info to compare with.
     * @return bool.
     */
    bool operator==(const PartitionInfo& rhs) const
    {
        return mName == rhs.mName && mPath == rhs.mPath && mTypes == rhs.mTypes && mTotalSize == rhs.mTotalSize;
    }

    /**
     * Compares partition info.
     *
     * @param rhs partition info to compare with.
     * @return bool.
     */
    bool operator!=(const PartitionInfo& rhs) const { return !operator==(rhs); }
};

/**
 * Partition info array.
 */
using PartitionInfoArray = StaticArray<PartitionInfo, cMaxNumPartitions>;

/**
 * Resource info.
 */
struct ResourceInfo {
    StaticString<cResourceNameLen> mName;
    size_t                         mSharedCount {};

    /**
     * Compares resource info.
     *
     * @param rhs resource info to compare with.
     * @return bool.
     */
    bool operator==(const ResourceInfo& rhs) const { return mName == rhs.mName && mSharedCount == rhs.mSharedCount; }
    /**
     * Compares resource info.
     *
     * @param rhs resource info to compare with.
     * @return bool.
     */
    bool operator!=(const ResourceInfo& rhs) const { return !operator==(rhs); }
};

using ResourceInfoArray = StaticArray<ResourceInfo, cMaxNumNodeResources>;

/**
 * Runtime info.
 */
struct RuntimeInfo : public PlatformInfo {
    StaticString<cIDLen>          mRuntimeID;
    StaticString<cRuntimeTypeLen> mRuntimeType;
    Optional<size_t>              mMaxDMIPS;
    Optional<size_t>              mAllowedDMIPS;
    Optional<size_t>              mTotalRAM;
    Optional<size_t>              mAllowedRAM;
    size_t                        mMaxInstances {};

    /**
     * Compares runtime info.
     *
     * @param rhs runtime info to compare with.
     * @return bool.
     */
    bool operator==(const RuntimeInfo& rhs) const
    {
        return PlatformInfo::operator==(rhs) && mRuntimeID == rhs.mRuntimeID && mRuntimeType == rhs.mRuntimeType
            && mMaxDMIPS == rhs.mMaxDMIPS && mAllowedDMIPS == rhs.mAllowedDMIPS && mTotalRAM == rhs.mTotalRAM
            && mAllowedRAM == rhs.mAllowedRAM && mMaxInstances == rhs.mMaxInstances;
    }

    /**
     * Compares runtime info.
     *
     * @param rhs runtime info to compare with.
     * @return bool.
     */
    bool operator!=(const RuntimeInfo& rhs) const { return !operator==(rhs); }
};

using RuntimeInfoArray = StaticArray<RuntimeInfo, cMaxNumNodeRuntimes>;

/**
 * Node attribute.
 */
struct NodeAttribute {
    StaticString<cNodeAttributeNameLen>  mName;
    StaticString<cNodeAttributeValueLen> mValue;

    /**
     * Compares node attributes.
     *
     * @param rhs node attributes info to compare with.
     * @return bool.
     */
    bool operator==(const NodeAttribute& rhs) const { return mName == rhs.mName && mValue == rhs.mValue; }

    /**
     * Compares node attributes.
     *
     * @param rhs node attributes info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeAttribute& rhs) const { return !operator==(rhs); }
};

/**
 * Node attribute array.
 */
using NodeAttributeArray = StaticArray<NodeAttribute, cMaxNumNodeAttributes>;

/**
 * Node info.
 */
struct NodeInfo {
    StaticString<cIDLen>        mNodeID;
    StaticString<cNodeTypeLen>  mNodeType;
    StaticString<cNodeTitleLen> mTitle;
    size_t                      mMaxDMIPS {};
    size_t                      mTotalRAM {};
    Optional<size_t>            mPhysicalRAM;
    OSInfo                      mOSInfo;
    CPUInfoArray                mCPUs;
    PartitionInfoArray          mPartitions;
    NodeAttributeArray          mAttrs;
    NodeState                   mState;
    bool                        mIsConnected {};
    Error                       mError;

    /**
     * Compares node info.
     *
     * @param rhs node info to compare with.
     * @return bool.
     */
    bool operator==(const NodeInfo& rhs) const
    {
        return mNodeID == rhs.mNodeID && mNodeType == rhs.mNodeType && mTitle == rhs.mTitle
            && mMaxDMIPS == rhs.mMaxDMIPS && mTotalRAM == rhs.mTotalRAM && mPhysicalRAM == rhs.mPhysicalRAM
            && mOSInfo == rhs.mOSInfo && mCPUs == rhs.mCPUs && mPartitions == rhs.mPartitions && mAttrs == rhs.mAttrs
            && mState == rhs.mState && mIsConnected == rhs.mIsConnected && mError == rhs.mError;
    }

    /**
     * Compares node info.
     *
     * @param rhs node info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeInfo& rhs) const { return !operator==(rhs); }

    /**
     * Checks whether node is main node.
     *
     * @return bool.
     */
    bool IsMainNode() const
    {
        return mAttrs.FindIf([](const auto& attr) {
            return !attr.mName.Compare(cAttrMainNode, String::CaseSensitivity::CaseInsensitive);
        }) != mAttrs.end();
    }

    /**
     * Checks whether node contains given component.
     *
     * @param component component to check.
     * @return bool.
     */
    bool ContainsComponent(const CoreComponent& component) const
    {
        constexpr auto cNodeComponentStrLen  = 8;
        constexpr auto cMaxNumNodeComponents = static_cast<size_t>(CoreComponentEnum::eNumComponents);

        auto it = mAttrs.FindIf([](const NodeAttribute& attr) {
            return !attr.mName.Compare(cAttrAosComponents, String::CaseSensitivity::CaseInsensitive);
        });

        if (it == mAttrs.end()) {
            return false;
        }

        StaticArray<StaticString<cNodeComponentStrLen>, cMaxNumNodeComponents> components;

        if (auto err = it->mValue.Split(components, ','); !err.IsNone()) {
            return false;
        }

        return components.FindIf([&component](const String& compStr) {
            return !compStr.Compare(component.ToString(), String::CaseSensitivity::CaseInsensitive);
        }) != components.end();
    }
};

/*
 * Subjects.
 */
using SubjectArray = StaticArray<StaticString<cIDLen>, cMaxNumSubjects>;

/**
 * Instance run parameters.
 */
struct RunParameters {
    Optional<Duration> mStartInterval;
    Optional<Duration> mRestartInterval;
    Optional<long>     mStartBurst;

    /**
     * Compares run parameters.
     *
     * @param rhs run parameters to compare.
     * @return bool.
     */
    bool operator==(const RunParameters& rhs) const
    {
        return mStartInterval == rhs.mStartInterval && mRestartInterval == rhs.mRestartInterval
            && mStartBurst == rhs.mStartBurst;
    }

    /**
     * Compares run parameters.
     *
     * @param rhs run parameters to compare.
     * @return bool.
     */
    bool operator!=(const RunParameters& rhs) const { return !operator==(rhs); }
};

/**
 * File system mount.
 */
struct Mount {
    /**
     * Creates mount.
     */
    Mount() = default;

    /**
     * Creates mount.
     *
     * @param source source.
     * @param destination destination.
     * @param type mount type.
     * @param options mount options separated by comma e.g. "ro,bind".
     */
    Mount(const String& source, const String& destination, const String& type, const String& options = "")
        : mDestination(destination)
        , mType(type)
        , mSource(source)
    {
        [[maybe_unused]] auto err = options.Split(mOptions, ',');
        assert(err.IsNone());
    }

    /**
     * Compares file system mount.
     *
     * @param mount file system mount to compare.
     * @return bool.
     */
    bool operator==(const Mount& mount) const
    {
        return mDestination == mount.mDestination && mType == mount.mType && mSource == mount.mSource
            && mOptions == mount.mOptions;
    }

    /**
     * Compares file system mount.
     *
     * @param mount file system mount to compare.
     * @return bool.
     */
    bool operator!=(const Mount& mount) const { return !operator==(mount); }

    StaticString<cFilePathLen>                                          mDestination;
    StaticString<cFSMountTypeLen>                                       mType;
    StaticString<cFilePathLen>                                          mSource;
    StaticArray<StaticString<cFSMountOptionLen>, cFSMountMaxNumOptions> mOptions;
};

/**
 * General certificate information.
 */
struct CertInfo {
    StaticString<cCertTypeLen>                    mCertType;
    StaticArray<uint8_t, crypto::cCertIssuerSize> mIssuer;
    StaticArray<uint8_t, crypto::cSerialNumSize>  mSerial;
    StaticString<cURLLen>                         mCertURL;
    StaticString<cURLLen>                         mKeyURL;
    Time                                          mNotAfter;

    /**
     * Checks whether certificate info is equal the the current one.
     *
     * @param certInfo info to compare.
     * @return bool.
     */
    bool operator==(const CertInfo& certInfo) const
    {
        return certInfo.mCertURL == mCertURL && certInfo.mIssuer == mIssuer && certInfo.mKeyURL == mKeyURL
            && certInfo.mNotAfter == mNotAfter && certInfo.mSerial == mSerial;
    }
    /**
     * Checks whether certificate info is equal the the current one.
     *
     * @param certInfo info to compare.
     * @return bool.
     */
    bool operator!=(const CertInfo& certInfo) const { return !operator==(certInfo); }

    /**
     * Prints object to log.
     *
     * @param log log to output.
     * @param certInfo object instance.
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const CertInfo& certInfo)
    {
        return log << "{certURL = " << certInfo.mCertURL << ", keyURL = " << certInfo.mKeyURL
                   << ", notAfter = " << certInfo.mNotAfter << "}";
    }
};

/**
 * Protocol structure. Containing protocol related information.
 */
struct Protocol {
    StaticString<uuid::cUUIDLen> mCorrelationID;

    /**
     * Compares protocols.
     *
     * @param rhs protocol to compare with.
     * @return bool.
     */
    bool operator==(const Protocol& rhs) const { return mCorrelationID == rhs.mCorrelationID; }

    /**
     * Compares protocols.
     *
     * @param rhs protocol to compare with.
     * @return bool.
     */
    bool operator!=(const Protocol& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
