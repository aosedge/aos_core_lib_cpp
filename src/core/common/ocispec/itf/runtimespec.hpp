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
    friend bool operator==(const Root& lhs, const Root& rhs)
    {
        return lhs.mPath == rhs.mPath && lhs.mReadonly == rhs.mReadonly;
    };

    /**
     * Compares root spec.
     *
     * @param rhs root spec to compare.
     * @return bool.
     */
    friend bool operator!=(const Root& lhs, const Root& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const User& lhs, const User& rhs)
    {
        return lhs.mUID == rhs.mUID && lhs.mGID == rhs.mGID && lhs.mUmask == rhs.mUmask
            && lhs.mAdditionalGIDs == rhs.mAdditionalGIDs && lhs.mUsername == rhs.mUsername;
    };

    /**
     * Compares user spec.
     *
     * @param rhs user spec to compare.
     * @return bool.
     */
    friend bool operator!=(const User& lhs, const User& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const LinuxCapabilities& lhs, const LinuxCapabilities& rhs)
    {
        return lhs.mBounding == rhs.mBounding && lhs.mEffective == rhs.mEffective
            && lhs.mInheritable == rhs.mInheritable && lhs.mPermitted == rhs.mPermitted && lhs.mAmbient == rhs.mAmbient;
    };

    /**
     * Compares LinuxCapabilities spec.
     *
     * @param rhs LinuxCapabilities spec to compare.
     * @return bool.
     */
    friend bool operator!=(const LinuxCapabilities& lhs, const LinuxCapabilities& capabilities)
    {
        return !(lhs == capabilities);
    };
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
    friend bool operator==(const POSIXRlimit& lhs, const POSIXRlimit& rhs)
    {
        return lhs.mType == rhs.mType && lhs.mHard == rhs.mHard && lhs.mSoft == rhs.mSoft;
    };

    /**
     * Compares POSIXRlimit spec.
     *
     * @param rhs POSIXRlimit spec to compare.
     * @return bool.
     */
    friend bool operator!=(const POSIXRlimit& lhs, const POSIXRlimit& rhs) { return !(lhs == rhs); };
};

/**
 * Process contains information to start a specific application inside the container.
 */
struct Process {
    bool                                                       mTerminal;
    User                                                       mUser;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount>    mArgs;
    StaticArray<StaticString<cEnvVarLen>, cMaxNumEnvVariables> mEnv;
    StaticString<cMaxParamLen>                                 mCwd;
    bool                                                       mNoNewPrivileges;
    Optional<LinuxCapabilities>                                mCapabilities;
    StaticArray<POSIXRlimit, cMaxParamCount>                   mRlimits;

    /**
     * Compares process spec.
     *
     * @param rhs process spec to compare.
     * @return bool.
     */
    friend bool operator==(const Process& lhs, const Process& rhs)
    {
        return lhs.mTerminal == rhs.mTerminal && lhs.mUser == rhs.mUser && lhs.mArgs == rhs.mArgs
            && lhs.mEnv == rhs.mEnv && lhs.mCwd == rhs.mCwd && lhs.mNoNewPrivileges == rhs.mNoNewPrivileges
            && lhs.mCapabilities == rhs.mCapabilities && lhs.mRlimits == rhs.mRlimits;
    };

    /**
     * Compares process spec.
     *
     * @param rhs process spec to compare.
     * @return bool.
     */
    friend bool operator!=(const Process& lhs, const Process& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const LinuxDeviceCgroup& lhs, const LinuxDeviceCgroup& rhs)
    {
        return lhs.mType == rhs.mType && lhs.mAccess == rhs.mAccess && lhs.mAllow == rhs.mAllow
            && lhs.mMajor == rhs.mMajor && lhs.mMinor == rhs.mMinor;
    };

    /**
     * Compares LinuxDeviceCgroup spec.
     *
     * @param rhs LinuxDeviceCgroup spec to compare.
     * @return bool.
     */
    friend bool operator!=(const LinuxDeviceCgroup& lhs, const LinuxDeviceCgroup& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const LinuxMemory& lhs, const LinuxMemory& rhs)
    {
        return lhs.mLimit == rhs.mLimit && lhs.mReservation == rhs.mReservation && lhs.mSwap == rhs.mSwap
            && lhs.mKernel == rhs.mKernel && lhs.mKernelTCP == rhs.mKernelTCP && lhs.mSwappiness == rhs.mSwappiness
            && lhs.mDisableOOMKiller == rhs.mDisableOOMKiller && lhs.mUseHierarchy == rhs.mUseHierarchy
            && lhs.mCheckBeforeUpdate == rhs.mCheckBeforeUpdate;
    };

    /**
     * Compares LinuxMemory spec.
     *
     * @param rhs LinuxMemory spec to compare.
     * @return bool.
     */
    friend bool operator!=(const LinuxMemory& lhs, const LinuxMemory& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const LinuxCPU& lhs, const LinuxCPU& rhs)
    {
        return lhs.mShares == rhs.mShares && lhs.mQuota == rhs.mQuota && lhs.mBurst == rhs.mBurst
            && lhs.mPeriod == rhs.mPeriod && lhs.mRealtimeRuntime == rhs.mRealtimeRuntime
            && lhs.mRealtimePeriod == rhs.mRealtimePeriod && lhs.mCpus == rhs.mCpus && lhs.mMems == rhs.mMems
            && lhs.mIdle == rhs.mIdle;
    };

    /**
     * Compares LinuxCPU spec.
     *
     * @param rhs LinuxCPU spec to compare.
     * @return bool.
     */
    friend bool operator!=(const LinuxCPU& lhs, const LinuxCPU& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const LinuxResources& lhs, const LinuxResources& rhs)
    {
        return lhs.mDevices == rhs.mDevices;
    };

    /**
     * Compares LinuxResources spec.
     *
     * @param rhs LinuxResources spec to compare.
     * @return bool.
     */
    friend bool operator!=(const LinuxResources& lhs, const LinuxResources& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const LinuxNamespace& lhs, const LinuxNamespace& rhs)
    {
        return lhs.mType == rhs.mType && lhs.mPath == rhs.mPath;
    };

    /**
     * Compares LinuxNamespace spec.
     *
     * @param rhs LinuxNamespace spec to compare.
     * @return bool.
     */
    friend bool operator!=(const LinuxNamespace& lhs, const LinuxNamespace& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const LinuxDevice& lhs, const LinuxDevice& rhs)
    {
        return lhs.mPath == rhs.mPath && lhs.mType == rhs.mType && lhs.mMajor == rhs.mMajor && lhs.mMinor == rhs.mMinor
            && lhs.mFileMode == rhs.mFileMode && lhs.mUID == rhs.mUID && lhs.mGID == rhs.mGID;
    };

    /**
     * Compares LinuxDevice spec.
     *
     * @param rhs LinuxDevice spec to compare.
     * @return bool.
     */
    friend bool operator!=(const LinuxDevice& lhs, const LinuxDevice& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const Linux& lhs, const Linux& rhs)
    {
        return lhs.mSysctl == rhs.mSysctl && lhs.mResources == rhs.mResources && lhs.mCgroupsPath == rhs.mCgroupsPath
            && lhs.mNamespaces == rhs.mNamespaces && lhs.mMaskedPaths == rhs.mMaskedPaths
            && lhs.mReadonlyPaths == rhs.mReadonlyPaths;
    };

    /**
     * Compares Linux spec.
     *
     * @param rhs Linux spec to compare.
     * @return bool.
     */
    friend bool operator!=(const Linux& lhs, const Linux& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const VMHypervisor& lhs, const VMHypervisor& rhs)
    {
        return lhs.mPath == rhs.mPath && lhs.mParameters == rhs.mParameters;
    };

    /**
     * Compares VMHypervisor spec.
     *
     * @param rhs VMHypervisor spec to compare.
     * @return bool.
     */
    friend bool operator!=(const VMHypervisor& lhs, const VMHypervisor& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const VMKernel& lhs, const VMKernel& rhs)
    {
        return lhs.mPath == rhs.mPath && lhs.mParameters == rhs.mParameters;
    };

    /**
     * Compares VMKernel spec.
     *
     * @param rhs VMKernel spec to compare.
     * @return bool.
     */
    friend bool operator!=(const VMKernel& lhs, const VMKernel& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const VMHWConfigIOMEM& lhs, const VMHWConfigIOMEM& rhs)
    {
        return lhs.mFirstGFN == rhs.mFirstGFN && lhs.mFirstMFN == rhs.mFirstMFN && lhs.mNrMFNs == rhs.mNrMFNs;
    };

    /**
     * Compares IOMEMs.
     *
     * @param rhs IOMEM to compare.
     * @return bool.
     */
    friend bool operator!=(const VMHWConfigIOMEM& lhs, const VMHWConfigIOMEM& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const VMHWConfig& lhs, const VMHWConfig& rhs)
    {
        return lhs.mDeviceTree == rhs.mDeviceTree && lhs.mVCPUs == rhs.mVCPUs && lhs.mMemKB == rhs.mMemKB
            && lhs.mDTDevs == rhs.mDTDevs && lhs.mIOMEMs == rhs.mIOMEMs && lhs.mIRQs == rhs.mIRQs;
    };

    /**
     * Compares VMHWConfig spec.
     *
     * @param rhs VMHWConfig spec to compare.
     * @return bool.
     */
    friend bool operator!=(const VMHWConfig& lhs, const VMHWConfig& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const VM& lhs, const VM& rhs)
    {
        return lhs.mHypervisor == rhs.mHypervisor && lhs.mKernel == rhs.mKernel && lhs.mHWConfig == rhs.mHWConfig;
    };

    /**
     * Compares VM spec.
     *
     * @param rhs VM spec to compare.
     * @return bool.
     */
    friend bool operator!=(const VM& lhs, const VM& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const RuntimeConfig& lhs, const RuntimeConfig& rhs)
    {
        return lhs.mOCIVersion == rhs.mOCIVersion && lhs.mProcess == rhs.mProcess && lhs.mRoot == rhs.mRoot
            && lhs.mHostname == rhs.mHostname && lhs.mMounts == rhs.mMounts && lhs.mLinux == rhs.mLinux
            && lhs.mVM == rhs.mVM;
    };

    /**
     * Compares runtime spec.
     *
     * @param rhs runtime spec to compare.
     * @return bool.
     */
    friend bool operator!=(const RuntimeConfig& lhs, const RuntimeConfig& rhs) { return !(lhs == rhs); };
};

/**
 * Creates example runtime spec.
 *
 * @param config runtime spec.
 * @param isCgroup2UnifiedMode adds croup namespace.
 * @return Error.
 */
inline Error CreateExampleRuntimeConfig(RuntimeConfig& config, bool isCgroup2UnifiedMode = true)
{
    config.mOCIVersion = cVersion;

    config.mRoot.EmplaceValue();

    config.mRoot->mPath     = "rootfs";
    config.mRoot->mReadonly = true;

    config.mProcess.EmplaceValue();

    config.mProcess->mTerminal = true;
    config.mProcess->mUser     = {};
    config.mProcess->mArgs.Clear();

    if (auto err = config.mProcess->mArgs.PushBack("sh"); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    config.mProcess->mEnv.Clear();

    constexpr const char* cDefaultEnv[]
        = {"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", "TERM=xterm"};

    for (const auto* env : cDefaultEnv) {
        if (auto err = config.mProcess->mEnv.PushBack(env); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    config.mProcess->mCwd             = "/";
    config.mProcess->mNoNewPrivileges = true;

    config.mProcess->mCapabilities.EmplaceValue();

    constexpr const char* cDefaultCaps[] = {"CAP_AUDIT_WRITE", "CAP_KILL", "CAP_NET_BIND_SERVICE"};

    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount>* capsList[] = {
        &config.mProcess->mCapabilities->mBounding,
        &config.mProcess->mCapabilities->mPermitted,
        &config.mProcess->mCapabilities->mEffective,
    };

    for (auto* caps : capsList) {
        caps->Clear();

        for (const auto* cap : cDefaultCaps) {
            if (auto err = caps->PushBack(cap); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    config.mProcess->mRlimits.Clear();

    if (auto err = config.mProcess->mRlimits.PushBack({"RLIMIT_NOFILE", 1024, 1024}); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    config.mHostname = "runc";

    config.mMounts.Clear();

    constexpr struct {
        const char* mSource;
        const char* mDestination;
        const char* mType;
        const char* mOptions;
    } cDefaultMounts[] = {
        {"proc", "/proc", "proc", ""},
        {"tmpfs", "/dev", "tmpfs", "nosuid,strictatime,mode=755,size=65536k"},
        {"devpts", "/dev/pts", "devpts", "nosuid,noexec,newinstance,ptmxmode=0666,mode=0620,gid=5"},
        {"shm", "/dev/shm", "tmpfs", "nosuid,noexec,nodev,mode=1777,size=65536k"},
        {"mqueue", "/dev/mqueue", "mqueue", "nosuid,noexec,nodev"},
        {"sysfs", "/sys", "sysfs", "nosuid,noexec,nodev,ro"},
        {"cgroup", "/sys/fs/cgroup", "cgroup", "nosuid,noexec,nodev,relatime,ro"},
    };

    for (const auto& mount : cDefaultMounts) {
        if (auto err = config.mMounts.EmplaceBack(mount.mSource, mount.mDestination, mount.mType, mount.mOptions);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    config.mLinux.EmplaceValue();

    constexpr const char* cMaskedPaths[] = {
        "/proc/acpi",
        "/proc/asound",
        "/proc/kcore",
        "/proc/keys",
        "/proc/latency_stats",
        "/proc/timer_list",
        "/proc/timer_stats",
        "/proc/sched_debug",
        "/proc/scsi",
        "/sys/firmware",
    };

    config.mLinux->mMaskedPaths.Clear();

    for (const auto* path : cMaskedPaths) {
        if (auto err = config.mLinux->mMaskedPaths.PushBack(path); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    constexpr const char* cReadonlyPaths[] = {"/proc/bus", "/proc/fs", "/proc/irq", "/proc/sys", "/proc/sysrq-trigger"};

    config.mLinux->mReadonlyPaths.Clear();

    for (const auto* path : cReadonlyPaths) {
        if (auto err = config.mLinux->mReadonlyPaths.PushBack(path); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    config.mLinux->mResources.EmplaceValue();

    config.mLinux->mResources->mDevices.Clear();

    if (auto err = config.mLinux->mResources->mDevices.EmplaceBack("", "rwm", false); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    constexpr LinuxNamespaceType cDefaultNamespaces[] = {
        LinuxNamespaceEnum::ePID,
        LinuxNamespaceEnum::eNetwork,
        LinuxNamespaceEnum::eIPC,
        LinuxNamespaceEnum::eUTS,
        LinuxNamespaceEnum::eMount,
    };

    config.mLinux->mNamespaces.Clear();

    for (const auto& nsType : cDefaultNamespaces) {
        if (auto err = config.mLinux->mNamespaces.EmplaceBack(nsType); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (isCgroup2UnifiedMode) {
        if (auto err = config.mLinux->mNamespaces.EmplaceBack(LinuxNamespaceEnum::eCgroup); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::oci

#endif
