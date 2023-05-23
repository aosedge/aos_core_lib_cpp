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
};

/**
 * OCI image specification.
 */
struct ImageSpec {
    ImageConfig mConfig;
};

/**
 * Contains information about the hypervisor to use for a virtual machine.
 */
struct VMHypervisor {
    StaticString<cFilePathLen>                              mPath;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mParameters;
};

/**
 * Contains information about the kernel to use for a virtual machine.
 */
struct VMKernel {
    StaticString<cFilePathLen>                              mPath;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mParameters;
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
};

/**
 * Contains information for virtual-machine-based containers.
 */
struct VM {
    VMHypervisor mHypervisor;
    VMKernel     mKernel;
    VMHWConfig   mHWConfig;
};

/**
 * OCI runtime specification.
 */
struct RuntimeSpec {
    StaticString<cSpecVersionLen> mOCIVersion;
    VM*                           mVM;
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
