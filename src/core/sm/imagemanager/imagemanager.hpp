/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_IMAGEMANAGER_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_IMAGEMANAGER_HPP_

#include "config.hpp"
#include "itf/imagemanager.hpp"
#include "itf/iteminfoprovider.hpp"

namespace aos::sm::imagemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Image manager.
 */
class ImageManager : public ImageManagerItf, public ItemInfoProviderItf {
public:
    /**
     * Initializes image manager.
     *
     * @param config image manager config.
     * @return Error.
     */
    Error Init(const Config& config);

    /**
     * Returns all installed update items statuses.
     *
     * @param statuses list of installed update item statuses.
     * @return Error.
     */
    Error GetAllInstalledItems(Array<UpdateItemStatus>& statuses) const override;

    /**
     * Installs update item.
     *
     * @param itemInfo update item info.
     * @return Error.
     */
    Error InstallUpdateItem(const UpdateItemInfo& itemInfo) override;

    /**
     * Removes update item.
     *
     * @param itemID update item ID.
     * @param version update item version.
     * @return Error.
     */
    Error RemoveUpdateItem(const String& itemID, const String& version) override;

    /**
     * Returns blob path by its digest.
     *
     * @param digest blob digest.
     * @param[out] path result blob path.
     * @return Error.
     */
    Error GetBlobPath(const String& digest, String& path) const override;

    /**
     * Returns layer path by its digest.
     *
     * @param digest layer digest.
     * @param[out] path result layer path.
     * @return Error.
     */
    Error GetLayerPath(const String& digest, String& path) const override;

private:
    static constexpr auto cBlobsFolder  = "blobs";
    static constexpr auto cLayersFolder = "layers";

    Config mConfig;
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
