/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_UPDATEITEMPROVIDER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_UPDATEITEMPROVIDER_HPP_

#include <core/cm/networkmanager/itf/networkmanager.hpp>
#include <core/common/ocispec/ocispec.hpp>
#include <core/common/tools/error.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Update image info.
 */
struct UpdateImageInfo : public ImageInfo {
    StaticString<cVersionLen> mVersion;
    StaticString<cURLLen>     mURL;
    StaticString<cSHA256Size> mSHA256;
    size_t                    mSize {};

    /**
     * Compares update image info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const UpdateImageInfo& other) const
    {
        return mVersion == other.mVersion && mURL == other.mURL && mSHA256 == other.mSHA256 && mSize == other.mSize;
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
 * Interface that provides update items images information.
 */
class ImageInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageInfoProviderItf() = default;

    /**
     * Returns update item images infos.
     *
     * @param urn update item URN.
     * @param[out] imagesInfos update item images info.
     * @return Error.
     */
    virtual Error GetItemImages(const String& urn, Array<UpdateImageInfo>& imagesInfos) = 0;

    /**
     * Returns service config.
     *
     * @param urn update item URN.
     * @param imageID image ID.
     * @param config service config.
     * @return Error.
     */
    virtual Error GetServiceConfig(const String& urn, const uuid::UUID& imageID, oci::ServiceConfig& config) = 0;

    /**
     * Returns image config.
     *
     * @param urn update item URN.
     * @param imageID image ID.
     * @param config image config.
     * @return Error.
     */
    virtual Error GetImageConfig(const String& urn, const uuid::UUID& imageID, oci::ImageConfig& config) = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
