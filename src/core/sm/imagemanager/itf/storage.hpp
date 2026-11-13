/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_ITF_STORAGE_HPP_

#include <core/common/imagemanager/itf/storage.hpp>

namespace aos::sm::imagemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * SM image manager storage interface.
 */
class StorageItf : public virtual common::imagemanager::StorageItf {
public:
    /**
     * Checks if layer is referenced by any existing deployable item, regardless of the item state. Blob references
     * left without an item are ignored.
     *
     * @param diffID layer diff ID.
     * @return RetWithError<bool>.
     */
    virtual RetWithError<bool> IsLayerUsed(const String& diffID) = 0;
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
