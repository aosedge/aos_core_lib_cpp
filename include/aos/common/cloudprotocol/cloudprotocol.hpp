/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_HPP_
#define AOS_CLOUDPROTOCOL_HPP_

#include "aos/common/tools/optional.hpp"
#include "aos/common/types.hpp"

namespace aos {
namespace cloudprotocol {

/**
 * Instance filter.
 */
struct InstanceFilter {
    Optional<StaticString<cServiceIDLen>> mServiceID;
    Optional<StaticString<cSubjectIDLen>> mSubjectID;
    Optional<uint64_t>                    mInstance;

    /**
     * Compares instance filter.
     *
     * @param filter instance filter to compare with.
     * @return bool.
     */
    bool operator==(const InstanceFilter& filter) const
    {
        return mServiceID == filter.mServiceID && mSubjectID == filter.mSubjectID && mInstance == filter.mInstance;
    }

    /**
     * Compares instance filter.
     *
     * @param filter instance filter to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceFilter& filter) const { return !operator==(filter); }
};

} // namespace cloudprotocol
} // namespace aos

#endif
