/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITEMCONFIGLOADER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITEMCONFIGLOADER_HPP_

#include <core/common/imagemanager/itf/iteminfoprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Loads the configs of the update item from its stored blobs.
 */
class ItemConfigLoader {
public:
    /**
     * Initializes item config loader.
     *
     * @param allocator allocator to use for temporary objects.
     * @param itemInfoProvider item info provider.
     * @param ociSpec OCI spec.
     */
    void Init(
        AllocatorItf& allocator, common::imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec);

    /**
     * Loads the item config and the image config referenced by the image manifest.
     *
     * @param manifestDigest image manifest digest.
     * @param[out] itemConfig item config.
     * @param[out] imageConfig image config.
     * @return Error.
     */
    Error Load(const String& manifestDigest, oci::ItemConfig& itemConfig, oci::ImageConfig& imageConfig);

private:
    AllocatorItf*                              mAllocator {};
    common::imagemanager::ItemInfoProviderItf* mItemInfoProvider {};
    oci::OCISpecItf*                           mOCISpec {};
};

/** @}*/

} // namespace aos::sm::launcher

#endif
