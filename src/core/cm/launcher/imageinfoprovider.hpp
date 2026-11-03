/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_IMAGEINFOPROVIDER_HPP_
#define AOS_CORE_CM_LAUNCHER_IMAGEINFOPROVIDER_HPP_

#include <core/common/imagemanager/itf/iteminfoprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/types/common.hpp>
#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::launcher {

/**
 * Helper that resolves OCI image metadata (config/service/etc.) for launcher.
 */
class ImageInfoProvider {
public:
    /**
     * @brief Initializes image info provider.
     *
     * @param allocator allocator to use for temporary objects.
     * @param itemInfoProvider item info provider.
     * @param ociSpec OCI spec.
     */
    void Init(
        AllocatorItf& allocator, common::imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec);

    /**
     * Returns OCI image config for the specified instance/image identifiers.
     *
     * @param imageDescriptor image descriptor.
     * @param[out] config resolved OCI image configuration.
     * @return Error.
     */
    Error GetImageConfig(const oci::IndexContentDescriptor& imageDescriptor, oci::ImageConfig& config);

    /**
     * Returns Aos item config for the specified descriptor.
     *
     * @param imageDescriptor image descriptor.
     * @param[out] itemConfig resolved item config.
     * @return Error.
     */
    Error GetItemConfig(const oci::IndexContentDescriptor& imageDescriptor, oci::ItemConfig& itemConfig);

    /**
     * Returns OCI image index for the specified index digest.
     *
     * @param indexDigest image index digest.
     * @param[out] imageIndex resolved image index.
     * @return Error.
     */
    Error GetImageIndex(const String& indexDigest, oci::ImageIndex& imageIndex);

private:
    common::imagemanager::ItemInfoProviderItf* mItemInfoProvider {};
    oci::OCISpecItf*                           mOCISpec {};

    AllocatorItf* mAllocator {};
};

} // namespace aos::cm::launcher

#endif
