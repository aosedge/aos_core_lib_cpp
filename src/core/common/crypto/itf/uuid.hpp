/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_UUID_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_UUID_HPP_

#include <core/common/tools/uuid.hpp>

namespace aos::crypto {

/***
 * UUID generator interface.
 */
class UUIDItf {
public:
    /**
     * Creates UUID v4.
     *
     * @return RetWithError<uuid::UUID>.
     */
    virtual RetWithError<uuid::UUID> CreateUUIDv4() = 0;

    /**
     * Creates UUID version 5 based on a given namespace identifier and name.
     *
     * @param space namespace identifier.
     * @param name name.
     * @result RetWithError<uuid::UUID>.
     */
    virtual RetWithError<uuid::UUID> CreateUUIDv5(const uuid::UUID& space, const Array<uint8_t>& name) = 0;

    /**
     * Destructor.
     */
    virtual ~UUIDItf() = default;
};

} // namespace aos::crypto

#endif // AOS_CORE_COMMON_CRYPTO_ITF_UUID_HPP_
