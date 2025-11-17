/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEMANAGER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEMANAGER_HPP_

#include <core/common/crypto/itf/cryptohelper.hpp>
#include <core/common/types/desiredstatus.hpp>

#include "iteminfoprovider.hpp"
#include "itemstatusprovider.hpp"

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Interface that manages update items images.
 */
class ImageManagerItf : public ItemStatusProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageManagerItf() = default;

    /**
     * Downloads update items.
     *
     * @param itemsInfo update items info.
     * @param certificates list of certificates.
     * @param certificateChains list of certificate chains.
     * @param[out] statuses update items statuses.
     * @return Error.
     */
    virtual Error DownloadUpdateItems(const Array<UpdateItemInfo>& itemsInfo,
        const Array<crypto::CertificateInfo>&                      certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, Array<UpdateItemStatus>& statuses)
        = 0;

    /**
     * Activate update items.
     *
     * @param ids update items IDs.
     * @param[out] statuses update items statuses.
     * @return Error.
     */
    virtual Error InstallUpdateItems(const Array<UpdateItemInfo>& itemsInfo, Array<UpdateItemStatus>& statuses) = 0;

    /**
     * Cancels ongoing operations: download or install.
     *
     * @return Error.
     */
    virtual Error Cancel() = 0;
};

/** @}*/

} // namespace aos::cm::imagemanager

#endif
