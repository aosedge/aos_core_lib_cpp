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

namespace aos {

/*
 * Provider ID len.
 */
constexpr auto cProviderIDLen = AOS_CONFIG_TYPES_PROVIDER_ID_LEN;

/*
 * Service ID len.
 */
constexpr auto cServiceIDLen = AOS_CONFIG_TYPES_SERVICE_ID_LEN;

/*
 * Subject ID len.
 */
constexpr auto cSubjectIDLen = AOS_CONFIG_TYPES_SUBJECT_ID_LEN;

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
 * URL len.
 */
constexpr auto cURLLen = AOS_CONFIG_TYPES_URL_LEN;

/**
 * Vendor version len.
 */
constexpr auto cVendorVersionLen = AOS_CONFIG_TYPES_VENDOR_VERSION_LEN;

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
 * Node ID len.
 */
constexpr auto cNodeIDLen = AOS_CONFIG_TYPES_NODE_ID_LEN;

/**
 * Node type len.
 */
constexpr auto cNodeTypeLen = AOS_CONFIG_TYPES_NODE_ID_LEN;

/**
 * SHA256 size.
 */
constexpr auto cSHA256Size = 32;

/**
 * SHA512 size.
 */
constexpr auto cSHA512Size = 64;

/**
 * Error message len.
 */
constexpr auto cErrorMessageLen = AOS_CONFIG_TYPES_ERROR_MESSAGE_LEN;

/**
 * File chunk size.
 */
constexpr auto cFileChunkSize = AOS_CONFIG_TYPES_FILE_CHUNK_SIZE;

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
 * Instance info.
 */
struct InstanceInfo {
    InstanceIdent              mInstanceIdent;
    uint32_t                   mUID;
    uint64_t                   mPriority;
    StaticString<cFilePathLen> mStoragePath;
    StaticString<cFilePathLen> mStatePath;

    /**
     * Compares instance info.
     *
     * @param instance info to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfo& instance) const
    {
        return mInstanceIdent == instance.mInstanceIdent && mUID == instance.mUID && mPriority == instance.mPriority
            && mStoragePath == instance.mStoragePath && mStatePath == instance.mStatePath;
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
    enum class Enum { eActive, eFailed, eNumStates };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sInstanceRunStateStrings[] = {"active", "failed"};

        return Array<const char* const>(sInstanceRunStateStrings, ArraySize(sInstanceRunStateStrings));
    };
};

using InstanceRunStateEnum = InstanceRunStateType::Enum;
using InstanceRunState     = EnumStringer<InstanceRunStateType>;

/**
 * Instance status.
 */
struct InstanceStatus {
    InstanceIdent    mInstanceIdent;
    uint64_t         mAosVersion;
    InstanceRunState mRunState;
    Error            mError;

    /**
     * Compares instance status.
     *
     * @param instance status to compare.
     * @return bool.
     */
    bool operator==(const InstanceStatus& instance) const
    {
        return mInstanceIdent == instance.mInstanceIdent && mAosVersion == instance.mAosVersion
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
 * Version info.
 */
struct VersionInfo {
    uint64_t                        mAosVersion;
    StaticString<cVendorVersionLen> mVendorVersion;
    StaticString<cDescriptionLen>   mDescription;

    /**
     * Compares version info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator==(const VersionInfo& info) const
    {
        return mAosVersion == info.mAosVersion && mVendorVersion == info.mVendorVersion;
    }

    /**
     * Compares version info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator!=(const VersionInfo& info) const { return !operator==(info); }
};

/**
 * Service info.
 */

struct ServiceInfo {
    VersionInfo                            mVersionInfo;
    StaticString<cServiceIDLen>            mServiceID;
    StaticString<cProviderIDLen>           mProviderID;
    uint32_t                               mGID;
    StaticString<cURLLen>                  mURL;
    aos::StaticArray<uint8_t, cSHA256Size> mSHA256;
    aos::StaticArray<uint8_t, cSHA512Size> mSHA512;
    size_t                                 mSize;

    /**
     * Compares service info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator==(const ServiceInfo& info) const
    {
        return mVersionInfo == info.mVersionInfo && mServiceID == info.mServiceID && mProviderID == info.mProviderID
            && mGID == info.mGID && mURL == info.mURL && mSHA256 == info.mSHA256 && mSHA512 == info.mSHA512
            && mSize == info.mSize;
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
    VersionInfo                            mVersionInfo;
    StaticString<cLayerIDLen>              mLayerID;
    StaticString<cLayerDigestLen>          mLayerDigest;
    StaticString<cURLLen>                  mURL;
    aos::StaticArray<uint8_t, cSHA256Size> mSHA256;
    aos::StaticArray<uint8_t, cSHA512Size> mSHA512;
    size_t                                 mSize;

    /**
     * Compares layer info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator==(const LayerInfo& info) const
    {
        return mVersionInfo == info.mVersionInfo && mLayerID == info.mLayerID && mLayerDigest == info.mLayerDigest
            && mURL == info.mURL && mSHA256 == info.mSHA256 && mSHA512 == info.mSHA512 && mSize == info.mSize;
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

} // namespace aos

#endif
