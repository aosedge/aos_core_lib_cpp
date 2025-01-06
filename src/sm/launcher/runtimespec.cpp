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

Error AddMount(const Mount& mount, oci::RuntimeSpec& runtimeSpec)
{
    auto [existMount, err] = runtimeSpec.mMounts.FindIf(
        [&destination = mount.mDestination](const Mount& mount) { return mount.mDestination == destination; });
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

Error SetCPULimit(int64_t quota, uint64_t period, oci::RuntimeSpec& runtimeSpec)
{
    if (!runtimeSpec.mLinux->mResources->mCPU.HasValue()) {
        runtimeSpec.mLinux->mResources->mCPU.EmplaceValue();
    }

    runtimeSpec.mLinux->mResources->mCPU->mPeriod = period;
    runtimeSpec.mLinux->mResources->mCPU->mQuota  = quota;

    return ErrorEnum::eNone;
}

Error SetRAMLimit(int64_t limit, oci::RuntimeSpec& runtimeSpec)
{
    if (!runtimeSpec.mLinux->mResources->mMemory.HasValue()) {
        runtimeSpec.mLinux->mResources->mMemory.EmplaceValue();
    }

    runtimeSpec.mLinux->mResources->mMemory->mLimit = limit;

    return ErrorEnum::eNone;
}

Error SetPIDLimit(int64_t limit, oci::RuntimeSpec& runtimeSpec)
{
    if (!runtimeSpec.mLinux->mResources->mPids.HasValue()) {
        runtimeSpec.mLinux->mResources->mPids.EmplaceValue();
    }

    runtimeSpec.mLinux->mResources->mPids->mLimit = limit;

    return ErrorEnum::eNone;
}

Error AddRLimit(const oci::POSIXRlimit& rlimit, oci::RuntimeSpec& runtimeSpec)
{
    auto [existRlimit, err] = runtimeSpec.mProcess->mRlimits.FindIf(
        [&type = rlimit.mType](const oci::POSIXRlimit& rlimit) { return rlimit.mType == type; });
    if (!err.IsNone()) {
        if (err = runtimeSpec.mProcess->mRlimits.PushBack(rlimit); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        *existRlimit = rlimit;
    }

    return ErrorEnum::eNone;
}

Error AddAdditionalGID(uint32_t gid, oci::RuntimeSpec& runtimeSpec)
{
    if (runtimeSpec.mProcess->mUser.mAdditionalGIDs.Find(gid).mError.IsNone()) {
        return ErrorEnum::eNone;
    }

    if (auto err = runtimeSpec.mProcess->mUser.mAdditionalGIDs.PushBack(gid); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error AddDevice(
    const oci::LinuxDevice& device, const StaticString<cPermissionsLen>& permissions, oci::RuntimeSpec& runtimeSpec)
{
    Error                   err;
    oci::LinuxDevice*       existDevice;
    oci::LinuxDeviceCgroup* existCgroup;

    Tie(existDevice, err) = runtimeSpec.mLinux->mDevices.FindIf(
        [&path = device.mPath](const oci::LinuxDevice& device) { return device.mPath == path; });
    if (!err.IsNone()) {
        if (err = runtimeSpec.mLinux->mDevices.PushBack(device); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        *existDevice = device;
    }

    Tie(existCgroup, err)
        = runtimeSpec.mLinux->mResources->mDevices.FindIf([&device](const oci::LinuxDeviceCgroup& cgroupDevice) {
              return cgroupDevice.mType == device.mType && cgroupDevice.mMajor == device.mMajor
                  && cgroupDevice.mMinor == device.mMinor;
          });
    if (!err.IsNone()) {
        if (err = runtimeSpec.mLinux->mResources->mDevices.EmplaceBack(
                device.mType, permissions, true, device.mMajor, device.mMinor);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        existCgroup->mAllow  = true;
        existCgroup->mAccess = permissions;
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
