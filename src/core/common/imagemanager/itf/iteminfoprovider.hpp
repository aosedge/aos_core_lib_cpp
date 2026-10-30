/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_ITF_ITEMINFOPROVIDER_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_ITF_ITEMINFOPROVIDER_HPP_

#include <core/common/tools/error.hpp>
#include <core/common/tools/string.hpp>

namespace aos::common::imagemanager {

/**
 * Interface that provides local blob and unpacked layer info.
 */
class ItemInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ItemInfoProviderItf() = default;

    /**
     * Returns local blob path.
     *
     * @param digest blob digest.
     * @param[out] path local blob path.
     * @return Error.
     */
    virtual Error GetBlobPath(const String& digest, String& path) const = 0;

    /**
     * Returns unpacked layer path.
     *
     * @param diffID layer diff ID.
     * @param[out] path unpacked layer path.
     * @return Error: eNotSupported if the image manager doesn't keep the unpacked layers.
     */
    virtual Error GetLayerPath(const String& diffID, String& path) const = 0;
};

} // namespace aos::common::imagemanager

#endif
