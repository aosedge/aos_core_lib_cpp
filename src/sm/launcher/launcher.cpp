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
            ProcessLayers(*layers);
            layers.Reset();

            ProcessServices(*services);
            services.Reset();

            ProcessInstances(*instances, forceRestart);
            instances.Reset();

            SendRunStatus();
        });
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::UpdateRunStatus(const Array<RunStatus>& instances)
{
    (void)instances;

    LOG_DBG() << "Update run status";

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Launcher::ProcessServices(const Array<ServiceInfo>& services)
{
    auto err = mServiceManager->InstallServices(services);
    if (!err.IsNone()) {
        LOG_ERR() << "Can't install services: " << err;
    }
}

void Launcher::ProcessLayers(const Array<LayerInfo>& layers)
{
    (void)layers;
}

void Launcher::ProcessInstances(const Array<InstanceInfo>& instances, bool forceRestart)
{
    auto err = mLaunchPool.Run();
    if (!err.IsNone()) {
        LOG_ERR() << "Can't run launcher thread pool: " << err;
    }

    StopInstances(instances, forceRestart);
    StartInstances(instances);

    mLaunchPool.Shutdown();
}

void Launcher::SendRunStatus()
{
    LockGuard lock(mMutex);

    mLaunchInProgress = false;

    auto status = MakeUnique<InstanceStatusStaticArray>(&mAllocator);

    for (const auto& instance : mCurrentInstances) {
        status->PushBack(
            {instance.Info().mInstanceIdent, instance.AosVersion(), instance.RunState(), instance.RunError()});
    }

    LOG_DBG() << "Send run status";

    auto err = mStatusReceiver->InstancesRunStatus(*status);
    if (!err.IsNone()) {
        LOG_ERR() << "Sending run status error: " << err;
    }
}

void Launcher::StopInstances(const Array<InstanceInfo>& instances, bool forceRestart)
{
    UniqueLock lock(mMutex);

    for (auto& instance : mCurrentInstances) {
        auto found = instances.Find(instance.Info()).mError.IsNone();

        if (!forceRestart && found) {
            continue;
        }

        auto err = mLaunchPool.AddTask([this, ident = instance.Info().mInstanceIdent](void*) mutable {
            auto err = StopInstance(ident);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't stop instance " << ident << ":" << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't stop instance " << instance << ":" << err;
        }
    }

    lock.Unlock();

    mLaunchPool.Wait();
}

void Launcher::StartInstances(const Array<InstanceInfo>& instances)
{
    UniqueLock lock(mMutex);

    for (const auto& info : instances) {
        if (mCurrentInstances.Find([&info](const Instance& instance) { return instance == info; }).mError.IsNone()) {
            continue;
        }

        auto err = mLaunchPool.AddTask([this, &info](void*) {
            auto err = StartInstance(info);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't start instance " << info.mInstanceIdent << ":" << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't start instance " << info.mInstanceIdent << ":" << err;
        }
    }

    lock.Unlock();

    mLaunchPool.Wait();
}

Error Launcher::StartInstance(const InstanceInfo& info)
{
    Instance instance(info);

    auto err = instance.Start();
    if (!err.IsNone()) {
        return err;
    }

    LockGuard lock(mMutex);

    err = mCurrentInstances.PushBack(instance);
    if (!err.IsNone()) {
        return err;
    }

    LOG_INF() << "Instance started: " << instance;

    return ErrorEnum::eNone;
}

Error Launcher::StopInstance(const InstanceIdent& ident)
{

    UniqueLock lock(mMutex);

    auto result = mCurrentInstances.Find(
        [&ident](const Instance& instance) { return instance.Info().mInstanceIdent == ident; });
    if (!result.mError.IsNone()) {
        return result.mError;
    }

    auto instance = *result.mValue;

    mCurrentInstances.Remove(result.mValue);

    lock.Unlock();

    auto err = instance.Stop();
    if (!err.IsNone()) {
        return err;
    }

    LOG_INF() << "Instance stopped: " << instance;

    return ErrorEnum::eNone;
}

} // namespace launcher
} // namespace sm
} // namespace aos
