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

Mutex                                         Instance::sMutex {};
StaticAllocator<Instance::cSpecAllocatorSize> Instance::sAllocator {};

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Instance::Instance(const InstanceInfo& instanceInfo, const String& instanceID, oci::OCISpecItf& ociManager,
    runner::RunnerItf& runner, monitoring::ResourceMonitorItf& resourceMonitor)
    : mInstanceID(instanceID)
    , mInstanceInfo(instanceInfo)
    , mOCIManager(ociManager)
    , mRunner(runner)
    , mResourceMonitor(resourceMonitor)
{
    LOG_INF() << "Create instance: ident=" << mInstanceInfo.mInstanceIdent << ", instanceID=" << *this;
}

void Instance::SetService(const Service* service)
{
    mService = service;

    if (mService) {
        LOG_DBG() << "Set service for instance: serviceID=" << *service << ", version=" << mService->Data().mVersion
                  << ", instanceID=" << *this;
    }
}

Error Instance::Start()
{
    LOG_INF() << "Start instance: instanceID=" << *this;

    StaticString<cFilePathLen> instanceDir = FS::JoinPath(cRuntimeDir, mInstanceID);

    auto err = CreateRuntimeSpec(instanceDir);
    if (!err.IsNone()) {
        mRunError = err;

        return err;
    }

    auto runStatus = mRunner.StartInstance(mInstanceID, instanceDir, {});

    mRunState = runStatus.mState;
    mRunError = runStatus.mError;

    if (!runStatus.mError.IsNone()) {
        return runStatus.mError;
    }

    err = mResourceMonitor.StartInstanceMonitoring(
        mInstanceID, monitoring::InstanceMonitorParams {mInstanceInfo.mInstanceIdent, {}});
    if (!err.IsNone()) {
        mRunError = err;

        return err;
    }

    return aos::ErrorEnum::eNone;
}

Error Instance::Stop()
{
    LOG_INF() << "Stop instance: instanceID=" << *this;

    StaticString<cFilePathLen> instanceDir = FS::JoinPath(cRuntimeDir, mInstanceID);
    Error                      stopErr;

    auto err = mRunner.StopInstance(mInstanceID);
    if (!err.IsNone() && stopErr.IsNone()) {
        stopErr = err;
    }

    err = FS::RemoveAll(instanceDir);
    if (!err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    err = mResourceMonitor.StopInstanceMonitoring(mInstanceID);
    if (!err.IsNone() && stopErr.IsNone()) {
        stopErr = err;
    }

    return stopErr;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Instance::CreateRuntimeSpec(const String& path)
{
    LockGuard lock(sMutex);

    LOG_DBG() << "Create runtime spec: path=" << path;

    auto err = FS::ClearDir(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!mService) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto imageSpec = mService->ImageSpec();
    if (!imageSpec.mError.IsNone()) {
        return imageSpec.mError;
    }

    auto serviceFS = mService->ServiceFSPath();
    if (!serviceFS.mError.IsNone()) {
        return serviceFS.mError;
    }

    auto runtimeSpec = MakeUnique<oci::RuntimeSpec>(&sAllocator);

    runtimeSpec->mVM.EmplaceValue();

    if (imageSpec.mValue.mConfig.mEntryPoint.Size() == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    // Set default HW config values. Normally they should be taken from service config.
    runtimeSpec->mVM->mHWConfig.mVCPUs = 1;
    // For xen this value should be aligned to 1024Kb
    runtimeSpec->mVM->mHWConfig.mMemKB = 8192;

    runtimeSpec->mVM->mKernel.mPath       = FS::JoinPath(serviceFS.mValue, imageSpec.mValue.mConfig.mEntryPoint[0]);
    runtimeSpec->mVM->mKernel.mParameters = imageSpec.mValue.mConfig.mCmd;

    LOG_DBG() << "Unikernel path: path=" << runtimeSpec->mVM->mKernel.mPath;

    err = mOCIManager.SaveRuntimeSpec(FS::JoinPath(path, cRuntimeSpecFile), *runtimeSpec);
    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
