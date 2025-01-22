/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RUNTIMESPEC_HPP_
#define AOS_RUNTIMESPEC_HPP_

#include "aos/common/ocispec/common.hpp"
#include "aos/common/tools/map.hpp"
#include "aos/common/types.hpp"

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
     * @param root root spec to compare.
     * @return bool.
     */
    bool operator==(const Root& root) const { return mPath == root.mPath && mReadonly == root.mReadonly; }

    /**
     * Compares root spec.
     *
     * @param root root spec to compare.
     * @return bool.
     */
    bool operator!=(const Root& root) const { return !operator==(root); }
};

/**
 * User specifies specific user (and group) information for the container process.
 */
struct User {
    uint32_t                             mUID;
    uint32_t                             mGID;
    Optional<uint32_t>                   mUmask;
    StaticArray<uint32_t, cMaxNumGroups> mAdditionalGIDs;
    StaticString<cUserNameLen>           mUsername;

    /**
     * Compares user spec.
     *
     * @param user user spec to compare.
     * @return bool.
     */
    bool operator==(const User& user) const
    {
        return mUID == user.mUID && mGID == user.mGID && mUmask == user.mUmask
            && mAdditionalGIDs == user.mAdditionalGIDs && mUsername == user.mUsername;
    }

    /**
     * Compares user spec.
     *
     * @param user user spec to compare.
     * @return bool.
     */
    bool operator!=(const User& user) const { return !operator==(user); }
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
     * @param capabilities LinuxCapabilities spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxCapabilities& capabilities) const
    {
        return mBounding == capabilities.mBounding && mEffective == capabilities.mEffective
            && mInheritable == capabilities.mInheritable && mPermitted == capabilities.mPermitted
            && mAmbient == capabilities.mAmbient;
    }

    /**
     * Compares LinuxCapabilities spec.
     *
     * @param capabilities LinuxCapabilities spec to compare.
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
     * @param rlimit POSIXRlimit spec to compare.
     * @return bool.
     */
    bool operator==(const POSIXRlimit& rlimit) const
    {
        return mType == rlimit.mType && mHard == rlimit.mHard && mSoft == rlimit.mSoft;
    }

    /**
     * Compares POSIXRlimit spec.
     *
     * @param rlimit POSIXRlimit spec to compare.
     * @return bool.
     */
    bool operator!=(const POSIXRlimit& rlimit) const { return !operator==(rlimit); }
};

/**
 * Process contains information to start a specific application inside the container.
 */
struct Process {
    bool                                                    mTerminal;
    User                                                    mUser;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mArgs;
    EnvVarsArray                                            mEnv;
    StaticString<cMaxParamLen>                              mCwd;
    bool                                                    mNoNewPrivileges;
    Optional<LinuxCapabilities>                             mCapabilities;
    StaticArray<POSIXRlimit, cMaxParamCount>                mRlimits;

    /**
     * Compares process spec.
     *
     * @param process process spec to compare.
     * @return bool.
     */
    bool operator==(const Process& process) const
    {
        return mTerminal == process.mTerminal && mUser == process.mUser && mArgs == process.mArgs
            && mEnv == process.mEnv && mCwd == process.mCwd && mNoNewPrivileges == process.mNoNewPrivileges
            && mCapabilities == process.mCapabilities && mRlimits == process.mRlimits;
    }

    /**
     * Compares process spec.
     *
     * @param process process spec to compare.
     * @return bool.
     */
    bool operator!=(const Process& process) const { return !operator==(process); }
};

/**
 * LinuxDeviceCgroup represents a device rule for the devices specified to the device controller.
 */
struct LinuxDeviceCgroup {
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

    bool                          mAllow;
    StaticString<cDeviceTypeLen>  mType;
    Optional<int64_t>             mMajor;
    Optional<int64_t>             mMinor;
    StaticString<cPermissionsLen> mAccess;

    /**
     * Compares LinuxDeviceCgroup spec.
     *
     * @param deviceCgroup LinuxDeviceCgroup spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxDeviceCgroup& deviceCgroup) const
    {
        return mType == deviceCgroup.mType && mAccess == deviceCgroup.mAccess && mAllow == deviceCgroup.mAllow
            && mMajor == deviceCgroup.mMajor && mMinor == deviceCgroup.mMinor;
    }

    /**
     * Compares LinuxDeviceCgroup spec.
     *
     * @param deviceCgroup LinuxDeviceCgroup spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxDeviceCgroup& deviceCgroup) const { return !operator==(deviceCgroup); }
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
     * @param memory LinuxMemory spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxMemory& memory) const
    {
        return mLimit == memory.mLimit && mReservation == memory.mReservation && mSwap == memory.mSwap
            && mKernel == memory.mKernel && mKernelTCP == memory.mKernelTCP && mSwappiness == memory.mSwappiness
            && mDisableOOMKiller == memory.mDisableOOMKiller && mUseHierarchy == memory.mUseHierarchy
            && mCheckBeforeUpdate == memory.mCheckBeforeUpdate;
    }

    /**
     * Compares LinuxMemory spec.
     *
     * @param memory LinuxMemory spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxMemory& memory) const { return !operator==(memory); }
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
     * @param cpu LinuxCPU spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxCPU& cpu) const
    {
        return mShares == cpu.mShares && mQuota == cpu.mQuota && mBurst == cpu.mBurst && mPeriod == cpu.mPeriod
            && mRealtimeRuntime == cpu.mRealtimeRuntime && mRealtimePeriod == cpu.mRealtimePeriod && mCpus == cpu.mCpus
            && mMems == cpu.mMems && mIdle == cpu.mIdle;
    }

    /**
     * Compares LinuxCPU spec.
     *
     * @param cpu LinuxCPU spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxCPU& cpu) const { return !operator==(cpu); }
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
     * @param resources LinuxResources spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxResources& resources) const { return mDevices == resources.mDevices; }

    /**
     * Compares LinuxResources spec.
     *
     * @param resources LinuxResources spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxResources& resources) const { return !operator==(resources); }
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
        static const char* const sNodeStatusStrings[] = {
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

        return Array<const char* const>(sNodeStatusStrings, ArraySize(sNodeStatusStrings));
    };
};

using LinuxNamespaceEnum = LinuxNamespaceTypeDesc::Enum;
using LinuxNamespaceType = EnumStringer<LinuxNamespaceTypeDesc>;

constexpr auto cMaxNumNamespaces = static_cast<size_t>(LinuxNamespaceEnum::eNumNamespaces);

/**
 * LinuxNamespace is the configuration for a Linux namespace.
 */
struct LinuxNamespace {
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

    LinuxNamespaceType         mType;
    StaticString<cMaxParamLen> mPath;

    /**
     * Compares LinuxNamespace spec.
     *
     * @param ns LinuxNamespace spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxNamespace& ns) const { return mType == ns.mType && mPath == ns.mPath; }

    /**
     * Compares LinuxNamespace spec.
     *
     * @param ns LinuxNamespace spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxNamespace& ns) const { return !operator==(ns); }
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
    Optional<uint32_t>           mUID;
    Optional<uint32_t>           mGID;

    /**
     * Compares LinuxDevice spec.
     *
     * @param device LinuxDevice spec to compare.
     * @return bool.
     */
    bool operator==(const LinuxDevice& device) const
    {
        return mPath == device.mPath && mType == device.mType && mMajor == device.mMajor && mMinor == device.mMinor
            && mFileMode == device.mFileMode && mUID == device.mUID && mGID == device.mGID;
    }

    /**
     * Compares LinuxDevice spec.
     *
     * @param device LinuxDevice spec to compare.
     * @return bool.
     */
    bool operator!=(const LinuxDevice& device) const { return !operator==(device); }
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
     * @param linux Linux spec to compare.
     * @return bool.
     */
    bool operator==(const Linux& linuxSpec) const
    {
        return mSysctl == linuxSpec.mSysctl && mResources == linuxSpec.mResources
            && mCgroupsPath == linuxSpec.mCgroupsPath && mNamespaces == linuxSpec.mNamespaces
            && mMaskedPaths == linuxSpec.mMaskedPaths && mReadonlyPaths == linuxSpec.mReadonlyPaths;
    }

    /**
     * Compares Linux spec.
     *
     * @param linux Linux spec to compare.
     * @return bool.
     */
    bool operator!=(const Linux& linuxSpec) const { return !operator==(linuxSpec); }
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
     * @param hypervisor VMHypervisor spec to compare.
     * @return bool.
     */
    bool operator==(const VMHypervisor& hypervisor) const
    {
        return mPath == hypervisor.mPath && mParameters == hypervisor.mParameters;
    }

    /**
     * Compares VMHypervisor spec.
     *
     * @param hypervisor VMHypervisor spec to compare.
     * @return bool.
     */
    bool operator!=(const VMHypervisor& hypervisor) const { return !operator==(hypervisor); }
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
     * @param kernel VMKernel spec to compare.
     * @return bool.
     */
    bool operator==(const VMKernel& kernel) const { return mPath == kernel.mPath && mParameters == kernel.mParameters; }

    /**
     * Compares VMKernel spec.
     *
     * @param kernel VMKernel spec to compare.
     * @return bool.
     */
    bool operator!=(const VMKernel& kernel) const { return !operator==(kernel); }
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
     * @param iomem IOMEM to compare.
     * @return bool.
     */
    bool operator==(const VMHWConfigIOMEM& iomem) const
    {
        return mFirstGFN == iomem.mFirstGFN && mFirstMFN == iomem.mFirstMFN && mNrMFNs == iomem.mNrMFNs;
    }

    /**
     * Compares IOMEMs.
     *
     * @param iomem IOMEM to compare.
     * @return bool.
     */
    bool operator!=(const VMHWConfigIOMEM& iomem) const { return !operator==(iomem); }
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
     * @param hwConfig VMHWConfig spec to compare.
     * @return bool.
     */
    bool operator==(const VMHWConfig& hwConfig) const
    {
        return mDeviceTree == hwConfig.mDeviceTree && mVCPUs == hwConfig.mVCPUs && mMemKB == hwConfig.mMemKB
            && mDTDevs == hwConfig.mDTDevs && mIOMEMs == hwConfig.mIOMEMs && mIRQs == hwConfig.mIRQs;
    }

    /**
     * Compares VMHWConfig spec.
     *
     * @param hwConfig VMHWConfig spec to compare.
     * @return bool.
     */
    bool operator!=(const VMHWConfig& hwConfig) const { return !operator==(hwConfig); }
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
     * @param vm VM spec to compare.
     * @return bool.
     */
    bool operator==(const VM& vm) const
    {
        return mHypervisor == vm.mHypervisor && mKernel == vm.mKernel && mHWConfig == vm.mHWConfig;
    }

    /**
     * Compares VM spec.
     *
     * @param vm VM spec to compare.
     * @return bool.
     */
    bool operator!=(const VM& vm) const { return !operator==(vm); }
};

/**
 * OCI runtime specification.
 */
struct RuntimeSpec {
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
     * @param spec runtime spec to compare.
     * @return bool.
     */
    bool operator==(const RuntimeSpec& spec) const
    {
        return mOCIVersion == spec.mOCIVersion && mProcess == spec.mProcess && mRoot == spec.mRoot
            && mHostname == spec.mHostname && mMounts == spec.mMounts && mLinux == spec.mLinux && mVM == spec.mVM;
    }

    /**
     * Compares runtime spec.
     *
     * @param spec runtime spec to compare.
     * @return bool.
     */
    bool operator!=(const RuntimeSpec& spec) const { return !operator==(spec); }
};

/**
 * Creates example runtime spec.
 *
 * @param spec runtime spec.
 * @param isCgroup2UnifiedMode adds croup namespace.
 */
inline Error CreateExampleRuntimeSpec(RuntimeSpec& spec, bool isCgroup2UnifiedMode = true)
{
    spec.mOCIVersion = cVersion;

    spec.mRoot.EmplaceValue();

    spec.mRoot->mPath     = "rootfs";
    spec.mRoot->mReadonly = true;

    spec.mProcess.EmplaceValue();

    spec.mProcess->mTerminal = true;
    spec.mProcess->mUser     = {};
    spec.mProcess->mArgs.Clear();
    spec.mProcess->mArgs.PushBack("sh");
    spec.mProcess->mEnv.Clear();
    spec.mProcess->mEnv.PushBack("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    spec.mProcess->mEnv.PushBack("TERM=xterm");
    spec.mProcess->mCwd             = "/";
    spec.mProcess->mNoNewPrivileges = true;

    spec.mProcess->mCapabilities.EmplaceValue();

    spec.mProcess->mCapabilities->mBounding.Clear();
    spec.mProcess->mCapabilities->mBounding.PushBack("CAP_AUDIT_WRITE");
    spec.mProcess->mCapabilities->mBounding.PushBack("CAP_KILL");
    spec.mProcess->mCapabilities->mBounding.PushBack("CAP_NET_BIND_SERVICE");
    spec.mProcess->mCapabilities->mPermitted.Clear();
    spec.mProcess->mCapabilities->mPermitted.PushBack("CAP_AUDIT_WRITE");
    spec.mProcess->mCapabilities->mPermitted.PushBack("CAP_KILL");
    spec.mProcess->mCapabilities->mPermitted.PushBack("CAP_NET_BIND_SERVICE");
    spec.mProcess->mCapabilities->mEffective.Clear();
    spec.mProcess->mCapabilities->mEffective.PushBack("CAP_AUDIT_WRITE");
    spec.mProcess->mCapabilities->mEffective.PushBack("CAP_KILL");
    spec.mProcess->mCapabilities->mEffective.PushBack("CAP_NET_BIND_SERVICE");

    spec.mProcess->mRlimits.Clear();
    spec.mProcess->mRlimits.PushBack({"RLIMIT_NOFILE", 1024, 1024});

    spec.mHostname = "runc";

    spec.mMounts.Clear();
    spec.mMounts.EmplaceBack("proc", "/proc", "proc");
    spec.mMounts.EmplaceBack("tmpfs", "/dev", "tmpfs", "nosuid,strictatime,mode=755,size=65536k");
    spec.mMounts.EmplaceBack("devpts", "/dev/pts", "devpts", "nosuid,noexec,newinstance,ptmxmode=0666,mode=0620,gid=5");
    spec.mMounts.EmplaceBack("shm", "/dev/shm", "tmpfs", "nosuid,noexec,nodev,mode=1777,size=65536k");
    spec.mMounts.EmplaceBack("mqueue", "/dev/mqueue", "mqueue", "nosuid,noexec,nodev");
    spec.mMounts.EmplaceBack("sysfs", "/sys", "sysfs", "nosuid,noexec,nodev,ro");
    spec.mMounts.EmplaceBack("cgroup", "/sys/fs/cgroup", "cgroup", "nosuid,noexec,nodev,relatime,ro");

    spec.mLinux.EmplaceValue();

    spec.mLinux->mMaskedPaths.Clear();
    spec.mLinux->mMaskedPaths.PushBack("/proc/acpi");
    spec.mLinux->mMaskedPaths.PushBack("/proc/asound");
    spec.mLinux->mMaskedPaths.PushBack("/proc/kcore");
    spec.mLinux->mMaskedPaths.PushBack("/proc/keys");
    spec.mLinux->mMaskedPaths.PushBack("/proc/latency_stats");
    spec.mLinux->mMaskedPaths.PushBack("/proc/timer_list");
    spec.mLinux->mMaskedPaths.PushBack("/proc/timer_stats");
    spec.mLinux->mMaskedPaths.PushBack("/proc/sched_debug");
    spec.mLinux->mMaskedPaths.PushBack("/proc/scsi");
    spec.mLinux->mMaskedPaths.PushBack("/sys/firmware");

    spec.mLinux->mReadonlyPaths.Clear();
    spec.mLinux->mReadonlyPaths.PushBack("/proc/bus");
    spec.mLinux->mReadonlyPaths.PushBack("/proc/fs");
    spec.mLinux->mReadonlyPaths.PushBack("/proc/irq");
    spec.mLinux->mReadonlyPaths.PushBack("/proc/sys");
    spec.mLinux->mReadonlyPaths.PushBack("/proc/sysrq-trigger");

    spec.mLinux->mResources.EmplaceValue();

    spec.mLinux->mResources->mDevices.Clear();
    spec.mLinux->mResources->mDevices.EmplaceBack("", "rwm", false);

    spec.mLinux->mNamespaces.Clear();
    spec.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::ePID);
    spec.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eNetwork);
    spec.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eIPC);
    spec.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eUTS);
    spec.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eMount);

    if (isCgroup2UnifiedMode) {
        spec.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eCgroup);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::oci

#endif
