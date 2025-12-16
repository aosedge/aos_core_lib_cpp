/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_ITF_ITEMINFOPROVIDER_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_ITF_ITEMINFOPROVIDER_HPP_

#include <core/common/types/unitstatus.hpp>

namespace aos::sm::imagemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Interface to provide update item info.
 */
class ItemInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ItemInfoProviderItf() = default;

    /**
     * Returns blob path by its digest.
     *
     * @param digest blob digest.
     * @param[out] path result blob path.
     * @return Error.
     */
    virtual Error GetBlobPath(const String& digest, String& path) const = 0;

    /**
     * Returns layer path by its digest.
     *
     * @param digest layer digest.
     * @param[out] path result layer path.
     * @return Error.
     */
    virtual Error GetLayerPath(const String& digest, String& path) const = 0;
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
