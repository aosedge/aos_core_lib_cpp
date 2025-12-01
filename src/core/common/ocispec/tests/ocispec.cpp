
/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

namespace aos::oci {

using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class OCISpecTest : public Test {
protected:
    void SetUp() override { tests::utils::InitLog(); }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(OCISpecTest, CreateExampleRuntimeConfig)
{
    auto runtimeConfig = std::make_shared<RuntimeConfig>();

    LOG_DBG() << "Runtime spec: size=" << sizeof(RuntimeConfig) << " bytes";

    auto err = CreateExampleRuntimeSpec(*runtimeConfig);
    EXPECT_TRUE(err.IsNone()) << "Error creating example runtime spec: err=" << tests::utils::ErrorToStr(err);

    EXPECT_EQ(runtimeConfig->mOCIVersion, cVersion);

    EXPECT_EQ(runtimeConfig->mRoot->mPath, "rootfs");
    EXPECT_EQ(runtimeConfig->mRoot->mReadonly, true);

    EXPECT_EQ(runtimeConfig->mProcess->mTerminal, true);
    EXPECT_EQ(runtimeConfig->mProcess->mUser.mUID, 0);
    EXPECT_EQ(runtimeConfig->mProcess->mUser.mGID, 0);

    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> args;

    args.PushBack("sh");

    EXPECT_EQ(runtimeConfig->mProcess->mArgs, args);

    StaticArray<StaticString<cEnvVarLen>, cMaxNumEnvVariables> env;

    env.PushBack("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    env.PushBack("TERM=xterm");

    EXPECT_EQ(runtimeConfig->mProcess->mEnv, env);

    EXPECT_EQ(runtimeConfig->mProcess->mCwd, "/");
    EXPECT_EQ(runtimeConfig->mProcess->mNoNewPrivileges, true);

    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> caps;

    caps.PushBack("CAP_AUDIT_WRITE");
    caps.PushBack("CAP_KILL");
    caps.PushBack("CAP_NET_BIND_SERVICE");

    EXPECT_EQ(runtimeConfig->mProcess->mCapabilities->mBounding, caps);
    EXPECT_EQ(runtimeConfig->mProcess->mCapabilities->mPermitted, caps);
    EXPECT_EQ(runtimeConfig->mProcess->mCapabilities->mEffective, caps);

    StaticArray<POSIXRlimit, cMaxParamCount> rlimits;

    rlimits.PushBack({"RLIMIT_NOFILE", 1024, 1024});

    EXPECT_EQ(runtimeConfig->mProcess->mRlimits, rlimits);

    EXPECT_EQ(runtimeConfig->mHostname, "runc");

    StaticArray<Mount, cMaxNumFSMounts> mounts;

    mounts.EmplaceBack("proc", "/proc", "proc");
    mounts.EmplaceBack("tmpfs", "/dev", "tmpfs", "nosuid,strictatime,mode=755,size=65536k");
    mounts.EmplaceBack("devpts", "/dev/pts", "devpts", "nosuid,noexec,newinstance,ptmxmode=0666,mode=0620,gid=5");
    mounts.EmplaceBack("shm", "/dev/shm", "tmpfs", "nosuid,noexec,nodev,mode=1777,size=65536k");
    mounts.EmplaceBack("mqueue", "/dev/mqueue", "mqueue", "nosuid,noexec,nodev");
    mounts.EmplaceBack("sysfs", "/sys", "sysfs", "nosuid,noexec,nodev,ro");
    mounts.EmplaceBack("cgroup", "/sys/fs/cgroup", "cgroup", "nosuid,noexec,nodev,relatime,ro");

    EXPECT_EQ(runtimeConfig->mMounts, mounts);

    StaticArray<StaticString<cFilePathLen>, cMaxParamCount> paths;

    paths.PushBack("/proc/acpi");
    paths.PushBack("/proc/asound");
    paths.PushBack("/proc/kcore");
    paths.PushBack("/proc/keys");
    paths.PushBack("/proc/latency_stats");
    paths.PushBack("/proc/timer_list");
    paths.PushBack("/proc/timer_stats");
    paths.PushBack("/proc/sched_debug");
    paths.PushBack("/proc/scsi");
    paths.PushBack("/sys/firmware");

    EXPECT_EQ(runtimeConfig->mLinux->mMaskedPaths, paths);

    paths.Clear();
    paths.PushBack("/proc/bus");
    paths.PushBack("/proc/fs");
    paths.PushBack("/proc/irq");
    paths.PushBack("/proc/sys");
    paths.PushBack("/proc/sysrq-trigger");

    EXPECT_EQ(runtimeConfig->mLinux->mReadonlyPaths, paths);

    StaticArray<LinuxDeviceCgroup, cMaxNumHostDevices> devs;

    devs.EmplaceBack("", "rwm", false);

    EXPECT_EQ(runtimeConfig->mLinux->mResources->mDevices, devs);

    StaticArray<LinuxNamespace, cMaxNumNamespaces> namespaces;

    namespaces.EmplaceBack(LinuxNamespaceEnum::ePID);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eNetwork);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eIPC);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eUTS);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eMount);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eCgroup);

    EXPECT_EQ(runtimeConfig->mLinux->mNamespaces, namespaces);
}

} // namespace aos::oci
