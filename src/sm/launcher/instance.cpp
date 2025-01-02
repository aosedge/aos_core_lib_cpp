/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/launcher/instance.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/memory.hpp"
#include "log.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

StaticAllocator<Instance::cAllocatorSize, Instance::cNumAllocations> Instance::sAllocator {};

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Instance::Instance(const Config& config, const InstanceInfo& instanceInfo, const String& instanceID,
    servicemanager::ServiceManagerItf& serviceManager, layermanager::LayerManagerItf& layerManager,
    networkmanager::NetworkManagerItf& networkmanager, runner::RunnerItf& runner, RuntimeItf& runtime,
    monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager, const String& hostWhiteoutsDir)
    : mConfig(config)
    , mInstanceID(instanceID)
    , mInstanceInfo(instanceInfo)
    , mServiceManager(serviceManager)
    , mLayerManager(layerManager)
    , mNetworkManager(networkmanager)
    , mRunner(runner)
    , mRuntime(runtime)
    , mResourceMonitor(resourceMonitor)
    , mOCIManager(ociManager)
    , mHostWhiteoutsDir(hostWhiteoutsDir)
    , mRuntimeDir(FS::JoinPath(cRuntimeDir, mInstanceID))
{
    LOG_INF() << "Create instance: ident=" << mInstanceInfo.mInstanceIdent << ", instanceID=" << *this;
}

void Instance::SetService(const servicemanager::ServiceData* service)
{
    mService = service;

    if (mService) {
        LOG_DBG() << "Set service for instance: serviceID=" << service->mServiceID << ", version=" << mService->mVersion
                  << ", instanceID=" << *this;
    }
}

Error Instance::Start()
{
    LOG_INF() << "Start instance: instanceID=" << *this << ", runtimeDir=" << mRuntimeDir;

    if (!mService) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "service not found"));
    }

    if (auto err = FS::ClearDir(mRuntimeDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetupNetwork(); !err.IsNone()) {
        return err;
    }

    auto runtimeSpec = MakeUnique<oci::RuntimeSpec>(&sAllocator);
    auto imageParts  = MakeUnique<image::ImageParts>(&sAllocator);

    if (auto err = mServiceManager.GetImageParts(*mService, *imageParts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = CreateRuntimeSpec(*imageParts, *runtimeSpec); !err.IsNone()) {
        return err;
    }

    if (auto err = PrepareRootFS(*imageParts, *runtimeSpec); !err.IsNone()) {
        return err;
    }

    if (auto err = mOCIManager.SaveRuntimeSpec(FS::JoinPath(mRuntimeDir, cRuntimeSpecFile), *runtimeSpec);
        !err.IsNone()) {
        return err;
    }

    auto runStatus = mRunner.StartInstance(mInstanceID, mRuntimeDir, {});

    mRunState = runStatus.mState;

    if (!runStatus.mError.IsNone()) {
        return AOS_ERROR_WRAP(runStatus.mError);
    }

    if (auto err = SetupMonitoring(); !err.IsNone()) {
        return err;
    }

    return aos::ErrorEnum::eNone;
}

Error Instance::Stop()
{
    Error stopErr;

    LOG_INF() << "Stop instance: instanceID=" << *this;

    if (!mService && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "service not found"));
    }

    if (auto err = mRunner.StopInstance(mInstanceID); !err.IsNone() && stopErr.IsNone()) {
        stopErr = err;
    }

    if (auto err = mResourceMonitor.StopInstanceMonitoring(mInstanceID); !err.IsNone() && stopErr.IsNone()) {
        stopErr = err;
    }

    if (mService) {
        if (auto err = mNetworkManager.RemoveInstanceFromNetwork(mInstanceID, mService->mProviderID);
            !err.IsNone() && stopErr.IsNone()) {
            stopErr = err;
        }
    }

    if (auto err = mRuntime.ReleaseServiceRootFS(mRuntimeDir); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    if (auto err = FS::RemoveAll(mRuntimeDir); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    return stopErr;
}

void Instance::ShowAllocatorStats()
{
    LOG_DBG() << "Instances allocator: size=" << sAllocator.MaxSize()
              << ", maxAllocated=" << sAllocator.MaxAllocatedSize();
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Instance::SetupNetwork()
{
    LOG_DBG() << "Setup network: instanceID=" << *this;

    auto networkParams = MakeUnique<networkmanager::NetworkParams>(&sAllocator);

    networkParams->mInstanceIdent      = mInstanceInfo.mInstanceIdent;
    networkParams->mHostsFilePath      = FS::JoinPath(mRuntimeDir, cMountPointsDir, "etc", "hosts");
    networkParams->mResolvConfFilePath = FS::JoinPath(mRuntimeDir, cMountPointsDir, "etc", "resolv.conf");
    networkParams->mHosts              = mConfig.mHosts;
    networkParams->mNetworkParameters  = mInstanceInfo.mNetworkParameters;

    if (auto err = mNetworkManager.AddInstanceToNetwork(mInstanceID, mService->mProviderID, *networkParams);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return aos::ErrorEnum::eNone;
}

Error Instance::CreateRuntimeSpec(const image::ImageParts& imageParts, oci::RuntimeSpec& runtimeSpec)
{
    LOG_DBG() << "Create runtime spec: instanceID=" << *this;

    auto imageSpec = MakeUnique<oci::ImageSpec>(&sAllocator);

    if (auto err = mOCIManager.LoadImageSpec(imageParts.mImageConfigPath, *imageSpec); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    runtimeSpec.mVM.EmplaceValue();

    if (imageSpec->mConfig.mEntryPoint.Size() == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    // Set default HW config values. Normally they should be taken from service config.
    runtimeSpec.mVM->mHWConfig.mVCPUs = 1;
    // For xen this value should be aligned to 1024Kb
    runtimeSpec.mVM->mHWConfig.mMemKB = 8192;

    runtimeSpec.mVM->mKernel.mPath       = FS::JoinPath(imageParts.mServiceFSPath, imageSpec->mConfig.mEntryPoint[0]);
    runtimeSpec.mVM->mKernel.mParameters = imageSpec->mConfig.mCmd;

    LOG_DBG() << "Unikernel path: path=" << runtimeSpec.mVM->mKernel.mPath;

    return ErrorEnum::eNone;
}

Error Instance::SetupMonitoring()
{
    LOG_DBG() << "Setup monitoring: instanceID=" << *this;

    auto monitoringParms = MakeUnique<monitoring::InstanceMonitorParams>(&sAllocator);

    monitoringParms->mInstanceIdent = mInstanceInfo.mInstanceIdent;

    if (auto err = mResourceMonitor.StartInstanceMonitoring(mInstanceID, *monitoringParms); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Instance::PrepareRootFS(const image::ImageParts& imageParts, oci::RuntimeSpec& runtimeSpec)
{
    LOG_DBG() << "Prepare root FS: instanceID=" << *this;

    auto layers = MakeUnique<LayersStaticArray>(&sAllocator);

    if (auto err = layers->PushBack(imageParts.mServiceFSPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto layerData = MakeUnique<layermanager::LayerData>(&sAllocator);

    for (const auto& digest : imageParts.mLayerDigests) {
        if (auto err = mLayerManager.GetLayer(digest, *layerData); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = layers->PushBack(layerData->mPath); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = layers->PushBack(mHostWhiteoutsDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mRuntime.PrepareServiceRootFS(FS::JoinPath(mRuntimeDir, cRootFSDir),
            FS::JoinPath(mRuntimeDir, cMountPointsDir), runtimeSpec.mMounts, *layers);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
