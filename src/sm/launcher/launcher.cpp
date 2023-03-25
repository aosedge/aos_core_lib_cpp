/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/launcher.hpp"
#include "log.hpp"

namespace aos {
namespace sm {
namespace launcher {

using namespace runner;

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(servicemanager::ServiceManagerItf& serviceManager, runner::RunnerItf& runner,
    InstanceStatusReceiverItf& statusReceiver, StorageItf& storage)
{
    LOG_DBG() << "Initialize launcher";

    mServiceManager = &serviceManager;
    mRunner = &runner;
    mStatusReceiver = &statusReceiver;
    mStorage = &storage;

    return ErrorEnum::eNone;
}

Error Launcher::RunInstances(const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
    const Array<InstanceInfo>& instances, bool forceRestart)
{
    UniqueLock lock(mMutex);

    if (forceRestart) {
        LOG_DBG() << "Restart instances";
    } else {
        LOG_DBG() << "Run instances";
    }

    if (mLaunchInProgress) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    mLaunchInProgress = true;

    lock.Unlock();

    // Wait in case previous request is not yet finished
    mThread.Join();

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    auto err = mThread.Run(
        [this,
            instances
            = SharedPtr<const Array<InstanceInfo>>(&mAllocator, new (&mAllocator) InstanceInfoStaticArray(instances)),
            services
            = SharedPtr<const Array<ServiceInfo>>(&mAllocator, new (&mAllocator) ServiceInfoStaticArray(services)),
            layers = SharedPtr<const Array<LayerInfo>>(&mAllocator, new (&mAllocator) LayerInfoStaticArray(layers)),
            forceRestart](void*) mutable {
            ProcessLayers(layers);
            ProcessServices(services);
            ProcessInstances(instances, forceRestart);

            LockGuard lock(mMutex);

            mLaunchInProgress = false;
            auto err = mStatusReceiver->InstancesRunStatus(aos::Array<aos::InstanceStatus>());
            if (!err.IsNone()) {
                LOG_ERR() << "Sending run status error: " << err;
            }
        });
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::UpdateRunStatus(const Array<RunStatus>& instances)
{
    (void)instances;

    LOG_DBG() << "Run instances";

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Launcher::ProcessServices(SharedPtr<const Array<ServiceInfo>> services)
{
    auto err = mServiceManager->InstallServices(*services);
    if (!err.IsNone()) {
        LOG_ERR() << "Can't install services: " << err;
    }
}

void Launcher::ProcessLayers(SharedPtr<const Array<LayerInfo>> layers)
{
    (void)layers;
}

void Launcher::ProcessInstances(SharedPtr<const Array<InstanceInfo>> instances, bool forceRestart)
{
    (void)instances;
    (void)forceRestart;

    auto err = mLaunchPool.Run();
    if (!err.IsNone()) {
        LOG_ERR() << "Can't run launcher thread pool: " << err;
    }

    mLaunchPool.Shutdown();
}

} // namespace launcher
} // namespace sm
} // namespace aos
