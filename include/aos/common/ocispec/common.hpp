/*
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OCICOMMON_HPP_
#define AOS_OCICOMMON_HPP_

#include "aos/common/config.hpp"

namespace aos::oci {

/**
 * Spec parameter max len.
 */
constexpr auto cMaxParamLen = AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_LEN;

/**
 * Spec parameter max count.
 */
constexpr auto cMaxParamCount = AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_COUNT;

} // namespace aos::oci

#endif
