/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/launcher/instance.hpp"
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
    resourcemanager::ResourceManagerItf& resourceManager, networkmanager::NetworkManagerItf& networkmanager,
    iam::permhandler::PermHandlerItf& permHandler, runner::RunnerItf& runner, RuntimeItf& runtime,
    monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager, const String& hostWhiteoutsDir,
    const NodeInfo& nodeInfo)
    : mConfig(config)
    , mInstanceID(instanceID)
    , mInstanceInfo(instanceInfo)
    , mServiceManager(serviceManager)
    , mLayerManager(layerManager)
    , mResourceManager(resourceManager)
    , mNetworkManager(networkmanager)
    , mPermHandler(permHandler)
    , mRunner(runner)
    , mRuntime(runtime)
    , mResourceMonitor(resourceMonitor)
    , mOCIManager(ociManager)
    , mHostWhiteoutsDir(hostWhiteoutsDir)
    , mNodeInfo(nodeInfo)
    , mRuntimeDir(FS::JoinPath(cRuntimeDir, mInstanceID))
{
    LOG_DBG() << "Create instance: ident=" << mInstanceInfo.mInstanceIdent << ", instanceID=" << *this;
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

    auto imageParts = MakeUnique<image::ImageParts>(&sAllocator);

    if (auto err = mServiceManager.GetImageParts(*mService, *imageParts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto serviceConfig = MakeUnique<oci::ServiceConfig>(&sAllocator);

    if (auto err = mOCIManager.LoadServiceConfig(imageParts->mServiceConfigPath, *serviceConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto runtimeSpec = MakeUnique<oci::RuntimeSpec>(&sAllocator);

    if (auto err = CreateRuntimeSpec(*imageParts, *serviceConfig, *runtimeSpec); !err.IsNone()) {
        return err;
    }

    if (auto err = SetupNetwork(*serviceConfig); !err.IsNone()) {
        return err;
    }

    if (!mInstanceInfo.mStatePath.IsEmpty()) {
        auto statePath = GetFullStatePath(mInstanceInfo.mStatePath);

        LOG_DBG() << "Prepare state: instance=" << *this << ", path=" << statePath;

        if (auto err = mRuntime.PrepareServiceState(statePath, mInstanceInfo.mUID, mService->mGID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (!mInstanceInfo.mStoragePath.IsEmpty()) {
        auto storagePath = GetFullStoragePath(mInstanceInfo.mStoragePath);

        LOG_DBG() << "Prepare storage: instance=" << *this << ", path=" << storagePath;

        if (auto err = mRuntime.PrepareServiceStorage(storagePath, mInstanceInfo.mUID, mService->mGID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
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

    if (mPermissionsRegistered) {
        if (auto err = mPermHandler.UnregisterInstance(mInstanceInfo.mInstanceIdent);
            !err.IsNone() && stopErr.IsNone()) {
            stopErr = err;
        }

        mPermissionsRegistered = false;
    }

    if (mService) {
        if (auto err = mNetworkManager.RemoveInstanceFromNetwork(mInstanceID, mService->mProviderID);
            !err.IsNone() && stopErr.IsNone()) {
            stopErr = err;
        }
    }

    auto rootfsPath = FS::JoinPath(mRuntimeDir, cRootFSDir);

    if (auto [exist, err] = FS::DirExist(rootfsPath); !err.IsNone() || exist) {
        if (err = mRuntime.UmountServiceRootFS(rootfsPath); !err.IsNone() && stopErr.IsNone()) {
            stopErr = AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = FS::RemoveAll(mRuntimeDir); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    return stopErr;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Instance::BindHostDirs(oci::RuntimeSpec& runtimeSpec)
{
    for (const auto& hostEntry : cBindEtcEntries) {
        auto path  = FS::JoinPath("/etc", hostEntry);
        auto mount = MakeShared<Mount>(&sAllocator, path, path, "bind", "bind,ro");

        if (auto err = AddMount(*mount, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Instance::CreateAosEnvVars(oci::RuntimeSpec& runtimeSpec)
{
    auto                         envVars = MakeUnique<EnvVarsArray>(&sAllocator);
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

size_t Instance::GetNumCPUCores() const
{
    int numCores = 0;

    for (const auto& cpu : mNodeInfo.mCPUs) {
        numCores += cpu.mNumCores;
    }

    if (numCores == 0) {
        LOG_WRN() << "Can't identify number of CPU cores, default value (1) will be taken: instanceID=" << *this;

        numCores = 1;
    }

    return numCores;
}

Error Instance::SetResources(const Array<StaticString<cResourceNameLen>>& resources, oci::RuntimeSpec& runtimeSpec)
{
    for (const auto& resource : resources) {
        auto resourceInfo = MakeUnique<ResourceInfo>(&sAllocator);

        if (auto err = mResourceManager.GetResourceInfo(resource, *resourceInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        for (const auto& group : resourceInfo->mGroups) {
            auto [gid, err] = mRuntime.GetGIDByName(group);
            if (!err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (err = AddAdditionalGID(gid, runtimeSpec); !err.IsNone()) {
                return err;
            }
        }

        for (const auto& mount : resourceInfo->mMounts) {
            if (auto err = AddMount(mount, runtimeSpec); !err.IsNone()) {
                return err;
            }
        }

        if (auto err = AddEnvVars(resourceInfo->mEnv, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Instance::SetDevices(const Array<oci::ServiceDevice>& devices, oci::RuntimeSpec& runtimeSpec)
{
    for (const auto& device : devices) {
        auto deviceInfo = MakeUnique<DeviceInfo>(&sAllocator);

        if (auto err = mResourceManager.GetDeviceInfo(device.mDevice, *deviceInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        StaticArray<StaticString<cDeviceNameLen>, 2> devicePaths;

        if (auto err = device.mDevice.Split(devicePaths, ':'); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto ociDevices = MakeUnique<StaticArray<oci::LinuxDevice, cMaxNumHostDevices>>(&sAllocator);

        if (auto err = mRuntime.PopulateHostDevices(devicePaths[0], *ociDevices); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (devicePaths.Size() == 2) {
            for (auto& ociDevice : *ociDevices) {
                if (auto err = ociDevice.mPath.Replace(devicePaths[0], devicePaths[1], 1); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }
            }
        }

        for (const auto& ociDevice : *ociDevices) {
            if (auto err = AddDevice(ociDevice, device.mPermissions, runtimeSpec); !err.IsNone()) {
                return err;
            }
        }

        for (const auto& group : deviceInfo->mGroups) {
            auto [gid, err] = mRuntime.GetGIDByName(group);
            if (!err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (err = AddAdditionalGID(gid, runtimeSpec); !err.IsNone()) {
                return err;
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Instance::ApplyServiceConfig(const oci::ServiceConfig& serviceConfig, oci::RuntimeSpec& runtimeSpec)
{
    if (serviceConfig.mHostname.HasValue()) {
        runtimeSpec.mHostname = *serviceConfig.mHostname;
    }

    runtimeSpec.mLinux->mSysctl = serviceConfig.mSysctl;

    if (serviceConfig.mQuotas.mCPUDMIPSLimit.HasValue()) {
        int64_t quota
            = *serviceConfig.mQuotas.mCPUDMIPSLimit * cDefaultCPUPeriod * GetNumCPUCores() / mNodeInfo.mMaxDMIPS;
        if (quota < cMinCPUQuota) {
            quota = cMinCPUQuota;
        }

        if (auto err = SetCPULimit(quota, cDefaultCPUPeriod, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    if (serviceConfig.mQuotas.mRAMLimit.HasValue()) {
        if (auto err = SetRAMLimit(*serviceConfig.mQuotas.mRAMLimit, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    if (serviceConfig.mQuotas.mPIDsLimit.HasValue()) {
        auto pidLimit = *serviceConfig.mQuotas.mPIDsLimit;

        if (auto err = SetPIDLimit(pidLimit, runtimeSpec); !err.IsNone()) {
            return err;
        }

        if (auto err = AddRLimit(oci::POSIXRlimit {"RLIMIT_NPROC", pidLimit, pidLimit}, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    if (serviceConfig.mQuotas.mNoFileLimit.HasValue()) {
        auto noFileLimit = *serviceConfig.mQuotas.mNoFileLimit;

        if (auto err = AddRLimit(oci::POSIXRlimit {"RLIMIT_NOFILE", noFileLimit, noFileLimit}, runtimeSpec);
            !err.IsNone()) {
            return err;
        }
    }

    if (serviceConfig.mQuotas.mTmpLimit.HasValue()) {
        StaticString<cFSMountOptionLen> tmpFSOpts;

        if (auto err = tmpFSOpts.Format("nosuid,strictatime,mode=1777,size=%lu", *serviceConfig.mQuotas.mTmpLimit);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto mount = MakeShared<Mount>(&sAllocator, "tmpfs", "/tmp", "tmpfs", tmpFSOpts);

        if (auto err = AddMount(*mount, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    if (!serviceConfig.mPermissions.IsEmpty()) {
        auto [secret, err] = mPermHandler.RegisterInstance(mInstanceInfo.mInstanceIdent, serviceConfig.mPermissions);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        mPermissionsRegistered = true;

        StaticString<cEnvVarNameLen> envVar;

        if (err = envVar.Format("%s=%s", cEnvAosSecret, secret.CStr()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (err = AddEnvVars(Array<StaticString<cEnvVarNameLen>>(&envVar, 1), runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    if (auto err = SetResources(serviceConfig.mResources, runtimeSpec); !err.IsNone()) {
        return err;
    }

    if (auto err = SetDevices(serviceConfig.mDevices, runtimeSpec); !err.IsNone()) {
        return err;
    }

    mOfflineTTL = serviceConfig.mOfflineTTL;

    return ErrorEnum::eNone;
}

Error Instance::ApplyStateStorage(oci::RuntimeSpec& runtimeSpec)
{
    if (!mInstanceInfo.mStatePath.IsEmpty()) {
        auto [absPath, err] = mRuntime.GetAbsPath(GetFullStatePath(mInstanceInfo.mStatePath));
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto mount = MakeShared<Mount>(&sAllocator, absPath, cInstanceStateFile, "bind", "bind,rw");

        if (err = AddMount(*mount, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    if (!mInstanceInfo.mStoragePath.IsEmpty()) {
        auto [absPath, err] = mRuntime.GetAbsPath(GetFullStoragePath(mInstanceInfo.mStoragePath));
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto mount = MakeShared<Mount>(&sAllocator, absPath, cInstanceStorageDir, "bind", "bind,rw");

        if (err = AddMount(*mount, runtimeSpec); !err.IsNone()) {
            return err;
        }
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
    LOG_DBG() << "Create Linux runtime spec: instanceID=" << *this;

    if (auto err = oci::CreateExampleRuntimeSpec(runtimeSpec, cGroupV2); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    runtimeSpec.mProcess->mTerminal  = false;
    runtimeSpec.mProcess->mUser.mUID = mInstanceInfo.mUID;
    runtimeSpec.mProcess->mUser.mGID = mService->mGID;

    runtimeSpec.mLinux->mCgroupsPath = FS::JoinPath(cCgroupsPath, mInstanceID);

    runtimeSpec.mRoot->mPath     = FS::JoinPath(mRuntimeDir, cRootFSDir);
    runtimeSpec.mRoot->mReadonly = false;

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

    if (auto err = AddEnvVars(mOverrideEnvVars, runtimeSpec); !err.IsNone()) {
        return err;
    }

    if (auto err = ApplyImageConfig(imageSpec, runtimeSpec); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = ApplyServiceConfig(serviceConfig, runtimeSpec); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = ApplyStateStorage(runtimeSpec); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Instance::CreateRuntimeSpec(
    const image::ImageParts& imageParts, const oci::ServiceConfig& serviceConfig, oci::RuntimeSpec& runtimeSpec)
{
    auto imageSpec = MakeUnique<oci::ImageSpec>(&sAllocator);

    if (auto err = mOCIManager.LoadImageSpec(imageParts.mImageConfigPath, *imageSpec); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (serviceConfig.mRunners.FindIf(
            [](const String& runner) { return runner == Runner(RunnerEnum::eXRUN).ToString(); })
        != serviceConfig.mRunners.end()) {
        if (auto err = CreateVMSpec(imageParts.mServiceFSPath, *imageSpec, runtimeSpec); !err.IsNone()) {
            return err;
        }
    } else {
        if (auto err = CreateLinuxSpec(*imageSpec, serviceConfig, runtimeSpec); !err.IsNone()) {
            return err;
        }
    }

    if (auto err = mOCIManager.SaveRuntimeSpec(FS::JoinPath(mRuntimeDir, cRuntimeSpecFile), runtimeSpec);
        !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error Instance::AddNetworkHostsFromResources(const Array<StaticString<cResourceNameLen>>& resources, Array<Host>& hosts)
{
    for (auto const& resource : resources) {
        auto resourceInfo = MakeUnique<ResourceInfo>(&sAllocator);

        if (auto err = mResourceManager.GetResourceInfo(resource, *resourceInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = hosts.Insert(hosts.end(), resourceInfo->mHosts.begin(), resourceInfo->mHosts.end());
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Instance::SetupNetwork(const oci::ServiceConfig& serviceConfig)
{
    LOG_DBG() << "Setup network: instanceID=" << *this;

    auto networkParams = MakeUnique<networkmanager::InstanceNetworkParameters>(&sAllocator);

    networkParams->mInstanceIdent      = mInstanceInfo.mInstanceIdent;
    networkParams->mHostsFilePath      = FS::JoinPath(mRuntimeDir, cMountPointsDir, "etc", "hosts");
    networkParams->mResolvConfFilePath = FS::JoinPath(mRuntimeDir, cMountPointsDir, "etc", "resolv.conf");
    networkParams->mHosts              = mConfig.mHosts;
    networkParams->mNetworkParameters  = mInstanceInfo.mNetworkParameters;

    if (auto err = AddNetworkHostsFromResources(serviceConfig.mResources, networkParams->mHosts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (serviceConfig.mQuotas.mDownloadSpeed.HasValue()) {
        networkParams->mIngressKbit = *serviceConfig.mQuotas.mDownloadSpeed;
    }

    if (serviceConfig.mQuotas.mUploadSpeed.HasValue()) {
        networkParams->mEgressKbit = *serviceConfig.mQuotas.mUploadSpeed;
    }

    if (serviceConfig.mQuotas.mDownloadLimit.HasValue()) {
        networkParams->mDownloadLimit = *serviceConfig.mQuotas.mDownloadLimit;
    }

    if (serviceConfig.mHostname.HasValue()) {
        networkParams->mHostname = *serviceConfig.mHostname;
    }

    if (serviceConfig.mQuotas.mUploadLimit.HasValue()) {
        networkParams->mUploadLimit = *serviceConfig.mQuotas.mUploadLimit;
    }

    if (auto err = mRuntime.PrepareNetworkDir(FS::JoinPath(mRuntimeDir, cMountPointsDir)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

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

    if (!mInstanceInfo.mStatePath.IsEmpty()) {
        monitoringParms->mPartitions.PushBack({cStatePartitionName, GetFullStatePath(mInstanceInfo.mStatePath)});
    }

    if (!mInstanceInfo.mStoragePath.IsEmpty()) {
        monitoringParms->mPartitions.PushBack({cStoragePartitionName, GetFullStoragePath(mInstanceInfo.mStoragePath)});
    }

    if (auto err = mResourceMonitor.StartInstanceMonitoring(mInstanceID, *monitoringParms); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Instance::PrepareRootFS(const image::ImageParts& imageParts, const Array<Mount>& mounts)
{
    LOG_DBG() << "Prepare root FS: instanceID=" << *this;

    auto mountPoints = FS::JoinPath(mRuntimeDir, cMountPointsDir);

    if (auto err = mRuntime.CreateMountPoints(mountPoints, mounts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto layers = MakeUnique<LayersStaticArray>(&sAllocator);

    if (auto err = layers->PushBack(mountPoints); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

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

    if (auto err = layers->PushBack("/"); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mRuntime.MountServiceRootFS(FS::JoinPath(mRuntimeDir, cRootFSDir), *layers); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
