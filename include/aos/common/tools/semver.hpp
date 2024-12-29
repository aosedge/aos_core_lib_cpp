/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SEMVER_HPP_
#define AOS_SEMVER_HPP_

#include "aos/common/tools/string.hpp"

namespace aos::semver {

/**
 * Validates semantic version.
 *
 * @param version version to validate.
 * @return Error.
 */
Error ValidateSemver(const String& version);

/**
 * Compares two semantic versions.
 *
 * @param version1 first version.
 * @param version2 second version.
 * @return RetWithError<int>.
 */
RetWithError<int> CompareSemver(const String& version1, const String& version2);

} // namespace aos::semver

#endif
