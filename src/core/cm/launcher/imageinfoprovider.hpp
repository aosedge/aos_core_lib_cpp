/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_IMAGEINFOPROVIDER_HPP_
#define AOS_CORE_CM_LAUNCHER_IMAGEINFOPROVIDER_HPP_

#include <core/cm/imagemanager/itf/iteminfoprovider.hpp>
#include <core/common/blobinfoprovider/itf/blobinfoprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
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
     * @param itemInfoProvider item info provider.
     * @param blobInfoProvider blob info provider.
     * @param ociSpec OCI spec.
     */
    void Init(imagemanager::ItemInfoProviderItf& itemInfoProvider, blobinfoprovider::ProviderItf& blobInfoProvider,
        oci::OCISpecItf& ociSpec);

    /**
     * Returns OCI image config for the specified instance/image identifiers.
     *
     * @param imageDescriptor image descriptor.
     * @param[out] config resolved OCI image configuration.
     * @return Error.
     */
    Error GetImageConfig(const oci::IndexContentDescriptor& imageDescriptor, oci::ImageConfig& config);

    /**
     * Returns Aos service config for the specified descriptor.
     *
     * @param imageDescriptor image descriptor.
     * @param[out] serviceConfig resolved service config.
     * @return Error.
     */
    Error GetServiceConfig(const oci::IndexContentDescriptor& imageDescriptor, oci::ServiceConfig& serviceConfig);

    /**
     * Returns OCI image index for the specified instance/version.
     *
     * @param itemID item identifier.
     * @param version item version.
     * @param[out] imageIndex resolved image index.
     * @return Error.
     */
    Error GetImageIndex(const String& itemID, const String& version, oci::ImageIndex& imageIndex);

private:
    static constexpr auto cAllocatorSize = sizeof(oci::ImageIndex) + sizeof(oci::ImageManifest)
        + sizeof(StaticArray<BlobInfo, cMaxNumBlobs>) + sizeof(StaticString<cFilePathLen>) * 3
        + sizeof(StaticString<oci::cDigestLen>);

    imagemanager::ItemInfoProviderItf* mItemInfoProvider {};
    blobinfoprovider::ProviderItf*     mBlobInfoProvider {};
    oci::OCISpecItf*                   mOCISpec {};

    StaticAllocator<cAllocatorSize> mAllocator;
};

} // namespace aos::cm::launcher

#endif
