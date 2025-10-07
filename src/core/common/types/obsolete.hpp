/**
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_OBSOLETE_HPP_
#define AOS_CORE_COMMON_TYPES_OBSOLETE_HPP_

#include <cstdint>

#include <core/common/config.hpp>
#include <core/common/crypto/itf/hash.hpp>
#include <core/common/tools/enum.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/log.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/time.hpp>
#include <core/common/tools/uuid.hpp>

#include "envvars.hpp"
#include "network.hpp"

namespace aos {

/**
 * Layer digest len.
 */
constexpr auto cLayerDigestLen = AOS_CONFIG_TYPES_LAYER_DIGEST_LEN;

/**
 * Max number of services.
 */
constexpr auto cMaxNumServices = AOS_CONFIG_TYPES_MAX_NUM_SERVICES;

/**
 * Max number of layers.
 */
constexpr auto cMaxNumLayers = AOS_CONFIG_TYPES_MAX_NUM_LAYERS;

/**
 * Device name len.
 */
constexpr auto cDeviceNameLen = AOS_CONFIG_TYPES_DEVICE_NAME_LEN;

/**
 * Max number of node's devices.
 */
constexpr auto cMaxNumNodeDevices = AOS_CONFIG_TYPES_MAX_NUM_NODE_DEVICES;

/**
 *  Max num runners.
 */
static constexpr auto cMaxNumRunners = AOS_CONFIG_TYPES_MAX_NUM_RUNNERS;

/**
 * Runner name max length.
 */
static constexpr auto cRunnerNameLen = AOS_CONFIG_TYPES_RUNNER_NAME_LEN;

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
 * Service status array.
 */
using ServiceStatusArray = StaticArray<ServiceStatus, cMaxNumServices>;

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
 * Layer status array.
 */
using LayerStatusArray = StaticArray<LayerStatus, cMaxNumLayers>;

/**
 * Service info.
 */
struct ServiceInfo {
    StaticString<cIDLen>                      mServiceID;
    StaticString<cIDLen>                      mProviderID;
    StaticString<cVersionLen>                 mVersion;
    gid_t                                     mGID;
    StaticString<cURLLen>                     mURL;
    StaticArray<uint8_t, crypto::cSHA256Size> mSHA256;
    size_t                                    mSize;

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
 * Service info array.
 */
using ServiceInfoArray = StaticArray<ServiceInfo, cMaxNumServices>;

/**
 * Layer info.
 */

// LayerInfo layer info.
struct LayerInfo {
    StaticString<cIDLen>                      mLayerID;
    StaticString<cLayerDigestLen>             mLayerDigest;
    StaticString<cVersionLen>                 mVersion;
    StaticString<cURLLen>                     mURL;
    StaticArray<uint8_t, crypto::cSHA256Size> mSHA256;
    size_t                                    mSize;

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
 * Layer info array.
 */
using LayerInfoArray = StaticArray<LayerInfo, cMaxNumLayers>;

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
 * Resource info.
 */
struct ResourceInfoObsolete {
    StaticString<cResourceNameLen>                          mName;
    StaticArray<StaticString<cGroupNameLen>, cMaxNumGroups> mGroups;
    StaticArray<Mount, cMaxNumFSMounts>                     mMounts;
    EnvVarArray                                             mEnv;
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
struct PartitionInfoObsolete {
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
    bool operator==(const PartitionInfoObsolete& info) const
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
    bool operator!=(const PartitionInfoObsolete& info) const { return !operator==(info); }
};

/**
 * Partition info  array.
 */
using PartitionInfoObsoleteArray = StaticArray<PartitionInfoObsolete, cMaxNumPartitions>;

/**
 * Node attribute enum.
 */
class NodeAttributeObsoleteType {
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

using NodeAttributeObsoleteEnum = NodeAttributeObsoleteType::Enum;
using NodeAttributeObsoleteName = EnumStringer<NodeAttributeObsoleteType>;

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
 * Node info.
 */
struct NodeInfoObsolete {
    StaticString<cIDLen>        mNodeID;
    StaticString<cNodeTypeLen>  mNodeType;
    StaticString<cNodeTitleLen> mName;
    NodeStateObsolete           mState;
    StaticString<cOSTypeLen>    mOSType;
    CPUInfoArray                mCPUs;
    PartitionInfoObsoleteArray  mPartitions;
    NodeAttributeArray          mAttrs;
    uint64_t                    mMaxDMIPS = 0;
    uint64_t                    mTotalRAM = 0;

    Error GetRunners(Array<StaticString<cRunnerNameLen>>& runners) const
    {
        auto attr = mAttrs.FindIf([](const NodeAttribute& attr) {
            return attr.mName == NodeAttributeObsoleteName(NodeAttributeObsoleteEnum::eNodeRunners).ToString();
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

} // namespace aos

#endif
