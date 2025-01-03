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
#include "runtimespec.hpp"
namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {
static const char* const cBindEtcEntries[] = {"nsswitch.conf", "ssl"};
}

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

    auto imageParts = MakeUnique<image::ImageParts>(&sAllocator);

    if (auto err = mServiceManager.GetImageParts(*mService, *imageParts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto runtimeSpec = MakeUnique<oci::RuntimeSpec>(&sAllocator);

    if (auto err = CreateRuntimeSpec(*imageParts, *runtimeSpec); !err.IsNone()) {
        return err;
    }

    if (auto err = PrepareRootFS(*imageParts, runtimeSpec->mMounts); !err.IsNone()) {
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

    return ErrorEnum::eNone;
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

Error Instance::BindHostDirs(oci::RuntimeSpec& runtimeSpec)
{
    for (const auto& hostEntry : cBindEtcEntries) {
        auto path  = FS::JoinPath("/etc", hostEntry);
        auto mount = MakeShared<oci::Mount>(&sAllocator, path, path, "bind", "bind,ro");

        if (auto err = AddMount(*mount, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Instance::CreateAosEnvVars(oci::RuntimeSpec& runtimeSpec)
{
    auto envVars = MakeUnique<StaticArray<StaticString<cEnvVarNameLen>, cMaxNumEnvVariables>>(&sAllocator);
    StaticString<cEnvVarNameLen> envVar;

    if (auto err = envVar.Format("%s=%s", cEnvAosServiceID, mService->mServiceID.CStr()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = envVars->PushBack(envVar); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = envVar.Format("%s=%s", cEnvAosSubjectID, mInstanceInfo.mInstanceIdent.mSubjectID.CStr());
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = envVars->PushBack(envVar); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = envVar.Format("%s=%d", cEnvAosInstanceIndex, mInstanceInfo.mInstanceIdent.mInstance);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = envVars->PushBack(envVar); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = envVar.Format("%s=%s", cEnvAosInstanceID, mInstanceID.CStr()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = envVars->PushBack(envVar); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = AddEnvVars(*envVars, runtimeSpec); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error Instance::ApplyImageConfig(const oci::ImageSpec& imageSpec, oci::RuntimeSpec& runtimeSpec)
{
    StaticString<cOSTypeLen> os = imageSpec.mOS;

    if (os.ToLower() != cLinuxOS) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotSupported, "unsupported OS in image config"));
    }

    runtimeSpec.mProcess->mArgs.Clear();

    for (const auto& arg : imageSpec.mConfig.mEntryPoint) {
        if (auto err = runtimeSpec.mProcess->mArgs.PushBack(arg); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& arg : imageSpec.mConfig.mCmd) {
        if (auto err = runtimeSpec.mProcess->mArgs.PushBack(arg); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    runtimeSpec.mProcess->mCwd = imageSpec.mConfig.mWorkingDir;

    if (runtimeSpec.mProcess->mCwd.IsEmpty()) {
        runtimeSpec.mProcess->mCwd = "/";
    }

    if (auto err = AddEnvVars(imageSpec.mConfig.mEnv, runtimeSpec); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Instance::CreateVMSpec(
    const String& serviceFSPath, const oci::ImageSpec& imageSpec, oci::RuntimeSpec& runtimeSpec)
{
    LOG_DBG() << "Create VM runtime spec: instanceID=" << *this;

    runtimeSpec.mVM.EmplaceValue();

    if (imageSpec.mConfig.mEntryPoint.Size() == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    // Set default HW config values. Normally they should be taken from service config.
    runtimeSpec.mVM->mHWConfig.mVCPUs = 1;
    // For xen this value should be aligned to 1024Kb
    runtimeSpec.mVM->mHWConfig.mMemKB = 8192;

    runtimeSpec.mVM->mKernel.mPath       = FS::JoinPath(serviceFSPath, imageSpec.mConfig.mEntryPoint[0]);
    runtimeSpec.mVM->mKernel.mParameters = imageSpec.mConfig.mCmd;

    LOG_DBG() << "VM path: path=" << runtimeSpec.mVM->mKernel.mPath;

    return ErrorEnum::eNone;
}

Error Instance::CreateLinuxSpec(
    const oci::ImageSpec& imageSpec, const oci::ServiceConfig& serviceConfig, oci::RuntimeSpec& runtimeSpec)
{
    (void)imageSpec;
    (void)serviceConfig;

    LOG_DBG() << "Create Linux runtime spec: instanceID=" << *this;

    if (auto err = oci::CreateExampleRuntimeSpec(runtimeSpec, cGroupV2); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    runtimeSpec.mProcess->mTerminal  = false;
    runtimeSpec.mProcess->mUser.mUID = mInstanceInfo.mUID;
    runtimeSpec.mProcess->mUser.mGID = mService->mGID;

    runtimeSpec.mLinux->mCgroupsPath = FS::JoinPath(cCgroupsPath, mInstanceID);

    runtimeSpec.mRoot->mPath     = FS::JoinPath(mRuntimeDir, cRootFSDir);
    runtimeSpec.mRoot->mReadonly = true;

    if (auto err = BindHostDirs(runtimeSpec); !err.IsNone()) {
        return err;
    }

    auto instanceNetns = mNetworkManager.GetNetnsPath(mInstanceID);
    if (!instanceNetns.mError.IsNone()) {
        return AOS_ERROR_WRAP(instanceNetns.mError);
    }

    if (auto err
        = AddNamespace(oci::LinuxNamespace {oci::LinuxNamespaceEnum::eNetwork, instanceNetns.mValue}, runtimeSpec);
        !err.IsNone()) {
        return err;
    }

    if (auto err = CreateAosEnvVars(runtimeSpec); !err.IsNone()) {
        return err;
    }

    if (auto err = ApplyImageConfig(imageSpec, runtimeSpec); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Instance::CreateRuntimeSpec(const image::ImageParts& imageParts, oci::RuntimeSpec& runtimeSpec)
{
    auto imageSpec     = MakeUnique<oci::ImageSpec>(&sAllocator);
    auto serviceConfig = MakeUnique<oci::ServiceConfig>(&sAllocator);

    if (auto err = mOCIManager.LoadImageSpec(imageParts.mImageConfigPath, *imageSpec); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCIManager.LoadServiceConfig(imageParts.mServiceConfigPath, *serviceConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (serviceConfig->mRunners
            .FindIf([](const String& runner) { return runner == Runner(RunnerEnum::eXRUN).ToString(); })
            .mError.IsNone()) {
        if (auto err = CreateVMSpec(imageParts.mServiceFSPath, *imageSpec, runtimeSpec); !err.IsNone()) {
            return err;
        }
    } else {
        if (auto err = CreateLinuxSpec(*imageSpec, *serviceConfig, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    if (auto err = mOCIManager.SaveRuntimeSpec(FS::JoinPath(mRuntimeDir, cRuntimeSpecFile), runtimeSpec);
        !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

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

Error Instance::PrepareRootFS(const image::ImageParts& imageParts, const Array<oci::Mount>& mounts)
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

    if (auto err = mRuntime.PrepareServiceRootFS(
            FS::JoinPath(mRuntimeDir, cRootFSDir), FS::JoinPath(mRuntimeDir, cMountPointsDir), mounts, *layers);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
