/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_OCISPEC_COMPONENTCONFIG_HPP_
#define AOS_CORE_COMMON_OCISPEC_COMPONENTCONFIG_HPP_

#include <core/common/types/common.hpp>

#include "common.hpp"

namespace aos::oci {

/**
 * Component configuration.
 */
struct ComponentConfig {
    Time                          mCreated;
    StaticString<cAuthorLen>      mAuthor;
    StaticString<cRuntimeTypeLen> mRunner;

    /**
     * Compares component config.
     *
     * @param rhs component config to compare.
     * @return bool.
     */
    bool operator==(const ComponentConfig& rhs) const
    {
        return mCreated == rhs.mCreated && mAuthor == rhs.mAuthor && mRunner == rhs.mRunner;
    }

    /**
     * Compares component config.
     *
     * @param rhs component config to compare.
     * @return bool.
     */
    bool operator!=(const ComponentConfig& rhs) const { return !operator==(rhs); }
};

} // namespace aos::oci

#endif
