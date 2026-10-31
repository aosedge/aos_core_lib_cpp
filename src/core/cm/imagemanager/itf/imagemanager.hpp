/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEMANAGER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEMANAGER_HPP_

#include <core/common/crypto/itf/cryptohelper.hpp>
#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Interface that manages update items images.
 */
class ImageManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageManagerItf() = default;

    /**
     * Sets certificates and certificate chains used to verify blob signatures.
     *
     * @param certificates list of certificates.
     * @param certificateChains list of certificate chains.
     * @return Error.
     */
    virtual Error SetCerts(const Array<crypto::CertificateInfo>& certificates,
        const Array<crypto::CertificateChainInfo>&               certificateChains)
        = 0;

    /**
     * Installs update item.
     *
     * @param itemInfo update item info.
     * @return Error.
     */
    virtual Error InstallItem(const UpdateItemInfo& itemInfo) = 0;

    /**
     * Removes update item.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error.
     */
    virtual Error RemoveItem(const String& itemID, const String& version) = 0;

    /**
     * Cancels all items being installed. Waits until all InstallItem and RemoveItem calls in progress return.
     *
     * @return Error.
     */
    virtual Error Cancel() = 0;
};

/** @}*/

} // namespace aos::cm::imagemanager

#endif
