/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OCICOMMON_HPP_
#define AOS_OCICOMMON_HPP_

#include "aos/common/config.hpp"

namespace aos::oci {

constexpr auto cVersion = "1.1.0";

/**
 * Spec parameter max len.
 */
constexpr auto cMaxParamLen = AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_LEN;

/**
 * Spec parameter max count.
 */
constexpr auto cMaxParamCount = AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_COUNT;

/**
 * Author len.
 */
constexpr auto cAuthorLen = AOS_CONFIG_OCISPEC_AUTHOR_LEN;

/**
 * Max sysctl name len.
 */
constexpr auto cSysctlLen = AOS_CONFIG_OCISPEC_SYSCTL_LEN;

/**
 * Max sysctl count.
 */
constexpr auto cSysctlMaxCount = AOS_CONFIG_OCISPEC_SYSCTL_MAX_COUNT;

} // namespace aos::oci

#endif
