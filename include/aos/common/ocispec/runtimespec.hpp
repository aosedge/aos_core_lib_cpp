/*
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RUNTIMESPEC_HPP_
#define AOS_RUNTIMESPEC_HPP_

#include "aos/common/ocispec/common.hpp"
#include "aos/common/types.hpp"

namespace aos::oci {
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
    StaticString<cVersionLen> mOCIVersion;
    VM*                       mVM;

    /**
     * Compares runtime spec.
     *
     * @param spec runtime spec to compare.
     * @return bool.
     */
    bool operator==(const RuntimeSpec& spec) const
    {
        return mOCIVersion == spec.mOCIVersion && ((!mVM && !spec.mVM) || (mVM && spec.mVM && *mVM == *spec.mVM));
    }

    /**
     * Compares runtime spec.
     *
     * @param spec runtime spec to compare.
     * @return bool.
     */
    bool operator!=(const RuntimeSpec& spec) const { return !operator==(spec); }
};

} // namespace aos::oci

#endif
