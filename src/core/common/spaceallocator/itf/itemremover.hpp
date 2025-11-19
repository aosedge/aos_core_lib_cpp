/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_SPACEALLOCATOR_ITF_ITEMREMOVER_HPP_
#define AOS_CORE_COMMON_SPACEALLOCATOR_ITF_ITEMREMOVER_HPP_

#include <core/common/tools/string.hpp>

namespace aos::spaceallocator {

/**
 * Item remover interface.
 */
class ItemRemoverItf {
public:
    /**
     * Removes item.
     *
     * @param id item id.
     * @return Error.
     */
    virtual Error RemoveItem(const String& id) = 0;

    /**
     * Destructor.
     */
    virtual ~ItemRemoverItf() = default;
};

} // namespace aos::spaceallocator

#endif
