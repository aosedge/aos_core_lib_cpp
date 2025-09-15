/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_SMCONTROLLER_ITF_UPDATEIMAGEPROVIDER_HPP_
#define AOS_CORE_CM_SMCONTROLLER_ITF_UPDATEIMAGEPROVIDER_HPP_

#include <core/common/tools/error.hpp>
#include <core/common/types/types.hpp>

namespace aos::cm::smcontroller {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Update image info.
 */
struct UpdateImageInfo {
    StaticString<cIDLen>              mImageID;
    StaticString<cVersionLen>         mVersion;
    StaticString<cURLLen>             mURL;
    StaticArray<uint8_t, cSHA256Size> mSHA256;
    size_t                            mSize {};

    /**
     * Compares update image info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const UpdateImageInfo& other) const
    {
        return mImageID == other.mImageID && mVersion == other.mVersion && mURL == other.mURL
            && mSHA256 == other.mSHA256 && mSize == other.mSize;
    }

    /**
     * Compares update image info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateImageInfo& other) const { return !operator==(other); }
};

/**
 * Provides update item image info.
 */
class UpdateImageProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~UpdateImageProviderItf() = default;

    /**
     * Returns update image info for desired platform.
     *
     * @param id update item ID.
     * @param platform platform information.
     * @param[out] info update image info.
     * @return Error.
     */
    virtual Error GetUpdateImageInfo(const String& id, const PlatformInfo& platform, UpdateImageInfo& info) = 0;

    /**
     * Returns layer image info.
     *
     * @param digest layer digest.
     * @param info layer image info.
     * @return Error.
     */
    virtual Error GetLayerImageInfo(const String& digest, UpdateImageInfo& info) = 0;
};

} // namespace aos::cm::smcontroller

#endif
