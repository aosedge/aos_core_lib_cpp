/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_OBSOLETE_HPP_
#define AOS_CORE_COMMON_TYPES_OBSOLETE_HPP_

#include <cstdint>

#include <core/common/config.hpp>
#include <core/common/tools/enum.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/log.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/time.hpp>
#include <core/common/tools/uuid.hpp>

#include "common.hpp"

namespace aos {

/**
 * Max number of service providers.
 */
static constexpr auto cMaxNumServiceProviders = AOS_CONFIG_TYPES_MAX_NUM_SERVICE_PROVIDERS;

/*
 * Max number of subjects.
 */
constexpr auto cMaxNumSubjects = AOS_CONFIG_TYPES_MAX_NUM_SUBJECTS;

/*
 * Layer digest len.
 */
constexpr auto cLayerDigestLen = AOS_CONFIG_TYPES_LAYER_DIGEST_LEN;

/*
 * Unit model len.
 */
constexpr auto cUnitModelLen = AOS_CONFIG_TYPES_UNIT_MODEL_LEN;

/*
 * Maximum number of URLs.
 */
constexpr auto cMaxNumURLs = AOS_CONFIG_TYPES_MAX_NUM_URLS;

/**
 * Max number of update items.
 */
constexpr auto cMaxNumUpdateItems = AOS_CONFIG_TYPES_MAX_NUM_UPDATE_ITEMS;

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
constexpr auto cNodeIDLen = AOS_CONFIG_CLOUDPROTOCOL_CODENAME_LEN;

/**
 * Runtime ID len.
 */
constexpr auto cRuntimeIDLen = AOS_CONFIG_CLOUDPROTOCOL_CODENAME_LEN;

/**
 * Runtime type len.
 */
static constexpr auto cRuntimeTypeLen = AOS_CONFIG_TYPES_RUNTIME_TYPE_LEN;

/**
 * Max number of runtimes per node.
 */
static constexpr auto cMaxNumNodeRuntimes = AOS_CONFIG_TYPES_MAX_NUM_NODE_RUNTIMES;

/**
 * Node type len.
 */
constexpr auto cNodeTypeLen = AOS_CONFIG_TYPES_NODE_TYPE_LEN;

/**
 * SHA256 size.
 */
constexpr auto cSHA256Size = 32;

/**
 * SHA384 size.
 */
constexpr auto cSHA384Size = 48;

/**
 * SHA3-224 size.
 */
constexpr auto cSHA3_224Size = 28;

/**
 * Error message len.
 */
constexpr auto cErrorMessageLen = AOS_CONFIG_TYPES_ERROR_MESSAGE_LEN;

/**
 * File chunk size.
 */
constexpr auto cFileChunkSize = AOS_CONFIG_TYPES_FILE_CHUNK_SIZE;

/*
 * Node name len.
 */
constexpr auto cNodeNameLen = AOS_CONFIG_TYPES_NODE_NAME_LEN;

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
 * CPU variant len.
 */
constexpr auto cCPUVariantLen = AOS_CONFIG_TYPES_CPU_VARIANT_LEN;

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
 * Environment variable value len.
 */
constexpr auto cEnvVarValueLen = AOS_CONFIG_TYPES_ENV_VAR_VALUE_LEN;

/**
 * Environment variable len.
 *
 * Consists of name and value plus equal sign.
 */
constexpr auto cEnvVarLen = cEnvVarNameLen + cEnvVarValueLen + 1;

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
static constexpr auto cMACLen = AOS_CONFIG_TYPES_MAC_LEN;

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
 * Max number of exposed ports.
 */
static constexpr auto cMaxNumExposedPorts = AOS_CONFIG_TYPES_MAX_NUM_EXPOSED_PORTS;

/**
 * Max exposed port len.
 */
static constexpr auto cExposedPortLen = cPortLen + cProtocolNameLen;

/**
 * Max length of connection name.
 */
static constexpr auto cConnectionNameLen = cIDLen + cExposedPortLen;

/**
 * Max number of allowed connections.
 */
static constexpr auto cMaxNumConnections = AOS_CONFIG_TYPES_MAX_NUM_ALLOWED_CONNECTIONS;

/**
 * Max number of update images per update item.
 */
constexpr auto cMaxNumUpdateImages = AOS_CONFIG_TYPES_MAX_NUM_UPDATE_IMAGES;

/*
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_TYPES_FILE_PATH_LEN;

/**
 * Max number of concurrent items.
 */
constexpr auto cMaxNumConcurrentItems = AOS_CONFIG_TYPES_MAX_CONCURRENT_ITEMS;

/**
 * State length.
 */
static constexpr auto cStateLen = AOS_CONFIG_TYPES_STATE_LEN;

/**
 * Max length of JSON.
 */
constexpr auto cJSONMaxLen = AOS_CONFIG_TYPES_JSON_MAX_LEN;

/**
 * Architecture info.
 */
struct ArchInfo {
    StaticString<cCPUArchLen>              mArchitecture;
    Optional<StaticString<cCPUVariantLen>> mVariant;

    /**
     * Compares architecture info.
     *
     * @param other architecture info to compare with.
     * @return bool.
     */
    bool operator==(const ArchInfo& other) const
    {
        return mArchitecture == other.mArchitecture && mVariant == other.mVariant;
    }

    /**
     * Compares architecture info.
     *
     * @param info architecture info to compare with.
     * @return bool.
     */
    bool operator!=(const ArchInfo& info) const { return !operator==(info); }
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
     * @param other OS info to compare with.
     * @return bool.
     */
    bool operator==(const OSInfo& other) const
    {
        return mOS == other.mOS && mVersion == other.mVersion && mFeatures == other.mFeatures;
    }

    /**
     * Compares OS info.
     *
     * @param other OS info to compare with.
     * @return bool.
     */
    bool operator!=(const OSInfo& other) const { return !operator==(other); }
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
     * @param other platform info to compare with.
     * @return bool.
     */
    bool operator==(const PlatformInfo& other) const
    {
        return mArchInfo == other.mArchInfo && mOSInfo == other.mOSInfo;
    }

    /**
     * Compares platform info.
     *
     * @param other platform info to compare with.
     * @return bool.
     */
    bool operator!=(const PlatformInfo& other) const { return !operator==(other); }
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
     * @param filter instance filter to compare with.
     * @return bool.
     */
    bool operator==(const InstanceFilter& filter) const
    {
        return mItemID == filter.mItemID && mSubjectID == filter.mSubjectID && mInstance == filter.mInstance;
    }

    /**
     * Compares instance filter.
     *
     * @param filter instance filter to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceFilter& filter) const { return !operator==(filter); }

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
    uint64_t                                                   mVlanID = 0;
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
 * Item status type.
 */
class ItemStatusType {
public:
    enum class Enum {
        eUnknown,
        ePending,
        eDownloading,
        eDownloaded,
        eInstalling,
        eInstalled,
        eRemoving,
        eRemoved,
        eError,
        eFailed,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "unknown",
            "pending",
            "downloading",
            "downloaded",
            "installing",
            "installed",
            "removing",
            "removed",
            "error",
            "failed",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using ItemStatusEnum = ItemStatusType::Enum;
using ItemStatus     = EnumStringer<ItemStatusType>;

/**
 * Image state type.
 */
class ImageStateType {
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

using ImageStateEnum = ImageStateType::Enum;
using ImageState     = EnumStringer<ImageStateType>;

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
 * Unit config state type.
 */
class UnitConfigStateType {
public:
    enum class Enum {
        eAbsent,
        eInstalled,
        eFailed,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "absent",
            "installed",
            "failed",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using UnitConfigStateEnum = UnitConfigStateType::Enum;
using UnitConfigState     = EnumStringer<UnitConfigStateType>;

/**
 * Instance info.
 */
struct InstanceInfo {
    InstanceIdent               mInstanceIdent;
    StaticString<cRuntimeIDLen> mRuntimeID;
    uid_t                       mUID {};
    uint64_t                    mPriority {};
    StaticString<cFilePathLen>  mStoragePath;
    StaticString<cFilePathLen>  mStatePath;
    NetworkParameters           mNetworkParameters;

    /**
     * Compares instance info.
     *
     * @param instance info to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfo& instance) const
    {
        return mInstanceIdent == instance.mInstanceIdent && mRuntimeID == instance.mRuntimeID && mUID == instance.mUID
            && mPriority == instance.mPriority && mStoragePath == instance.mStoragePath
            && mStatePath == instance.mStatePath && mNetworkParameters == instance.mNetworkParameters;
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
 * Instance status.
 */
struct InstanceStatus {
    InstanceIdent                     mInstanceIdent;
    StaticString<cVersionLen>         mVersion;
    StaticString<cNodeIDLen>          mNodeID;
    StaticString<cRuntimeIDLen>       mRuntimeID;
    StaticArray<uint8_t, cSHA256Size> mStateChecksum;
    InstanceState                     mState;
    Error                             mError;

    /**
     * Compares instance status.
     *
     * @param instance status to compare.
     * @return bool.
     */
    bool operator==(const InstanceStatus& instance) const
    {
        return mInstanceIdent == instance.mInstanceIdent && mVersion == instance.mVersion
            && mStateChecksum == instance.mStateChecksum && mState == instance.mState && mNodeID == instance.mNodeID
            && mRuntimeID == instance.mRuntimeID && mError == instance.mError;
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
 * Service status.
 */
struct ServiceStatus {
    /**
     * Default constructor.
     */
    ServiceStatus() = default;

    /**
     * Construct a new service status object
     *
     * @param serviceID service ID.
     * @param version service version.
     * @param status service status.
     * @param error service error.
     */
    ServiceStatus(const String& serviceID, const String& version, ItemStatus status = ItemStatusEnum::eUnknown,
        const Error& error = ErrorEnum::eNone)
        : mServiceID(serviceID)
        , mVersion(version)
        , mStatus(status)
        , mError(error)
    {
    }

    /**
     * Sets error with specified status.
     *
     * @param error error.
     * @param status status.
     */
    void SetError(const Error& error, ItemStatus status = ItemStatusEnum::eError)
    {
        mError  = error;
        mStatus = status;
    }

    StaticString<cIDLen>      mServiceID;
    StaticString<cVersionLen> mVersion;
    ItemStatus                mStatus;
    Error                     mError;

    /**
     * Compares service status.
     *
     * @param service status to compare.
     * @return bool.
     */
    bool operator==(const ServiceStatus& service) const
    {
        return mServiceID == service.mServiceID && mVersion == service.mVersion && mStatus == service.mStatus
            && mError == service.mError;
    }

    /**
     * Compares service status.
     *
     * @param service status to compare.
     * @return bool.
     */
    bool operator!=(const ServiceStatus& service) const { return !operator==(service); }
};

/**
 * Service status static array.
 */
using ServiceStatusStaticArray = StaticArray<ServiceStatus, cMaxNumServices>;

/**
 * Layer status.
 */
struct LayerStatus {
    /**
     * Default constructor.
     */
    LayerStatus() = default;

    /**
     * Construct a new layer status object
     *
     * @param layerID layer ID.
     * @param digest layer digest.
     * @param version layer version.
     * @param status layer status.
     * @param error layer error.
     */
    LayerStatus(const String& layerID, const String& digest, const String& version,
        ItemStatus status = ItemStatusEnum::eUnknown, const Error& error = ErrorEnum::eNone)
        : mLayerID(layerID)
        , mDigest(digest)
        , mVersion(version)
        , mStatus(status)
        , mError(error)
    {
    }

    /**
     * Sets error with specified status.
     *
     * @param error error.
     * @param status status.
     */
    void SetError(const Error& error, ItemStatus status = ItemStatusEnum::eError)
    {
        mError  = error;
        mStatus = status;
    }

    StaticString<cIDLen>          mLayerID;
    StaticString<cLayerDigestLen> mDigest;
    StaticString<cVersionLen>     mVersion;
    ItemStatus                    mStatus;
    Error                         mError;

    /**
     * Compares layer status.
     *
     * @param layer layer status to compare.
     * @return bool.
     */
    bool operator==(const LayerStatus& layer) const
    {
        return mLayerID == layer.mLayerID && mDigest == layer.mDigest && mVersion == layer.mVersion
            && mStatus == layer.mStatus && mError == layer.mError;
    }

    /**
     * Compares layer status.
     *
     * @param layer layer status to compare.
     * @return bool.
     */
    bool operator!=(const LayerStatus& layer) const { return !operator==(layer); }
};

/**
 * Layer status static array.
 */
using LayerStatusStaticArray = StaticArray<LayerStatus, cMaxNumLayers>;

/**
 * Service info.
 */
struct ServiceInfo {
    StaticString<cIDLen>              mServiceID;
    StaticString<cIDLen>              mProviderID;
    StaticString<cVersionLen>         mVersion;
    gid_t                             mGID;
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
    StaticString<cIDLen>              mLayerID;
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
    /**
     * Default constructor.
     */
    Host() = default;

    /**
     * Constructs host.
     *
     * @param ip IP.
     * @param hostname hostname.
     */
    Host(const String& ip, const String& hostname)
        : mHostname(hostname)
        , mIP(ip)

    {
    }

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

    StaticString<cHostNameLen> mHostname;
    StaticString<cIPLen>       mIP;
};

/**
 * Device info.
 */
struct DeviceInfo {
    StaticString<cDeviceNameLen>                                  mName;
    size_t                                                        mSharedCount {0};
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
 * Env vars static array.
 */
using EnvVarsStaticArray = StaticArray<StaticString<cEnvVarLen>, cMaxNumEnvVariables>;

/**
 * Labels static array.
 */
using LabelsStaticArray = StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels>;

/**
 * Resource info.
 */
struct ResourceInfoObsolete {
    StaticString<cResourceNameLen>                          mName;
    StaticArray<StaticString<cGroupNameLen>, cMaxNumGroups> mGroups;
    StaticArray<Mount, cMaxNumFSMounts>                     mMounts;
    EnvVarsStaticArray                                      mEnv;
    StaticArray<Host, cMaxNumHosts>                         mHosts;

    /**
     * Compares resource info.
     *
     * @param resourceInfo resource info to compare.
     * @return bool.
     */
    bool operator==(const ResourceInfoObsolete& resourceInfo) const
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
    bool operator!=(const ResourceInfoObsolete& resourceInfo) const { return !operator==(resourceInfo); }
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
    StaticString<cCPUModelNameLen> mModelName;
    size_t                         mNumCores   = 0;
    size_t                         mNumThreads = 0;
    ArchInfo                       mArchInfo;
    Optional<size_t>               mMaxDMIPS;

    /**
     * Compares CPU info.
     *
     * @param info cpu info to compare with.
     * @return bool.
     */
    bool operator==(const CPUInfo& info) const
    {
        return mModelName == info.mModelName && mNumCores == info.mNumCores && mNumThreads == info.mNumThreads
            && mArchInfo == info.mArchInfo && mMaxDMIPS == info.mMaxDMIPS;
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
        static const char* const sStrings[] = {
            "MainNode",
            "AosComponents",
            "NodeRunners",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
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
        static const char* const sStrings[] = {
            "runc",
            "crun",
            "xrun",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
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
 * Node state.
 */
class NodeStateObsoleteType {
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

using NodeStateObsoleteEnum = NodeStateObsoleteType::Enum;
using NodeStateObsolete     = EnumStringer<NodeStateObsoleteType>;

/**
 * Node state.
 */
class NodeStateType {
public:
    enum class Enum {
        eOffline,
        eOnline,
        eError,
        ePaused,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "offline",
            "online",
            "error",
            "paused",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using NodeStateEnum = NodeStateType::Enum;
using NodeState     = EnumStringer<NodeStateType>;

/**
 * Node info.
 */
struct NodeInfoObsolete {
    StaticString<cNodeIDLen>   mNodeID;
    StaticString<cNodeTypeLen> mNodeType;
    StaticString<cNodeNameLen> mName;
    NodeStateObsolete          mState;
    StaticString<cOSTypeLen>   mOSType;
    CPUInfoStaticArray         mCPUs;
    PartitionInfoStaticArray   mPartitions;
    NodeAttributeStaticArray   mAttrs;
    uint64_t                   mMaxDMIPS = 0;
    uint64_t                   mTotalRAM = 0;

    Error GetRunners(Array<StaticString<cRunnerNameLen>>& runners) const
    {
        auto attr = mAttrs.FindIf([](const NodeAttribute& attr) {
            return attr.mName == NodeAttributeName(NodeAttributeEnum::eNodeRunners).ToString();
        });

        if (attr == mAttrs.end()) {
            return ErrorEnum::eNotFound;
        }

        if (auto err = attr->mValue.Split(runners, ','); !err.IsNone()) {
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
    bool operator==(const NodeInfoObsolete& info) const
    {
        return mNodeID == info.mNodeID && mNodeType == info.mNodeType && mName == info.mName && mState == info.mState
            && mOSType == info.mOSType && mCPUs == info.mCPUs && mMaxDMIPS == info.mMaxDMIPS
            && mTotalRAM == info.mTotalRAM && mPartitions == info.mPartitions && mAttrs == info.mAttrs;
    }

    /**
     * Compares node info.
     *
     * @param info node info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeInfoObsolete& info) const { return !operator==(info); }
};

/**
 * Node info.
 */
struct NodeInfo {
    StaticString<cNodeIDLen>   mNodeID;
    StaticString<cNodeTypeLen> mNodeType;
    StaticString<cNodeNameLen> mName;
    size_t                     mMaxDMIPS {};
    size_t                     mTotalRAM {};
    Optional<size_t>           mPhysicalRAM;
    OSInfo                     mOSInfo;
    CPUInfoStaticArray         mCPUs;
    PartitionInfoStaticArray   mPartitions;
    NodeAttributeStaticArray   mAttrs;
    bool                       mProvisioned = false;
    NodeState                  mState;
    Error                      mError;

    /**
     * Compares node info.
     *
     * @param info node info to compare with.
     * @return bool.
     */
    bool operator==(const NodeInfo& info) const
    {
        return mNodeID == info.mNodeID && mNodeType == info.mNodeType && mName == info.mName
            && mMaxDMIPS == info.mMaxDMIPS && mTotalRAM == info.mTotalRAM && mPhysicalRAM == info.mPhysicalRAM
            && mOSInfo == info.mOSInfo && mCPUs == info.mCPUs && mPartitions == info.mPartitions
            && mAttrs == info.mAttrs && mProvisioned == info.mProvisioned && mState == info.mState
            && mError == info.mError;
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
    Optional<Duration> mStartInterval;
    Optional<Duration> mRestartInterval;
    Optional<long>     mStartBurst;

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

/**
 * Layer state type.
 */
class LayerStateType {
public:
    enum class Enum {
        eActive,
        eCached,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "active",
            "cached",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using LayerStateEnum = LayerStateType::Enum;
using LayerState     = EnumStringer<LayerStateType>;

/**
 * Service state type.
 */
class ServiceStateType {
public:
    enum class Enum {
        eActive,
        eCached,
        ePending,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "active",
            "cached",
            "pending",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using ServiceStateEnum = ServiceStateType::Enum;
using ServiceState     = EnumStringer<ServiceStateType>;

/**
 * Image info.
 */
struct ImageInfo : public PlatformInfo {
    StaticString<cIDLen> mImageID;

    /**
     * Compares image info.
     *
     * @param other image info to compare with.
     * @return bool.
     */
    bool operator==(const ImageInfo& other) const
    {
        return mImageID == other.mImageID
            && static_cast<const PlatformInfo&>(*this) == static_cast<const PlatformInfo&>(other);
    }

    /**
     * Compares image info.
     *
     * @param other image info to compare with.
     * @return bool.
     */
    bool operator!=(const ImageInfo& other) const { return !operator==(other); }
};

/**
 * Cert type type.
 */
class CertTypeType {
public:
    enum class Enum {
        eOffline,
        eOnline,
        eUM,
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
            "um",
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
 * Resource info.
 */
struct ResourceInfo {
    StaticString<cResourceNameLen> mName;
    size_t                         mSharedCount {0};

    /**
     * Compares resource info.
     *
     * @param other resource info to compare with.
     * @return bool.
     */
    bool operator==(const ResourceInfo& other) const
    {
        return mName == other.mName && mSharedCount == other.mSharedCount;
    }

    /**
     * Compares resource info.
     *
     * @param other resource info to compare with.
     * @return bool.
     */
    bool operator!=(const ResourceInfo& other) const { return !operator==(other); }
};

using ResourceInfoStaticArray = StaticArray<ResourceInfo, cMaxNumNodeResources>;

/**
 * Runtime info.
 */
struct RuntimeInfo : public PlatformInfo {
    StaticString<cRuntimeIDLen>   mRuntimeID;
    StaticString<cRuntimeTypeLen> mRuntimeType;
    Optional<size_t>              mMaxDMIPS;
    Optional<size_t>              mAllowedDMIPS;
    Optional<size_t>              mTotalRAM;
    Optional<ssize_t>             mAllowedRAM;
    size_t                        mMaxInstances = 0;

    /**
     * Compares runtime info.
     *
     * @param other runtime info to compare with.
     * @return bool.
     */
    bool operator==(const RuntimeInfo& other) const
    {
        return mRuntimeID == other.mRuntimeID && mRuntimeType == other.mRuntimeType && mMaxDMIPS == other.mMaxDMIPS
            && mAllowedDMIPS == other.mAllowedDMIPS && mTotalRAM == other.mTotalRAM && mAllowedRAM == other.mAllowedRAM
            && mMaxInstances == other.mMaxInstances && PlatformInfo::operator==(other);
    }

    /**
     * Compares runtime info.
     *
     * @param other runtime info to compare with.
     * @return bool.
     */
    bool operator!=(const RuntimeInfo& other) const { return !operator==(other); }
};

using RuntimeInfoStaticArray = StaticArray<RuntimeInfo, cMaxNumNodeRuntimes>;

/**
 * State result type.
 */
class StateResultType {
public:
    enum class Enum {
        eAccepted,
        eRejected,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "accepted",
            "rejected",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using StateResultEnum = StateResultType::Enum;
using StateResult     = EnumStringer<StateResultType>;

} // namespace aos

#endif
