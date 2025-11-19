/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_OCISPEC_RUNTIMESPEC_HPP_
#define AOS_CORE_COMMON_OCISPEC_RUNTIMESPEC_HPP_

#include <core/common/tools/map.hpp>
#include <core/common/types/envvars.hpp>
#include <core/common/types/permissions.hpp>

#include "common.hpp"

namespace aos::oci {

/**
 * Max device type len.
 */
constexpr auto cDeviceTypeLen = AOS_CONFIG_OCISPEC_DEV_TYPE_LEN;

/**
 * Max DT devices count.
 */
constexpr auto cMaxDTDevsCount = AOS_CONFIG_OCISPEC_MAX_DT_DEVICES_COUNT;

/**
 * Max DT device name length.
 */
constexpr auto cMaxDTDevLen = AOS_CONFIG_OCISPEC_DT_DEV_NAME_LEN;

/**
 * Max IOMEMs count.
 */
constexpr auto cMaxIOMEMsCount = AOS_CONFIG_OCISPEC_MAX_IOMEMS_COUNT;

/**
 * Max IRQs count.
 */
constexpr auto cMaxIRQsCount = AOS_CONFIG_OCISPEC_MAX_IRQS_COUNT;

/**
 * User name len.
 */
constexpr auto cUserNameLen = AOS_CONFIG_OCISPEC_USER_NAME_LEN;

/**
 * Contains information about the container's root filesystem on the host.
 */
struct Root {
    StaticString<cFilePathLen> mPath;
    bool                       mReadonly;

    /**
     * Compares root spec.
     *
     * @param rhs root spec to compare.
     * @return bool.
     */
    bool operator==(const Root& rhs) const { return mPath == rhs.mPath && mReadonly == rhs.mReadonly; }

    /**
     * Compares root spec.
     *
     * @param rhs root spec to compare.
     * @return bool.
     */
    bool operator!=(const Root& rhs) const { return !operator==(rhs); }
};

/**
 * User specifies specific user (and group) information for the container process.
 */
struct User {
    uid_t                                mUID;
    gid_t                                mGID;
    Optional<uint32_t>                   mUmask;
    StaticArray<uint32_t, cMaxNumGroups> mAdditionalGIDs;
    StaticString<cUserNameLen>           mUsername;

    /**
     * Compares user spec.
     *
     * @param rhs user spec to compare.
     * @return bool.
     */
    bool operator==(const User& rhs) const
    {
        return mUID == rhs.mUID && mGID == rhs.mGID && mUmask == rhs.mUmask && mAdditionalGIDs == rhs.mAdditionalGIDs
            && mUsername == rhs.mUsername;
    }

    /**
     * Compares user spec.
     *
     * @param rhs user spec to compare.
     * @return bool.
     */
    bool operator!=(const User& rhs) const { return !operator==(rhs); }
};

/**
 * LinuxCapabilities specifies the list of allowed capabilities that are kept for a process.
 * http://man7.org/linux/man-pages/man7/capabilities.7.html
 */
struct LinuxCapabilities {
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mBounding;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mEffective;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mInheritable;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mPermitted;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mAmbient;

    /**
     * Compares LinuxCapabilities spec.
     *
     * @param rhs LinuxCapabilities spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxCapabilities& rhs) const
    {
        return mBounding == rhs.mBounding && mEffective == rhs.mEffective && mInheritable == rhs.mInheritable
            && mPermitted == rhs.mPermitted && mAmbient == rhs.mAmbient;
    }

    /**
     * Compares LinuxCapabilities spec.
     *
     * @param rhs LinuxCapabilities spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxCapabilities& capabilities) const { return !operator==(capabilities); }
};

/**
 * POSIXRlimit type and restrictions
 */
struct POSIXRlimit {
    StaticString<cMaxParamLen> mType;
    uint64_t                   mHard;
    uint64_t                   mSoft;

    /**
     * Compares POSIXRlimit spec.
     *
     * @param rhs POSIXRlimit spec to compare.
     * @return bool.
     */
    bool operator==(const POSIXRlimit& rhs) const
    {
        return mType == rhs.mType && mHard == rhs.mHard && mSoft == rhs.mSoft;
    }

    /**
     * Compares POSIXRlimit spec.
     *
     * @param rhs POSIXRlimit spec to compare.
     * @return bool.
     */
    bool operator!=(const POSIXRlimit& rhs) const { return !operator==(rhs); }
};

/**
 * Process contains information to start a specific application inside the container.
 */
struct Process {
    bool                                                    mTerminal;
    User                                                    mUser;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mArgs;
    EnvVarArray                                             mEnv;
    StaticString<cMaxParamLen>                              mCwd;
    bool                                                    mNoNewPrivileges;
    Optional<LinuxCapabilities>                             mCapabilities;
    StaticArray<POSIXRlimit, cMaxParamCount>                mRlimits;

    /**
     * Compares process spec.
     *
     * @param rhs process spec to compare.
     * @return bool.
     */
    bool operator==(const Process& rhs) const
    {
        return mTerminal == rhs.mTerminal && mUser == rhs.mUser && mArgs == rhs.mArgs && mEnv == rhs.mEnv
            && mCwd == rhs.mCwd && mNoNewPrivileges == rhs.mNoNewPrivileges && mCapabilities == rhs.mCapabilities
            && mRlimits == rhs.mRlimits;
    }

    /**
     * Compares process spec.
     *
     * @param rhs process spec to compare.
     * @return bool.
     */
    bool operator!=(const Process& rhs) const { return !operator==(rhs); }
};

/**
 * LinuxDeviceCgroup represents a device rule for the devices specified to the device controller.
 */
struct LinuxDeviceCgroup {
    bool                          mAllow = false;
    StaticString<cDeviceTypeLen>  mType;
    Optional<int64_t>             mMajor;
    Optional<int64_t>             mMinor;
    StaticString<cPermissionsLen> mAccess;

    /**
     * Creates LinuxDeviceCgroup.
     */
    LinuxDeviceCgroup() = default;

    /**
     * Creates LinuxDeviceCgroup.
     *
     * @param type device type.
     * @param access permissions.
     * @param allow allow or deny.
     * @param major major number.
     * @param minor minor number.
     */
    LinuxDeviceCgroup(const String& type, const String& access, bool allow, Optional<int64_t> major = {},
        Optional<int64_t> minor = {})
        : mAllow(allow)
        , mType(type)
        , mMajor(major)
        , mMinor(minor)
        , mAccess(access)
    {
    }

    /**
     * Compares LinuxDeviceCgroup spec.
     *
     * @param rhs LinuxDeviceCgroup spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxDeviceCgroup& rhs) const
    {
        return mType == rhs.mType && mAccess == rhs.mAccess && mAllow == rhs.mAllow && mMajor == rhs.mMajor
            && mMinor == rhs.mMinor;
    }

    /**
     * Compares LinuxDeviceCgroup spec.
     *
     * @param rhs LinuxDeviceCgroup spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxDeviceCgroup& rhs) const { return !operator==(rhs); }
};

/**
 * Linux cgroup 'memory' resource management.
 */
struct LinuxMemory {
    Optional<int64_t>  mLimit;
    Optional<int64_t>  mReservation;
    Optional<int64_t>  mSwap;
    Optional<int64_t>  mKernel;
    Optional<int64_t>  mKernelTCP;
    Optional<uint64_t> mSwappiness;
    Optional<bool>     mDisableOOMKiller;
    Optional<bool>     mUseHierarchy;
    Optional<bool>     mCheckBeforeUpdate;

    /**
     * Compares LinuxMemory spec.
     *
     * @param rhs LinuxMemory spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxMemory& rhs) const
    {
        return mLimit == rhs.mLimit && mReservation == rhs.mReservation && mSwap == rhs.mSwap && mKernel == rhs.mKernel
            && mKernelTCP == rhs.mKernelTCP && mSwappiness == rhs.mSwappiness
            && mDisableOOMKiller == rhs.mDisableOOMKiller && mUseHierarchy == rhs.mUseHierarchy
            && mCheckBeforeUpdate == rhs.mCheckBeforeUpdate;
    }

    /**
     * Compares LinuxMemory spec.
     *
     * @param rhs LinuxMemory spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxMemory& rhs) const { return !operator==(rhs); }
};

/**
 * Linux cgroup 'cpu' resource management.
 */
struct LinuxCPU {
    Optional<uint64_t>                   mShares;
    Optional<int64_t>                    mQuota;
    Optional<uint64_t>                   mBurst;
    Optional<uint64_t>                   mPeriod;
    Optional<int64_t>                    mRealtimeRuntime;
    Optional<uint64_t>                   mRealtimePeriod;
    Optional<StaticString<cMaxParamLen>> mCpus;
    Optional<StaticString<cMaxParamLen>> mMems;
    Optional<int64_t>                    mIdle;

    /**
     * Compares LinuxCPU spec.
     *
     * @param rhs LinuxCPU spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxCPU& rhs) const
    {
        return mShares == rhs.mShares && mQuota == rhs.mQuota && mBurst == rhs.mBurst && mPeriod == rhs.mPeriod
            && mRealtimeRuntime == rhs.mRealtimeRuntime && mRealtimePeriod == rhs.mRealtimePeriod && mCpus == rhs.mCpus
            && mMems == rhs.mMems && mIdle == rhs.mIdle;
    }

    /**
     * Compares LinuxCPU spec.
     *
     * @param rhs LinuxCPU spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxCPU& rhs) const { return !operator==(rhs); }
};

/**
 * Linux cgroup 'pids' resource management (Linux 4.3).
 */
struct LinuxPids {
    int64_t mLimit;
};

/**
 * LinuxResources has container runtime resource constraints.
 */
struct LinuxResources {
    StaticArray<LinuxDeviceCgroup, cMaxNumHostDevices> mDevices;
    Optional<LinuxMemory>                              mMemory;
    Optional<LinuxCPU>                                 mCPU;
    Optional<LinuxPids>                                mPids;

    /**
     * Compares LinuxResources spec.
     *
     * @param rhs LinuxResources spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxResources& rhs) const { return mDevices == rhs.mDevices; }

    /**
     * Compares LinuxResources spec.
     *
     * @param rhs LinuxResources spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxResources& rhs) const { return !operator==(rhs); }
};

/**
 * LinuxNamespaceType is one of the Linux namespaces.
 */
class LinuxNamespaceTypeDesc {
public:
    enum class Enum {
        ePID,
        eNetwork,
        eMount,
        eIPC,
        eUTS,
        eUser,
        eCgroup,
        eTime,
        eNumNamespaces,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sLinuxNamespaceStrings[] = {
            "pid",
            "network",
            "mount",
            "ipc",
            "uts",
            "user",
            "cgroup",
            "time",
            "unknown",
        };

        return Array<const char* const>(sLinuxNamespaceStrings, ArraySize(sLinuxNamespaceStrings));
    };
};

using LinuxNamespaceEnum = LinuxNamespaceTypeDesc::Enum;
using LinuxNamespaceType = EnumStringer<LinuxNamespaceTypeDesc>;

constexpr auto cMaxNumNamespaces = static_cast<size_t>(LinuxNamespaceEnum::eNumNamespaces);

/**
 * LinuxNamespace is the configuration for a Linux namespace.
 */
struct LinuxNamespace {
    LinuxNamespaceType         mType;
    StaticString<cMaxParamLen> mPath;

    /**
     * Creates LinuxNamespace.
     */
    LinuxNamespace() = default;

    /**
     * Creates LinuxNamespace.
     */
    explicit LinuxNamespace(LinuxNamespaceType type, const String& path = "")
        : mType(type)
        , mPath(path)
    {
    }

    /**
     * Compares LinuxNamespace spec.
     *
     * @param rhs LinuxNamespace spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxNamespace& rhs) const { return mType == rhs.mType && mPath == rhs.mPath; }

    /**
     * Compares LinuxNamespace spec.
     *
     * @param rhs LinuxNamespace spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxNamespace& rhs) const { return !operator==(rhs); }
};

/**
 * Represents the mknod information for a Linux special device file.
 */
struct LinuxDevice {
    StaticString<cFilePathLen>   mPath;
    StaticString<cDeviceTypeLen> mType;
    int64_t                      mMajor;
    int64_t                      mMinor;
    Optional<uint32_t>           mFileMode;
    Optional<uid_t>              mUID;
    Optional<gid_t>              mGID;

    /**
     * Compares LinuxDevice spec.
     *
     * @param rhs LinuxDevice spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxDevice& rhs) const
    {
        return mPath == rhs.mPath && mType == rhs.mType && mMajor == rhs.mMajor && mMinor == rhs.mMinor
            && mFileMode == rhs.mFileMode && mUID == rhs.mUID && mGID == rhs.mGID;
    }

    /**
     * Compares LinuxDevice spec.
     *
     * @param rhs LinuxDevice spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxDevice& rhs) const { return !operator==(rhs); }
};

/**
 * Linux contains platform-specific configuration for Linux based containers.
 */
struct Linux {
    StaticMap<StaticString<cSysctlLen>, StaticString<cSysctlLen>, cSysctlMaxCount> mSysctl;
    Optional<LinuxResources>                                                       mResources;
    StaticString<cFilePathLen>                                                     mCgroupsPath;
    StaticArray<LinuxNamespace, cMaxNumNamespaces>                                 mNamespaces;
    StaticArray<LinuxDevice, cMaxNumHostDevices>                                   mDevices;
    StaticArray<StaticString<cFilePathLen>, cMaxParamCount>                        mMaskedPaths;
    StaticArray<StaticString<cFilePathLen>, cMaxParamCount>                        mReadonlyPaths;

    /**
     * Compares Linux spec.
     *
     * @param rhs Linux spec to compare.
     * @return bool.
     */
    bool operator==(const Linux& rhs) const
    {
        return mSysctl == rhs.mSysctl && mResources == rhs.mResources && mCgroupsPath == rhs.mCgroupsPath
            && mNamespaces == rhs.mNamespaces && mMaskedPaths == rhs.mMaskedPaths
            && mReadonlyPaths == rhs.mReadonlyPaths;
    }

    /**
     * Compares Linux spec.
     *
     * @param rhs Linux spec to compare.
     * @return bool.
     */
    bool operator!=(const Linux& rhs) const { return !operator==(rhs); }
};

/**
 * Contains information about the hypervisor to use for a virtual machine.
 */
struct VMHypervisor {
    StaticString<cFilePathLen>                              mPath;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mParameters;

    /**
     * Compares VMHypervisor spec.
     *
     * @param rhs VMHypervisor spec to compare.
     * @return bool.
     */
    bool operator==(const VMHypervisor& rhs) const { return mPath == rhs.mPath && mParameters == rhs.mParameters; }

    /**
     * Compares VMHypervisor spec.
     *
     * @param rhs VMHypervisor spec to compare.
     * @return bool.
     */
    bool operator!=(const VMHypervisor& rhs) const { return !operator==(rhs); }
};

/**
 * Contains information about the kernel to use for a virtual machine.
 */
struct VMKernel {
    StaticString<cFilePathLen>                              mPath;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mParameters;

    /**
     * Compares VMKernel spec.
     *
     * @param rhs VMKernel spec to compare.
     * @return bool.
     */
    bool operator==(const VMKernel& rhs) const { return mPath == rhs.mPath && mParameters == rhs.mParameters; }

    /**
     * Compares VMKernel spec.
     *
     * @param rhs VMKernel spec to compare.
     * @return bool.
     */
    bool operator!=(const VMKernel& rhs) const { return !operator==(rhs); }
};

/**
 * Contains information about IOMEMs.
 */
struct VMHWConfigIOMEM {
    uint64_t mFirstGFN;
    uint64_t mFirstMFN;
    uint64_t mNrMFNs;

    /**
     * Compares IOMEMs.
     *
     * @param rhs IOMEM to compare.
     * @return bool.
     */
    bool operator==(const VMHWConfigIOMEM& rhs) const
    {
        return mFirstGFN == rhs.mFirstGFN && mFirstMFN == rhs.mFirstMFN && mNrMFNs == rhs.mNrMFNs;
    }

    /**
     * Compares IOMEMs.
     *
     * @param rhs IOMEM to compare.
     * @return bool.
     */
    bool operator!=(const VMHWConfigIOMEM& rhs) const { return !operator==(rhs); }
};

/**
 * Contains information about HW configuration.
 */
struct VMHWConfig {
    StaticString<cFilePathLen>                               mDeviceTree;
    uint32_t                                                 mVCPUs;
    uint64_t                                                 mMemKB;
    StaticArray<StaticString<cMaxDTDevLen>, cMaxDTDevsCount> mDTDevs;
    StaticArray<VMHWConfigIOMEM, cMaxIOMEMsCount>            mIOMEMs;
    StaticArray<uint32_t, cMaxIRQsCount>                     mIRQs;

    /**
     * Compares VMHWConfig spec.
     *
     * @param rhs VMHWConfig spec to compare.
     * @return bool.
     */
    bool operator==(const VMHWConfig& rhs) const
    {
        return mDeviceTree == rhs.mDeviceTree && mVCPUs == rhs.mVCPUs && mMemKB == rhs.mMemKB && mDTDevs == rhs.mDTDevs
            && mIOMEMs == rhs.mIOMEMs && mIRQs == rhs.mIRQs;
    }

    /**
     * Compares VMHWConfig spec.
     *
     * @param rhs VMHWConfig spec to compare.
     * @return bool.
     */
    bool operator!=(const VMHWConfig& rhs) const { return !operator==(rhs); }
};

/**
 * Contains information for virtual-machine-based containers.
 */
struct VM {
    VMHypervisor mHypervisor;
    VMKernel     mKernel;
    VMHWConfig   mHWConfig;

    /**
     * Compares VM spec.
     *
     * @param rhs VM spec to compare.
     * @return bool.
     */
    bool operator==(const VM& rhs) const
    {
        return mHypervisor == rhs.mHypervisor && mKernel == rhs.mKernel && mHWConfig == rhs.mHWConfig;
    }

    /**
     * Compares VM spec.
     *
     * @param rhs VM spec to compare.
     * @return bool.
     */
    bool operator!=(const VM& rhs) const { return !operator==(rhs); }
};

/**
 * OCI runtime config.
 */
struct RuntimeConfig {
    StaticString<cVersionLen>           mOCIVersion;
    Optional<Process>                   mProcess;
    Optional<Root>                      mRoot;
    StaticString<cHostNameLen>          mHostname;
    StaticArray<Mount, cMaxNumFSMounts> mMounts;
    Optional<Linux>                     mLinux;
    Optional<VM>                        mVM;

    /**
     * Compares runtime spec.
     *
     * @param rhs runtime spec to compare.
     * @return bool.
     */
    bool operator==(const RuntimeConfig& rhs) const
    {
        return mOCIVersion == rhs.mOCIVersion && mProcess == rhs.mProcess && mRoot == rhs.mRoot
            && mHostname == rhs.mHostname && mMounts == rhs.mMounts && mLinux == rhs.mLinux && mVM == rhs.mVM;
    }

    /**
     * Compares runtime spec.
     *
     * @param rhs runtime spec to compare.
     * @return bool.
     */
    bool operator!=(const RuntimeConfig& rhs) const { return !operator==(rhs); }
};

/**
 * Creates example runtime spec.
 *
 * @param spec runtime spec.
 * @param isCgroup2UnifiedMode adds croup namespace.
 */
inline Error CreateExampleRuntimeSpec(RuntimeConfig& config, bool isCgroup2UnifiedMode = true)
{
    config.mOCIVersion = cVersion;

    config.mRoot.EmplaceValue();

    config.mRoot->mPath     = "rootfs";
    config.mRoot->mReadonly = true;

    config.mProcess.EmplaceValue();

    config.mProcess->mTerminal = true;
    config.mProcess->mUser     = {};
    config.mProcess->mArgs.Clear();
    config.mProcess->mArgs.PushBack("sh");
    config.mProcess->mEnv.Clear();
    config.mProcess->mEnv.PushBack("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    config.mProcess->mEnv.PushBack("TERM=xterm");
    config.mProcess->mCwd             = "/";
    config.mProcess->mNoNewPrivileges = true;

    config.mProcess->mCapabilities.EmplaceValue();

    config.mProcess->mCapabilities->mBounding.Clear();
    config.mProcess->mCapabilities->mBounding.PushBack("CAP_AUDIT_WRITE");
    config.mProcess->mCapabilities->mBounding.PushBack("CAP_KILL");
    config.mProcess->mCapabilities->mBounding.PushBack("CAP_NET_BIND_SERVICE");
    config.mProcess->mCapabilities->mPermitted.Clear();
    config.mProcess->mCapabilities->mPermitted.PushBack("CAP_AUDIT_WRITE");
    config.mProcess->mCapabilities->mPermitted.PushBack("CAP_KILL");
    config.mProcess->mCapabilities->mPermitted.PushBack("CAP_NET_BIND_SERVICE");
    config.mProcess->mCapabilities->mEffective.Clear();
    config.mProcess->mCapabilities->mEffective.PushBack("CAP_AUDIT_WRITE");
    config.mProcess->mCapabilities->mEffective.PushBack("CAP_KILL");
    config.mProcess->mCapabilities->mEffective.PushBack("CAP_NET_BIND_SERVICE");

    config.mProcess->mRlimits.Clear();
    config.mProcess->mRlimits.PushBack({"RLIMIT_NOFILE", 1024, 1024});

    config.mHostname = "runc";

    config.mMounts.Clear();
    config.mMounts.EmplaceBack("proc", "/proc", "proc");
    config.mMounts.EmplaceBack("tmpfs", "/dev", "tmpfs", "nosuid,strictatime,mode=755,size=65536k");
    config.mMounts.EmplaceBack(
        "devpts", "/dev/pts", "devpts", "nosuid,noexec,newinstance,ptmxmode=0666,mode=0620,gid=5");
    config.mMounts.EmplaceBack("shm", "/dev/shm", "tmpfs", "nosuid,noexec,nodev,mode=1777,size=65536k");
    config.mMounts.EmplaceBack("mqueue", "/dev/mqueue", "mqueue", "nosuid,noexec,nodev");
    config.mMounts.EmplaceBack("sysfs", "/sys", "sysfs", "nosuid,noexec,nodev,ro");
    config.mMounts.EmplaceBack("cgroup", "/sys/fs/cgroup", "cgroup", "nosuid,noexec,nodev,relatime,ro");

    config.mLinux.EmplaceValue();

    config.mLinux->mMaskedPaths.Clear();
    config.mLinux->mMaskedPaths.PushBack("/proc/acpi");
    config.mLinux->mMaskedPaths.PushBack("/proc/asound");
    config.mLinux->mMaskedPaths.PushBack("/proc/kcore");
    config.mLinux->mMaskedPaths.PushBack("/proc/keys");
    config.mLinux->mMaskedPaths.PushBack("/proc/latency_stats");
    config.mLinux->mMaskedPaths.PushBack("/proc/timer_list");
    config.mLinux->mMaskedPaths.PushBack("/proc/timer_stats");
    config.mLinux->mMaskedPaths.PushBack("/proc/sched_debug");
    config.mLinux->mMaskedPaths.PushBack("/proc/scsi");
    config.mLinux->mMaskedPaths.PushBack("/sys/firmware");

    config.mLinux->mReadonlyPaths.Clear();
    config.mLinux->mReadonlyPaths.PushBack("/proc/bus");
    config.mLinux->mReadonlyPaths.PushBack("/proc/fs");
    config.mLinux->mReadonlyPaths.PushBack("/proc/irq");
    config.mLinux->mReadonlyPaths.PushBack("/proc/sys");
    config.mLinux->mReadonlyPaths.PushBack("/proc/sysrq-trigger");

    config.mLinux->mResources.EmplaceValue();

    config.mLinux->mResources->mDevices.Clear();
    config.mLinux->mResources->mDevices.EmplaceBack("", "rwm", false);

    config.mLinux->mNamespaces.Clear();
    config.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::ePID);
    config.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eNetwork);
    config.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eIPC);
    config.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eUTS);
    config.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eMount);

    if (isCgroup2UnifiedMode) {
        config.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eCgroup);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::oci

#endif
