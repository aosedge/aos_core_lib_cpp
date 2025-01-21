/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TYPES_HPP_
#define AOS_TYPES_HPP_

#include <cstdint>

#include "aos/common/config.hpp"
#include "aos/common/tools/enum.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/log.hpp"
#include "aos/common/tools/optional.hpp"

namespace aos {

/*
 * Provider ID len.
 */
constexpr auto cProviderIDLen = AOS_CONFIG_TYPES_PROVIDER_ID_LEN;

/*
 * Service ID len.
 */
constexpr auto cServiceIDLen = AOS_CONFIG_TYPES_SERVICE_ID_LEN;

/**
 * Max number of service providers.
 */
static constexpr auto cMaxNumServiceProviders = AOS_CONFIG_TYPES_MAX_NUM_SERVICE_PROVIDERS;

/*
 * Subject ID len.
 */
constexpr auto cSubjectIDLen = AOS_CONFIG_TYPES_SUBJECT_ID_LEN;

/*
 * Max number of subject ID(s).
 */
constexpr auto cMaxSubjectIDSize = AOS_CONFIG_TYPES_MAX_SUBJECTS_SIZE;

/*
 * System ID len.
 */
constexpr auto cSystemIDLen = AOS_CONFIG_TYPES_SYSTEM_ID_LEN;

/*
 * Layer ID len.
 */
constexpr auto cLayerIDLen = AOS_CONFIG_TYPES_LAYER_ID_LEN;

/*
 * Layer digest len.
 */
constexpr auto cLayerDigestLen = AOS_CONFIG_TYPES_LAYER_DIGEST_LEN;

/*
 * Instance ID len.
 */
constexpr auto cInstanceIDLen = AOS_CONFIG_TYPES_INSTANCE_ID_LEN;

/*
 * Unit model len.
 */
constexpr auto cUnitModelLen = AOS_CONFIG_TYPES_UNIT_MODEL_LEN;

/*
 * URL len.
 */
constexpr auto cURLLen = AOS_CONFIG_TYPES_URL_LEN;

/**
 * Service/layer description len.
 */

constexpr auto cDescriptionLen = AOS_CONFIG_TYPES_DESCRIPTION_LEN;

/**
 * Max number of instances.
 */
constexpr auto cMaxNumInstances = AOS_CONFIG_TYPES_MAX_NUM_INSTANCES;

/**
 * Max number of services.
 */
constexpr auto cMaxNumServices = AOS_CONFIG_TYPES_MAX_NUM_SERVICES;

/**
 * Max number of layers.
 */
constexpr auto cMaxNumLayers = AOS_CONFIG_TYPES_MAX_NUM_LAYERS;

/**
 * Max number of nodes.
 */
constexpr auto cMaxNumNodes = AOS_CONFIG_TYPES_MAX_NUM_NODES;

/**
 * Node ID len.
 */
constexpr auto cNodeIDLen = AOS_CONFIG_TYPES_NODE_ID_LEN;

/**
 * Node type len.
 */
constexpr auto cNodeTypeLen = AOS_CONFIG_TYPES_NODE_TYPE_LEN;

/**
 * SHA256 size.
 */
constexpr auto cSHA256Size = 32;

/**
 * Error message len.
 */
constexpr auto cErrorMessageLen = AOS_CONFIG_TYPES_ERROR_MESSAGE_LEN;

/**
 * File chunk size.
 */
constexpr auto cFileChunkSize = AOS_CONFIG_TYPES_FILE_CHUNK_SIZE;

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

/*
 * Node name len.
 */
constexpr auto cNodeNameLen = AOS_CONFIG_TYPES_NODE_NAME_LEN;

/*
 * OS type len.
 */
constexpr auto cOSTypeLen = AOS_CONFIG_TYPES_OS_TYPE_LEN;

/*
 * Max number of CPUs.
 */
constexpr auto cMaxNumCPUs = AOS_CONFIG_TYPES_MAX_NUM_CPUS;

/*
 * Max number of node attributes.
 */
constexpr auto cMaxNumNodeAttributes = AOS_CONFIG_TYPES_MAX_NUM_NODE_ATTRIBUTES;

/*
 * Node attribute name len.
 */
constexpr auto cNodeAttributeNameLen = AOS_CONFIG_TYPES_NODE_ATTRIBUTE_NAME_LEN;

/*
 * Node attribute value len.
 */
constexpr auto cNodeAttributeValueLen = AOS_CONFIG_TYPES_NODE_ATTRIBUTE_VALUE_LEN;

/*
 * CPU model name len.
 */
constexpr auto cCPUModelNameLen = AOS_CONFIG_TYPES_CPU_MODEL_NAME_LEN;

/*
 * CPU arch len.
 */
constexpr auto cCPUArchLen = AOS_CONFIG_TYPES_CPU_ARCH_LEN;

/*
 * CPU arch family len.
 */
constexpr auto cCPUArchFamilyLen = AOS_CONFIG_TYPES_CPU_ARCH_FAMILY_LEN;

/**
 * Version max len.
 */
constexpr auto cVersionLen = AOS_CONFIG_TYPES_VERSION_LEN;

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
 * IP len.
 */
constexpr auto cIPLen = AOS_CONFIG_TYPES_IP_LEN;

/**
 * Port len.
 */
constexpr auto cPortLen = AOS_CONFIG_TYPES_PORT_LEN;

/**
 * Protocol name len.
 */
constexpr auto cProtocolNameLen = AOS_CONFIG_TYPES_PROTOCOL_NAME_LEN;

/**
 * Max number of DNS servers.
 */
constexpr auto cMaxNumDNSServers = AOS_CONFIG_TYPES_MAX_NUM_DNS_SERVERS;

/**
 * Max number of firewall rules.
 */
constexpr auto cMaxNumFirewallRules = AOS_CONFIG_TYPES_MAX_NUM_FIREWALL_RULES;

/**
 * Max number of networks.
 */
constexpr auto cMaxNumNetworks = AOS_CONFIG_TYPES_MAX_NUM_NETWORKS;

/**
 * Host name len.
 */
constexpr auto cHostNameLen = AOS_CONFIG_TYPES_HOST_NAME_LEN;

/**
 * Device name len.
 */
constexpr auto cDeviceNameLen = AOS_CONFIG_TYPES_DEVICE_NAME_LEN;

/**
 * Max number of host devices.
 */
constexpr auto cMaxNumHostDevices = AOS_CONFIG_TYPES_MAX_NUM_HOST_DEVICES;

/**
 * Resource name len.
 */
constexpr auto cResourceNameLen = AOS_CONFIG_TYPES_RESOURCE_NAME_LEN;

/**
 * Group name len.
 */
constexpr auto cGroupNameLen = AOS_CONFIG_TYPES_GROUP_NAME_LEN;

/**
 * Max number of groups.
 */
constexpr auto cMaxNumGroups = AOS_CONFIG_TYPES_MAX_NUM_GROUPS;

/**
 * Max number of file system mounts.
 */
constexpr auto cMaxNumFSMounts = AOS_CONFIG_TYPES_MAX_NUM_FS_MOUNTS;

/**
 * Environment variable name len.
 */
constexpr auto cEnvVarNameLen = AOS_CONFIG_TYPES_ENV_VAR_NAME_LEN;

/**
 * Max number of environment variables.
 */
constexpr auto cMaxNumEnvVariables = AOS_CONFIG_TYPES_MAX_NUM_ENV_VARIABLES;

/**
 * Max number of hosts.
 */
constexpr auto cMaxNumHosts = AOS_CONFIG_TYPES_MAX_NUM_HOSTS;

/**
 * Max number of node's devices.
 */
constexpr auto cMaxNumNodeDevices = AOS_CONFIG_TYPES_MAX_NUM_NODE_DEVICES;

/**
 * Max number of node's resources.
 */
constexpr auto cMaxNumNodeResources = AOS_CONFIG_TYPES_MAX_NUM_NODE_RESOURCES;

/**
 * Label name len.
 */
constexpr auto cLabelNameLen = AOS_CONFIG_TYPES_LABEL_NAME_LEN;

/**
 * Max number of node's labels.
 */
constexpr auto cMaxNumNodeLabels = AOS_CONFIG_TYPES_MAX_NUM_NODE_LABELS;

/**
 * Max subnet len.
 */
static constexpr auto cSubnetLen = AOS_CONFIG_TYPES_SUBNET_LEN;

/**
 * Max MAC len.
 */
static constexpr auto cMacLen = AOS_CONFIG_TYPES_MAC_LEN;

/**
 * Max iptables chain name length.
 */
static constexpr auto cIptablesChainNameLen = AOS_CONFIG_TYPES_IPTABLES_CHAIN_LEN;

/**
 * Max CNI interface name length.
 */
static constexpr auto cInterfaceLen = AOS_CONFIG_TYPES_INTERFACE_NAME_LEN;

/**
 *  Max num runners.
 */
static constexpr auto cMaxNumRunners = AOS_CONFIG_TYPES_MAX_NUM_RUNNERS;

/**
 * Runner name max length.
 */
static constexpr auto cRunnerNameLen = AOS_CONFIG_TYPES_RUNNER_NAME_LEN;

/**
 * Permissions length.
 */
static constexpr auto cPermissionsLen = AOS_CONFIG_TYPES_PERMISSIONS_LEN;

/**
 * Function name length.
 */
static constexpr auto cFunctionLen = AOS_CONFIG_TYPES_FUNCTION_LEN;

/**
 * Max number of functions for functional service.
 */
static constexpr auto cFunctionsMaxCount = AOS_CONFIG_TYPES_FUNCTIONS_MAX_COUNT;

/**
 * Functional service name length.
 */
static constexpr auto cFuncServiceLen = AOS_CONFIG_TYPES_FUNC_SERVICE_LEN;

/**
 * Maximum number of functional services.
 */
static constexpr auto cFuncServiceMaxCount = AOS_CONFIG_TYPES_FUNC_SERVICE_MAX_COUNT;

/**
 * Max number of outdated items.
 */
static constexpr auto cMaxNumOutdatedItems = AOS_CONFIG_TYPES_MAX_OUTDATED_ITEMS;

/**
 * Instance identification.
 */
struct InstanceIdent {
    StaticString<cServiceIDLen> mServiceID;
    StaticString<cSubjectIDLen> mSubjectID;
    uint64_t                    mInstance;

    /**
     * Compares instance ident.
     *
     * @param instance ident to compare.
     * @return bool.
     */
    bool operator==(const InstanceIdent& instance) const
    {
        return mServiceID == instance.mServiceID && mSubjectID == instance.mSubjectID
            && mInstance == instance.mInstance;
    }

    /**
     * Compares instance ident.
     *
     * @param instance ident to compare.
     * @return bool.
     */
    bool operator!=(const InstanceIdent& instance) const { return !operator==(instance); }

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
        return log << "{" << instanceIdent.mServiceID << ":" << instanceIdent.mSubjectID << ":"
                   << instanceIdent.mInstance << "}";
    }
};

/**
 * Firewall rule.
 */
struct FirewallRule {
    StaticString<cIPLen>           mDstIP;
    StaticString<cPortLen>         mDstPort;
    StaticString<cProtocolNameLen> mProto;
    StaticString<cIPLen>           mSrcIP;

    /**
     * Compares firewall rule.
     *
     * @param rule firewall rule to compare.
     * @return bool.
     */
    bool operator==(const FirewallRule& rule) const
    {
        return mDstIP == rule.mDstIP && mDstPort == rule.mDstPort && mProto == rule.mProto && mSrcIP == rule.mSrcIP;
    }

    /**
     * Compares firewall rule.
     *
     * @param rule firewall rule to compare.
     * @return bool.
     */
    bool operator!=(const FirewallRule& rule) const { return !operator==(rule); }
};

/**
 * Networks parameters.
 */
struct NetworkParameters {
    StaticString<cHostNameLen>                                 mNetworkID;
    StaticString<cSubnetLen>                                   mSubnet;
    StaticString<cIPLen>                                       mIP;
    uint64_t                                                   mVlanID;
    StaticArray<StaticString<cHostNameLen>, cMaxNumDNSServers> mDNSServers;
    StaticArray<FirewallRule, cMaxNumFirewallRules>            mFirewallRules;

    /**
     * Compares network parameters.
     *
     * @param networkParams network parameters to compare.
     * @return bool.
     */
    bool operator==(const NetworkParameters& networkParams) const
    {
        return mNetworkID == networkParams.mNetworkID && mSubnet == networkParams.mSubnet && mIP == networkParams.mIP
            && mVlanID == networkParams.mVlanID && mDNSServers == networkParams.mDNSServers
            && mFirewallRules == networkParams.mFirewallRules;
    }

    /**
     * Compares network parameters.
     *
     * @param networkParams network parameters to compare.
     * @return bool.
     */
    bool operator!=(const NetworkParameters& networkParams) const { return !operator==(networkParams); }
};

/**
 * Instance info.
 */
struct InstanceInfo {
    InstanceIdent              mInstanceIdent;
    uint32_t                   mUID;
    uint64_t                   mPriority;
    StaticString<cFilePathLen> mStoragePath;
    StaticString<cFilePathLen> mStatePath;
    NetworkParameters          mNetworkParameters;

    /**
     * Compares instance info.
     *
     * @param instance info to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfo& instance) const
    {
        return mInstanceIdent == instance.mInstanceIdent && mUID == instance.mUID && mPriority == instance.mPriority
            && mStoragePath == instance.mStoragePath && mStatePath == instance.mStatePath
            && mNetworkParameters == instance.mNetworkParameters;
    }

    /**
     * Compares instance info.
     *
     * @param instance info to compare.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& instance) const { return !operator==(instance); }
};

/**
 * Instance info static array.
 */
using InstanceInfoStaticArray = StaticArray<InstanceInfo, cMaxNumInstances>;

/**
 * Instance run state.
 */
class InstanceRunStateType {
public:
    enum class Enum { eFailed, eActive, eNumStates };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sInstanceRunStateStrings[] = {"failed", "active"};

        return Array<const char* const>(sInstanceRunStateStrings, ArraySize(sInstanceRunStateStrings));
    };
};

using InstanceRunStateEnum = InstanceRunStateType::Enum;
using InstanceRunState     = EnumStringer<InstanceRunStateType>;

/**
 * Instance status.
 */
struct InstanceStatus {
    InstanceIdent             mInstanceIdent;
    StaticString<cVersionLen> mServiceVersion;
    InstanceRunState          mRunState;
    Error                     mError;

    /**
     * Compares instance status.
     *
     * @param instance status to compare.
     * @return bool.
     */
    bool operator==(const InstanceStatus& instance) const
    {
        return mInstanceIdent == instance.mInstanceIdent && mServiceVersion == instance.mServiceVersion
            && mRunState == instance.mRunState && mError == instance.mError;
    }

    /**
     * Compares instance status.
     *
     * @param instance status to compare.
     * @return bool.
     */
    bool operator!=(const InstanceStatus& instance) const { return !operator==(instance); }
};

/**
 * Instance status static array.
 */
using InstanceStatusStaticArray = StaticArray<InstanceStatus, cMaxNumInstances>;

/**
 * Service info.
 */

struct ServiceInfo {
    StaticString<cServiceIDLen>       mServiceID;
    StaticString<cProviderIDLen>      mProviderID;
    StaticString<cVersionLen>         mVersion;
    uint32_t                          mGID;
    StaticString<cURLLen>             mURL;
    StaticArray<uint8_t, cSHA256Size> mSHA256;
    size_t                            mSize;

    /**
     * Compares service info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator==(const ServiceInfo& info) const
    {
        return mServiceID == info.mServiceID && mProviderID == info.mProviderID && mVersion == info.mVersion
            && mGID == info.mGID && mURL == info.mURL && mSHA256 == info.mSHA256 && mSize == info.mSize;
    }

    /**
     * Compares service info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator!=(const ServiceInfo& info) const { return !operator==(info); }
};

/**
 * Service info static array.
 */
using ServiceInfoStaticArray = StaticArray<ServiceInfo, cMaxNumServices>;

/**
 * Layer info.
 */

// LayerInfo layer info.
struct LayerInfo {
    StaticString<cLayerIDLen>         mLayerID;
    StaticString<cLayerDigestLen>     mLayerDigest;
    StaticString<cVersionLen>         mVersion;
    StaticString<cURLLen>             mURL;
    StaticArray<uint8_t, cSHA256Size> mSHA256;
    size_t                            mSize;

    /**
     * Compares layer info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator==(const LayerInfo& info) const
    {
        return mLayerID == info.mLayerID && mLayerDigest == info.mLayerDigest && mVersion == info.mVersion
            && mURL == info.mURL && mSHA256 == info.mSHA256 && mSize == info.mSize;
    }

    /**
     * Compares layer info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator!=(const LayerInfo& info) const { return !operator==(info); }
};

/**
 * Layer info static array.
 */
using LayerInfoStaticArray = StaticArray<LayerInfo, cMaxNumLayers>;

/**
 * File system mount.
 */
struct Mount {
    /**
     * Crates mount.
     */
    Mount() = default;

    /**
     * Creates mount.
     *
     * @param source source.
     * @param destination destination.
     * @param mType mount type.
     * @param options mount options separated by comma e.g. "ro,bind".
     */
    Mount(const String& source, const String& destination, const String& mType, const String& options = "")
        : mDestination(destination)
        , mType(mType)
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
 * Host.
 */
struct Host {
    StaticString<cIPLen>       mIP;
    StaticString<cHostNameLen> mHostname;

    /**
     * Compares host.
     *
     * @param host host to compare.
     * @return bool.
     */
    bool operator==(const Host& host) const { return mIP == host.mIP && mHostname == host.mHostname; }

    /**
     * Compares host.
     *
     * @param host host to compare.
     * @return bool.
     */
    bool operator!=(const Host& host) const { return !operator==(host); }
};

/**
 * Device info.
 */
struct DeviceInfo {
    StaticString<cDeviceNameLen>                                  mName;
    int                                                           mSharedCount {0};
    StaticArray<StaticString<cGroupNameLen>, cMaxNumGroups>       mGroups;
    StaticArray<StaticString<cDeviceNameLen>, cMaxNumHostDevices> mHostDevices;

    /**
     * Compares device info.
     *
     * @param deviceInfo device info to compare.
     * @return bool.
     */
    bool operator==(const DeviceInfo& deviceInfo) const
    {
        return mName == deviceInfo.mName && mSharedCount == deviceInfo.mSharedCount && mGroups == deviceInfo.mGroups
            && mHostDevices == deviceInfo.mHostDevices;
    }

    /**
     * Compares device info.
     *
     * @param deviceInfo device info to compare.
     * @return bool.
     */
    bool operator!=(const DeviceInfo& deviceInfo) const { return !operator==(deviceInfo); }
};

/**
 * Resource info.
 */
struct ResourceInfo {
    StaticString<cResourceNameLen>                                 mName;
    StaticArray<StaticString<cGroupNameLen>, cMaxNumGroups>        mGroups;
    StaticArray<Mount, cMaxNumFSMounts>                            mMounts;
    StaticArray<StaticString<cEnvVarNameLen>, cMaxNumEnvVariables> mEnv;
    StaticArray<Host, cMaxNumHosts>                                mHosts;

    /**
     * Compares resource info.
     *
     * @param resourceInfo resource info to compare.
     * @return bool.
     */
    bool operator==(const ResourceInfo& resourceInfo) const
    {
        return mName == resourceInfo.mName && mGroups == resourceInfo.mGroups && mMounts == resourceInfo.mMounts
            && mEnv == resourceInfo.mEnv && mHosts == resourceInfo.mHosts;
    }

    /**
     * Compares resource info.
     *
     * @param resourceInfo resource info to compare.
     * @return bool.
     */
    bool operator!=(const ResourceInfo& resourceInfo) const { return !operator==(resourceInfo); }
};

/**
 * Alert rule percents.
 */
struct AlertRulePercents {
    Duration mMinTimeout;
    double   mMinThreshold;
    double   mMaxThreshold;

    /**
     * Compares alert rule percents.
     *
     * @param rule alert rule percents to compare.
     * @return bool.
     */
    bool operator==(const AlertRulePercents& rule) const
    {
        return mMinTimeout == rule.mMinTimeout && mMinThreshold == rule.mMinThreshold
            && mMaxThreshold == rule.mMaxThreshold;
    }

    /**
     * Compares alert rule percents.
     *
     * @param rule alert rule percents to compare.
     * @return bool.
     */
    bool operator!=(const AlertRulePercents& rule) const { return !operator==(rule); }
};

struct AlertRulePoints {
    Duration mMinTimeout;
    uint64_t mMinThreshold;
    uint64_t mMaxThreshold;

    /**
     * Compares alert rule points.
     *
     * @param rule alert rule points to compare.
     * @return bool.
     */
    bool operator==(const AlertRulePoints& rule) const
    {
        return mMinTimeout == rule.mMinTimeout && mMinThreshold == rule.mMinThreshold
            && mMaxThreshold == rule.mMaxThreshold;
    }

    /**
     * Compares alert rule points.
     *
     * @param rule alert rule points to compare.
     * @return bool.
     */
    bool operator!=(const AlertRulePoints& rule) const { return !operator==(rule); }
};

/**
 * Partition alert rule.
 */
struct PartitionAlertRule : public AlertRulePercents {
    StaticString<cPartitionNameLen> mName;

    /**
     * Compares partition alert rule.
     *
     * @param rule partition alert rule to compare.
     * @return bool.
     */
    bool operator==(const PartitionAlertRule& rule) const
    {
        return mName == rule.mName && static_cast<const AlertRulePercents&>(*this) == rule;
    }

    /**
     * Compares partition alert rule.
     *
     * @param rule partition alert rule to compare.
     * @return bool.
     */
    bool operator!=(const PartitionAlertRule& rule) const { return !operator==(rule); }
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
     * @param rules alert rules to compare.
     * @return bool.
     */
    bool operator==(const AlertRules& rules) const
    {
        return mRAM == rules.mRAM && mCPU == rules.mCPU && mPartitions == rules.mPartitions
            && mDownload == rules.mDownload && mUpload == rules.mUpload;
    }

    /**
     * Compares alert rules.
     *
     * @param rules alert rules to compare.
     * @return bool.
     */
    bool operator!=(const AlertRules& rules) const { return !operator==(rules); }
};

/**
 * Node config.
 */
struct NodeConfig {
    StaticString<cNodeTypeLen>                                  mNodeType;
    StaticArray<DeviceInfo, cMaxNumNodeDevices>                 mDevices;
    StaticArray<ResourceInfo, cMaxNumNodeResources>             mResources;
    StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels> mLabels;
    uint32_t                                                    mPriority {0};
    Optional<AlertRules>                                        mAlertRules;

    /**
     * Compares node configs.
     *
     * @param nodeConfig node config to compare.
     * @return bool.
     */
    bool operator==(const NodeConfig& nodeConfig) const
    {
        return mNodeType == nodeConfig.mNodeType && mDevices == nodeConfig.mDevices
            && mResources == nodeConfig.mResources && mLabels == nodeConfig.mLabels && mPriority == nodeConfig.mPriority
            && mAlertRules == nodeConfig.mAlertRules;
    }

    /**
     * Compares node configs.
     *
     * @param nodeConfig node config to compare.
     * @return bool.
     */
    bool operator!=(const NodeConfig& nodeConfig) const { return !operator==(nodeConfig); }
};
/**
 * Partition info.
 */
struct PartitionInfo {
    StaticString<cPartitionNameLen>                                     mName;
    StaticArray<StaticString<cPartitionTypeLen>, cMaxNumPartitionTypes> mTypes;
    StaticString<cFilePathLen>                                          mPath;
    size_t                                                              mTotalSize;
    size_t                                                              mUsedSize;

    /**
     * Compares partition info.
     *
     * @param info partition info to compare with.
     * @return bool.
     */
    bool operator==(const PartitionInfo& info) const
    {
        return mName == info.mName && mPath == info.mPath && mTypes == info.mTypes && mTotalSize == info.mTotalSize
            && mUsedSize == info.mUsedSize;
    }

    /**
     * Compares partition info.
     *
     * @param info partition info to compare with.
     * @return bool.
     */
    bool operator!=(const PartitionInfo& info) const { return !operator==(info); }
};

/**
 * Partition info static array.
 */
using PartitionInfoStaticArray = StaticArray<PartitionInfo, cMaxNumPartitions>;

/**
 * CPU info.
 */
struct CPUInfo {
    StaticString<cCPUModelNameLen>  mModelName;
    size_t                          mNumCores;
    size_t                          mNumThreads;
    StaticString<cCPUArchLen>       mArch;
    StaticString<cCPUArchFamilyLen> mArchFamily;
    uint64_t                        mMaxDMIPS;

    /**
     * Compares CPU info.
     *
     * @param info cpu info to compare with.
     * @return bool.
     */
    bool operator==(const CPUInfo& info) const
    {
        return mModelName == info.mModelName && mNumCores == info.mNumCores && mNumThreads == info.mNumThreads
            && mArch == info.mArch && mArchFamily == info.mArchFamily && mMaxDMIPS == info.mMaxDMIPS;
    }

    /**
     * Compares CPU info.
     *
     * @param info cpu info to compare with.
     * @return bool.
     */
    bool operator!=(const CPUInfo& info) const { return !operator==(info); }
};

/**
 * CPU info static array.
 */
using CPUInfoStaticArray = StaticArray<CPUInfo, cMaxNumCPUs>;

/**
 * Node attribute enum.
 */
class NodeAttributeType {
public:
    enum class Enum {
        eMainNode,
        eAosComponents,
        eNodeRunners,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sNodeStatusStrings[] = {
            "MainNode",
            "AosComponents",
            "NodeRunners",
        };

        return Array<const char* const>(sNodeStatusStrings, ArraySize(sNodeStatusStrings));
    };
};

using NodeAttributeEnum = NodeAttributeType::Enum;
using NodeAttributeName = EnumStringer<NodeAttributeType>;

/**
 * Runner enum.
 */
class RunnerType {
public:
    enum class Enum {
        eRUNC,
        eCRUN,
        eXRUN,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sNodeStatusStrings[] = {
            "runc",
            "crun",
            "xrun",
        };

        return Array<const char* const>(sNodeStatusStrings, ArraySize(sNodeStatusStrings));
    };
};

using RunnerEnum = RunnerType::Enum;
using Runner     = EnumStringer<RunnerType>;

/**
 * Node attribute.
 */
struct NodeAttribute {
    StaticString<cNodeAttributeNameLen>  mName;
    StaticString<cNodeAttributeValueLen> mValue;

    /**
     * Compares node attributes.
     *
     * @param info node attributes info to compare with.
     * @return bool.
     */
    bool operator==(const NodeAttribute& info) const { return mName == info.mName && mValue == info.mValue; }

    /**
     * Compares node attributes.
     *
     * @param info node attributes info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeAttribute& info) const { return !operator==(info); }
};

/**
 * Node attribute static array.
 */
using NodeAttributeStaticArray = StaticArray<NodeAttribute, cMaxNumNodeAttributes>;

/**
 * Node status.
 */
class NodeStatusType {
public:
    enum class Enum {
        eUnprovisioned,
        eProvisioned,
        ePaused,
        eNumStates,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sNodeStatusStrings[] = {
            "unprovisioned",
            "provisioned",
            "paused",
        };

        return Array<const char* const>(sNodeStatusStrings, ArraySize(sNodeStatusStrings));
    };
};

using NodeStatusEnum = NodeStatusType::Enum;
using NodeStatus     = EnumStringer<NodeStatusType>;

/**
 * Node info.
 */
struct NodeInfo {
    StaticString<cNodeIDLen>   mNodeID;
    StaticString<cNodeTypeLen> mNodeType;
    StaticString<cNodeNameLen> mName;
    NodeStatus                 mStatus;
    StaticString<cOSTypeLen>   mOSType;
    CPUInfoStaticArray         mCPUs;
    PartitionInfoStaticArray   mPartitions;
    NodeAttributeStaticArray   mAttrs;
    uint64_t                   mMaxDMIPS = 0;
    uint64_t                   mTotalRAM = 0;

    Error GetRunners(Array<StaticString<cRunnerNameLen>>& runners) const
    {
        auto [attr, err] = mAttrs.FindIf([](const NodeAttribute& attr) {
            return attr.mName == NodeAttributeName(NodeAttributeEnum::eNodeRunners).ToString();
        });
        if (!err.IsNone()) {
            return err;
        }

        if (err = attr->mValue.Split(runners, ','); !err.IsNone()) {
            return err;
        }

        for (auto& runner : runners) {
            runner.Trim(" ");
        }

        return ErrorEnum::eNone;
    }

    /**
     * Compares node info.
     *
     * @param info node info to compare with.
     * @return bool.
     */
    bool operator==(const NodeInfo& info) const
    {
        return mNodeID == info.mNodeID && mNodeType == info.mNodeType && mName == info.mName && mStatus == info.mStatus
            && mOSType == info.mOSType && mCPUs == info.mCPUs && mMaxDMIPS == info.mMaxDMIPS
            && mTotalRAM == info.mTotalRAM && mPartitions == info.mPartitions && mAttrs == info.mAttrs;
    }

    /**
     * Compares node info.
     *
     * @param info node info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeInfo& info) const { return !operator==(info); }
};

/**
 * Service run parameters.
 */
struct RunParameters {
    Duration mStartInterval;
    Duration mRestartInterval;
    long     mStartBurst;

    /**
     * Compares run parameters.
     *
     * @param params run parameters to compare.
     * @return bool.
     */
    bool operator==(const RunParameters& params) const
    {
        return mStartInterval == params.mStartInterval && mRestartInterval == params.mRestartInterval
            && mStartBurst == params.mStartBurst;
    }

    /**
     * Compares run parameters.
     *
     * @param params run parameters to compare.
     * @return bool.
     */
    bool operator!=(const RunParameters& params) const { return !operator==(params); }
};

/**
 * Function permissions.
 */
struct FunctionPermissions {
    StaticString<cFunctionLen>    mFunction;
    StaticString<cPermissionsLen> mPermissions;

    /**
     * Compares permission key value.
     *
     * @param rhs object to compare.
     * @return bool.
     */
    bool operator==(const FunctionPermissions& rhs)
    {
        return (mFunction == rhs.mFunction) && (mPermissions == rhs.mPermissions);
    }
};

/**
 * Function service permissions.
 */
struct FunctionServicePermissions {
    StaticString<cFuncServiceLen>                        mName;
    StaticArray<FunctionPermissions, cFunctionsMaxCount> mPermissions;
};

} // namespace aos

#endif
