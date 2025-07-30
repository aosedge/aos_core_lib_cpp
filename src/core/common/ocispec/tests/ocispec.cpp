
/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/ocispec/ocispec.hpp"
#include "aos/test/log.hpp"
#include "aos/test/utils.hpp"

namespace aos::oci {

using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class OCISpecTest : public Test {
protected:
    void SetUp() override { test::InitLog(); }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(OCISpecTest, CreateExampleRuntimeSpec)
{
    auto spec = std::make_shared<RuntimeSpec>();

    LOG_DBG() << "Runtime spec: size=" << sizeof(RuntimeSpec) << " bytes";

    auto err = CreateExampleRuntimeSpec(*spec);
    EXPECT_TRUE(err.IsNone()) << "Error creating example runtime spec: err=" << test::ErrorToStr(err);

    EXPECT_EQ(spec->mOCIVersion, cVersion);

    EXPECT_EQ(spec->mRoot->mPath, "rootfs");
    EXPECT_EQ(spec->mRoot->mReadonly, true);

    EXPECT_EQ(spec->mProcess->mTerminal, true);
    EXPECT_EQ(spec->mProcess->mUser.mUID, 0);
    EXPECT_EQ(spec->mProcess->mUser.mGID, 0);

    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> args;

    args.PushBack("sh");

    EXPECT_EQ(spec->mProcess->mArgs, args);

    EnvVarsArray env;

    env.PushBack("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    env.PushBack("TERM=xterm");

    EXPECT_EQ(spec->mProcess->mEnv, env);

    EXPECT_EQ(spec->mProcess->mCwd, "/");
    EXPECT_EQ(spec->mProcess->mNoNewPrivileges, true);

    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> caps;

    caps.PushBack("CAP_AUDIT_WRITE");
    caps.PushBack("CAP_KILL");
    caps.PushBack("CAP_NET_BIND_SERVICE");

    EXPECT_EQ(spec->mProcess->mCapabilities->mBounding, caps);
    EXPECT_EQ(spec->mProcess->mCapabilities->mPermitted, caps);
    EXPECT_EQ(spec->mProcess->mCapabilities->mEffective, caps);

    StaticArray<POSIXRlimit, cMaxParamCount> rlimits;

    rlimits.PushBack({"RLIMIT_NOFILE", 1024, 1024});

    EXPECT_EQ(spec->mProcess->mRlimits, rlimits);

    EXPECT_EQ(spec->mHostname, "runc");

    StaticArray<Mount, cMaxNumFSMounts> mounts;

    mounts.EmplaceBack("proc", "/proc", "proc");
    mounts.EmplaceBack("tmpfs", "/dev", "tmpfs", "nosuid,strictatime,mode=755,size=65536k");
    mounts.EmplaceBack("devpts", "/dev/pts", "devpts", "nosuid,noexec,newinstance,ptmxmode=0666,mode=0620,gid=5");
    mounts.EmplaceBack("shm", "/dev/shm", "tmpfs", "nosuid,noexec,nodev,mode=1777,size=65536k");
    mounts.EmplaceBack("mqueue", "/dev/mqueue", "mqueue", "nosuid,noexec,nodev");
    mounts.EmplaceBack("sysfs", "/sys", "sysfs", "nosuid,noexec,nodev,ro");
    mounts.EmplaceBack("cgroup", "/sys/fs/cgroup", "cgroup", "nosuid,noexec,nodev,relatime,ro");

    EXPECT_EQ(spec->mMounts, mounts);

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

    EXPECT_EQ(spec->mLinux->mMaskedPaths, paths);

    paths.Clear();
    paths.PushBack("/proc/bus");
    paths.PushBack("/proc/fs");
    paths.PushBack("/proc/irq");
    paths.PushBack("/proc/sys");
    paths.PushBack("/proc/sysrq-trigger");

    EXPECT_EQ(spec->mLinux->mReadonlyPaths, paths);

    StaticArray<LinuxDeviceCgroup, cMaxNumHostDevices> devs;

    devs.EmplaceBack("", "rwm", false);

    EXPECT_EQ(spec->mLinux->mResources->mDevices, devs);

    StaticArray<LinuxNamespace, cMaxNumNamespaces> namespaces;

    namespaces.EmplaceBack(LinuxNamespaceEnum::ePID);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eNetwork);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eIPC);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eUTS);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eMount);
    namespaces.EmplaceBack(LinuxNamespaceEnum::eCgroup);

    EXPECT_EQ(spec->mLinux->mNamespaces, namespaces);
}

} // namespace aos::oci
