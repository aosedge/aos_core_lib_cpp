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
 * Max media type len.
 */
constexpr auto cMaxMediaTypeLen = AOS_CONFIG_OCISPEC_MEDIA_TYPE_LEN;

/**
 * Max digest len.
 *
 */
constexpr auto cMaxDigestLen = AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE;

/**
 * Spec parameter max len.
 */
constexpr auto cMaxParamLen = AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_LEN;

/**
 * Spec parameter max count.
 */
constexpr auto cMaxParamCount = AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_COUNT;

/**
 * Version max len.
 */
constexpr auto cVersionLen = AOS_CONFIG_TYPES_VERSION_LEN;

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
 * OCI content descriptor.
 */

struct ContentDescriptor {
    StaticString<cMaxMediaTypeLen> mMediaType;
    StaticString<cMaxDigestLen>    mDigest;
    int64_t                        mSize;

    /**
     * Compares content descriptor.
     *
     * @param descriptor content descriptor to compare.
     * @return bool.
     */
    bool operator==(const ContentDescriptor& descriptor) const
    {
        return mMediaType == descriptor.mMediaType && mDigest == descriptor.mDigest && mSize == descriptor.mSize;
    }

    /**
     * Compares content descriptor.
     *
     * @param descriptor content descriptor to compare.
     * @return bool.
     */
    bool operator!=(const ContentDescriptor& descriptor) const { return !operator==(descriptor); }
};

/**
 * OCI image manifest.
 */
struct ImageManifest {
    int                                           mSchemaVersion;
    StaticString<cMaxMediaTypeLen>                mMediaType;
    ContentDescriptor                             mConfig;
    StaticArray<ContentDescriptor, cMaxNumLayers> mLayers;
    ContentDescriptor*                            mAosService;

    /**
     * Compares image manifest.
     *
     * @param manifest manifest to compare.
     * @return bool.
     */
    bool operator==(const ImageManifest& manifest) const
    {
        return mSchemaVersion == manifest.mSchemaVersion && mMediaType == manifest.mMediaType
            && mConfig == manifest.mConfig && mLayers == manifest.mLayers
            && ((!mAosService && !manifest.mAosService)
                || (mAosService && manifest.mAosService && *mAosService == *manifest.mAosService));
    }

    /**
     * Compares image manifest.
     *
     * @param manifest manifest to compare.
     * @return bool.
     */
    bool operator!=(const ImageManifest& manifest) const { return !operator==(manifest); }
};

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

} // namespace oci

/**
 * OCI spec interface.
 */
class OCISpecItf {
public:
    /**
     * Loads OCI image manifest.
     *
     * @param path file path.
     * @param manifest image manifest.
     * @return Error.
     */
    virtual Error LoadImageManifest(const String& path, oci::ImageManifest& manifest) = 0;

    /**
     * Saves OCI image manifest.
     *
     * @param path file path.
     * @param manifest image manifest.
     * @return Error.
     */
    virtual Error SaveImageManifest(const String& path, const oci::ImageManifest& manifest) = 0;

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
