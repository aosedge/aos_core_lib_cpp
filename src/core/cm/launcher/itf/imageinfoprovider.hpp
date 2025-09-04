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
 * Interface that provides update items images information.
 */
class ImageInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageInfoProviderItf() = default;

    /**
     * Returns update item version.
     *
     * @param id update item ID.
     * @return RetWithError<StaticString<cVersionLen>>.
     */
    virtual RetWithError<StaticString<cVersionLen>> GetItemVersion(const uuid::UUID& id) = 0;

    /**
     * Returns update item images infos.
     *
     * @param id update item ID.
     * @param[out] imagesInfos update item images info.
     * @return Error.
     */
    virtual Error GetItemImages(const uuid::UUID& id, Array<ImageInfo>& imagesInfos) = 0;

    /**
     * Returns service config.
     *
     * @param id update item ID.
     * @param imageID image ID.
     * @param config service config.
     * @return Error.
     */
    virtual Error GetServiceConfig(const uuid::UUID& id, const uuid::UUID& imageID, oci::ServiceConfig& config) = 0;

    /**
     * Returns image config.
     *
     * @param id update item ID.
     * @param imageID image ID.
     * @param config image config.
     * @return Error.
     */
    virtual Error GetImageConfig(const uuid::UUID& id, const uuid::UUID& imageID, oci::ImageConfig& config) = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
