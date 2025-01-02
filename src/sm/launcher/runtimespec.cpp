/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "runtimespec.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error AddMount(const oci::Mount& mount, oci::RuntimeSpec& runtimeSpec)
{
    auto [existMount, err] = runtimeSpec.mMounts.FindIf(
        [&destination = mount.mDestination](const oci::Mount& mount) { return mount.mDestination == destination; });
    if (!err.IsNone()) {
        if (err = runtimeSpec.mMounts.PushBack(mount); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        existMount = &runtimeSpec.mMounts[runtimeSpec.mMounts.Size() - 1];
    } else {
        *existMount = mount;
    }

    if (existMount->mType.IsEmpty()) {
        existMount->mType = "bind";
    }

    return ErrorEnum::eNone;
}

Error AddNamespace(const oci::LinuxNamespace& ns, oci::RuntimeSpec& runtimeSpec)
{
    auto [existNS, err] = runtimeSpec.mLinux->mNamespaces.FindIf(
        [&nsType = ns.mType](const oci::LinuxNamespace& ns) { return ns.mType == nsType; });
    if (!err.IsNone()) {
        if (err = runtimeSpec.mLinux->mNamespaces.PushBack(ns); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        *existNS = ns;
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
