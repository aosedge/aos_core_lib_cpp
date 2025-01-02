/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAUNCHER_RUNTIMESPEC_HPP_
#define AOS_LAUNCHER_RUNTIMESPEC_HPP_

#include "aos/common/ocispec/ocispec.hpp"

namespace aos::sm::launcher {

/**
 * Adds mount entry to runtime spec.
 *
 * @param mount mount entry to add.
 * @param runtimeSpec runtime spec.
 * @return Error.
 */
Error AddMount(const oci::Mount& mount, oci::RuntimeSpec& runtimeSpec);

/**
 * Adds namespace path.
 *
 * @param ns OCI Linux namespace.
 * @param runtimeSpec runtime spec.
 * @return Error.
 */
Error AddNamespace(const oci::LinuxNamespace& ns, oci::RuntimeSpec& runtimeSpec);

/**
 * Adds environment variables.
 *
 * @param envVars environment variables to set.
 * @param runtimeSpec runtime spec.
 * @return Error.
 */
Error AddEnvVars(const Array<StaticString<cEnvVarNameLen>>& envVars, oci::RuntimeSpec& runtimeSpec);

} // namespace aos::sm::launcher

#endif
