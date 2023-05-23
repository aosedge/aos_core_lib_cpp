/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OCISPEC_HPP_
#define AOS_OCISPEC_HPP_

#include "aos/common/tools/string.hpp"
#include "aos/common/types.hpp"

namespace aos {
namespace oci {

/**
 * Spec parameter max len.
 */
constexpr auto cMaxParamLen = 64;

/**
 * Spec parameter max count.
 */
constexpr auto cMaxParamCount = 8;

/**
 * Spec version max len.
 */
constexpr auto cSpecVersionLen = 32;

/**
 * Max DT devices count.
 */
constexpr auto cMaxDTDevsCount = 20;

/**
 * Max DT device name length.
 */
constexpr auto cMaxDTDevLen = 32;

/**
 * Max IOMEMs count.
 */
constexpr auto cMaxIOMEMsCount = 20;

/**
 * Max IRQs count.
 */
constexpr auto cMaxIRQsCount = 20;

/**
 * OCI image config.
 */
struct ImageConfig {
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mEnv;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mEntryPoint;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mCmd;

    /**
     * Compares image config.
     *
     * @param config image config to compare.
     * @return bool.
     */
    bool operator==(const ImageConfig& config) const
    {
        return mEnv == config.mEnv && mEntryPoint == config.mEntryPoint && mCmd == config.mCmd;
    }

    /**
     * Compares image config.
     *
     * @param config image config to compare.
     * @return bool.
     */
    bool operator!=(const ImageConfig& config) const { return !operator==(config); }
};

/**
 * OCI image specification.
 */
struct ImageSpec {
    ImageConfig mConfig;

    /**
     * Compares image spec.
     *
     * @param spec image spec to compare.
     * @return bool.
     */
    bool operator==(const ImageSpec& spec) const { return mConfig == spec.mConfig; }

    /**
     * Compares image spec.
     *
     * @param spec image spec to compare.
     * @return bool.
     */
    bool operator!=(const ImageSpec& spec) const { return !operator==(spec); }
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
    StaticString<cSpecVersionLen> mOCIVersion;
    VM*                           mVM;

    /**
     * Compares runtime spec.
     *
     * @param spec runtime spec to compare.
     * @return bool.
     */
    bool operator==(const RuntimeSpec& spec) const
    {
        if (mOCIVersion != spec.mOCIVersion) {
            return false;
        }

        if ((mVM != nullptr && spec.mVM == nullptr) || (mVM == nullptr && spec.mVM != nullptr)) {
            return false;
        }

        if ((mVM != nullptr && spec.mVM != nullptr) && (*mVM != *spec.mVM)) {
            return false;
        }

        return true;
    }

    /**
     * Compares runtime spec.
     *
     * @param spec runtime spec to compare.
     * @return bool.
     */
    bool operator!=(const RuntimeSpec& spec) const { return !operator==(spec); }
};

} // namespace oci

/**
 * OCI spec interface.
 */
class OCISpecItf {
public:
    /**
     * Loads OCI image spec.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    virtual Error LoadImageSpec(const String& path, oci::ImageSpec& imageSpec) = 0;

    /**
     * Saves OCI image spec.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    virtual Error SaveImageSpec(const String& path, const oci::ImageSpec& imageSpec) = 0;

    /**
     * Loads OCI runtime spec.
     *
     * @param path file path.
     * @param runtimeSpec runtime spec.
     * @return Error.
     */
    virtual Error LoadRuntimeSpec(const String& path, oci::RuntimeSpec& runtimeSpec) = 0;

    /**
     * Saves OCI runtime spec.
     *
     * @param path file path.
     * @param runtimeSpec runtime spec.
     * @return Error.
     */
    virtual Error SaveRuntimeSpec(const String& path, const oci::RuntimeSpec& runtimeSpec) = 0;

    /**
     * Destroys OCI spec interface.
     */
    virtual ~OCISpecItf() = default;
};

} // namespace aos

#endif
