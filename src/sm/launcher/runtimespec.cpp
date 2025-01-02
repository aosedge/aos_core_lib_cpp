/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "runtimespec.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

RetWithError<StaticString<cEnvVarNameLen>> GetEnvVarName(const String& envVar)
{
    StaticString<cEnvVarNameLen> tmpStr = envVar;

    auto result = tmpStr.FindSubstr(0, "=");
    if (result.mError.IsNone()) {
        if (auto err = tmpStr.Resize(result.mValue); !err.IsNone()) {
            return {0, AOS_ERROR_WRAP(err)};
        }
    }

    tmpStr.Trim(" ");

    return {tmpStr, ErrorEnum::eNone};
}

} // namespace

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

Error AddEnvVars(const Array<StaticString<cEnvVarNameLen>>& envVars, oci::RuntimeSpec& runtimeSpec)
{
    for (const auto& newEnvVar : envVars) {
        StaticString<cEnvVarNameLen> newEnvVarName;
        Error                        err;

        Tie(newEnvVarName, err) = GetEnvVarName(newEnvVar);
        if (!err.IsNone()) {
            return err;
        }

        auto updated = false;

        for (auto& existingEnvVar : runtimeSpec.mProcess->mEnv) {
            StaticString<cEnvVarNameLen> existingEnvVarName;

            Tie(existingEnvVarName, err) = GetEnvVarName(existingEnvVar);
            if (!err.IsNone()) {
                return err;
            }

            if (newEnvVarName == existingEnvVarName) {
                existingEnvVar = newEnvVar;
                updated        = true;

                break;
            }
        }

        if (!updated) {
            if (err = runtimeSpec.mProcess->mEnv.PushBack(newEnvVar); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
