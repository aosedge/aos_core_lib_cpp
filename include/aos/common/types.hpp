/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TYPES_HPP_
#define AOS_TYPES_HPP_

#include <cstdint>

#include "aos/common/config/types.hpp"
#include "aos/common/tools/enum.hpp"
#include "aos/common/tools/error.hpp"

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
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_TYPES_FILE_PATH_LEN;

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
};

/**
 * Instance run state.
 */
class InstanceRunStateType {
public:
    enum class Enum { eActive, eFailed, eNumStates };

    static const Array<const char* const> GetStrings()
    {
        static const char* const cInstanceRunStateStrings[] = {"active", "failed"};

        return Array<const char* const>(cInstanceRunStateStrings, ArraySize(cInstanceRunStateStrings));
    };
};

using InstanceRunStateEnum = InstanceRunStateType::Enum;
using InstanceRunState = EnumStringer<InstanceRunStateType>;

/**
 * Instance status.
 */
struct InstanceStatus {
    InstanceIdent    mInstanceIdent;
    uint64_t         mAosVersion;
    InstanceRunState mRunState;
    Error            mError;
};

/**
 * Version info.
 */
struct VersionInfo {
    uint64_t                        mAosVersion;
    StaticString<cVendorVersionLen> mVendorVersion;
    StaticString<cDescriptionLen>   mDescription;
};

/**
 * Service info.
 */

struct ServiceInfo {
    VersionInfo                  mVersionInfo;
    StaticString<cServiceIDLen>  mServiceID;
    StaticString<cProviderIDLen> mProviderID;
    uint32_t                     mGID;
    StaticString<cURLLen>        mURL;
    uint8_t                      mSHA256[cSHA256Size];
    uint8_t                      mSHA512[cSHA512Size];
    size_t                       mSize;
};

/**
 * Layer info.
 */

// LayerInfo layer info.
struct LayerInfo {
    VersionInfo                   mVersionInfo;
    StaticString<cLayerIDLen>     mLayerID;
    StaticString<cLayerDigestLen> mLayerDigest;
    StaticString<cURLLen>         mURL;
    uint8_t                       mSHA256[cSHA256Size];
    uint8_t                       mSHA512[cSHA512Size];
    size_t                        mSize;
};

} // namespace aos

#endif
