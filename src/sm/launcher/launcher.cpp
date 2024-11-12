/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/launcher.hpp"
#include "aos/common/tools/memory.hpp"
#include "log.hpp"

namespace aos {
namespace sm {
namespace launcher {

using namespace runner;

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(servicemanager::ServiceManagerItf& serviceManager, runner::RunnerItf& runner,
    OCISpecItf& ociManager, InstanceStatusReceiverItf& statusReceiver, StorageItf& storage,
    monitoring::ResourceMonitorItf& resourceMonitor, ConnectionPublisherItf& connectionPublisher)
{
    LOG_DBG() << "Init launcher";

    mConnectionPublisher = &connectionPublisher;

    mServiceManager  = &serviceManager;
    mRunner          = &runner;
    mOCIManager      = &ociManager;
    mStatusReceiver  = &statusReceiver;
    mStorage         = &storage;
    mResourceMonitor = &resourceMonitor;

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start launcher";

    if (auto err = mConnectionPublisher->Subscribe(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = RunLastInstances(); !err.IsNone()) {
        LOG_ERR() << "Error running last instances: " << err;
    }

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Stop launcher";

        mConnectionPublisher->Unsubscribe(*this);

        mClose = true;

        mCondVar.NotifyOne();
    }

    mThread.Join();

    return ErrorEnum::eNone;
}

Error Launcher::RunInstances(const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
    const Array<InstanceInfo>& instances, bool forceRestart)
{
    UniqueLock lock {mMutex};

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

    auto err
        = mThread.Run([this, instances = MakeShared<const InstanceInfoStaticArray>(&mAllocator, instances),
                          services = MakeShared<const ServiceInfoStaticArray>(&mAllocator, services),
                          layers   = MakeShared<const LayerInfoStaticArray>(&mAllocator, layers), forceRestart](void*) {
              ProcessLayers(*layers);
              ProcessServices(*services);

              auto err = UpdateStorage(*instances);
              if (!err.IsNone()) {
                  LOG_ERR() << "Can't update storage: " << err;
              }

              ProcessInstances(*instances, forceRestart);

              LockGuard lock {mMutex};

              SendRunStatus();

              mLaunchInProgress = false;

              LOG_DBG() << "Allocator size: " << mAllocator.MaxSize()
                        << ", max allocated size: " << mAllocator.MaxAllocatedSize();
#if AOS_CONFIG_THREAD_STACK_USAGE
              LOG_DBG() << "Stack usage: size=" << mThread.GetStackUsage();
#endif
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

Error Launcher::UpdateStorage(const Array<InstanceInfo>& instances)
{
    auto currentInstances = MakeUnique<InstanceInfoStaticArray>(&mAllocator);

    auto err = mStorage->GetAllInstances(*currentInstances);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& currentInstance : *currentInstances) {
        auto findResult = instances.Find(currentInstance);
        if (!findResult.mError.IsNone()) {
            err = mStorage->RemoveInstance(currentInstance.mInstanceIdent);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't remove instance " << currentInstance.mInstanceIdent << " from storage: " << err;
            }
        }
    }

    for (const auto& instance : instances) {
        auto findResult = currentInstances->Find(instance);
        if (!findResult.mError.IsNone()) {
            err = mStorage->AddInstance(instance);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't store instance " << instance.mInstanceIdent << ": " << err;
            }
        }
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Launcher::RunLastInstances()
{
    LOG_DBG() << "Run last instances";

    if (mLaunchInProgress) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    mLaunchInProgress = true;

    // Wait in case previous request is not yet finished
    mThread.Join();

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    auto instances = SharedPtr<const Array<InstanceInfo>>(&mAllocator, new (&mAllocator) InstanceInfoStaticArray());

    auto err = mStorage->GetAllInstances(const_cast<Array<InstanceInfo>&>(*instances));
    if (!err.IsNone()) {
        return err;
    }

    err = mThread.Run([this, instances](void*) mutable {
        ProcessInstances(*instances);

        UniqueLock lock {mMutex};

        mCondVar.Wait(lock, [&] { return mConnected || mClose; });

        if (mClose) {
            return;
        }

        SendRunStatus();

        mLaunchInProgress = false;

#if AOS_CONFIG_THREAD_STACK_USAGE
        LOG_DBG() << "Stack usage: size=" << mThread.GetStackUsage();
#endif
        LOG_DBG() << "Allocator size: " << mAllocator.MaxSize()
                  << ", max allocated size: " << mAllocator.MaxAllocatedSize();
    });
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Launcher::ProcessServices(const Array<ServiceInfo>& services)
{
    LOG_DBG() << "Process services";

    auto err = mServiceManager->InstallServices(services);
    if (!err.IsNone()) {
        LOG_ERR() << "Can't install services: " << err;
    }
}

void Launcher::ProcessLayers(const Array<LayerInfo>& layers)
{
    (void)layers;

    LOG_DBG() << "Process layers";
}

void Launcher::ProcessInstances(const Array<InstanceInfo>& instances, const bool forceRestart)
{
    LOG_DBG() << "Process instances: restart=" << forceRestart;

    auto err = mLaunchPool.Run();
    if (!err.IsNone()) {
        LOG_ERR() << "Can't run launcher thread pool: " << err;
    }

    StopInstances(instances, forceRestart);
    CacheServices(instances);
    StartInstances(instances);

    mLaunchPool.Shutdown();

#if AOS_CONFIG_THREAD_STACK_USAGE
    LOG_DBG() << "Launch pool stack usage: size=" << mLaunchPool.GetStackUsage();
#endif
}

void Launcher::SendRunStatus()
{
    auto status = MakeUnique<InstanceStatusStaticArray>(&mAllocator);

    // cppcheck-suppress unusedVariable
    for (const auto& [_, instance] : mCurrentInstances) {
        LOG_DBG() << "Instance status: instance=" << instance << ", serviceVersion=" << instance.GetServiceVersion()
                  << ", runState=" << instance.RunState() << ", err=" << instance.RunError();

        status->PushBack(
            {instance.Info().mInstanceIdent, instance.GetServiceVersion(), instance.RunState(), instance.RunError()});
    }

    LOG_DBG() << "Send run status";

    auto err = mStatusReceiver->InstancesRunStatus(*status);
    if (!err.IsNone()) {
        LOG_ERR() << "Sending run status error: " << err;
    }
}

void Launcher::StopInstances(const Array<InstanceInfo>& instances, bool forceRestart)
{
    UniqueLock lock {mMutex};

    LOG_DBG() << "Stop instances";

    auto services = MakeUnique<servicemanager::ServiceDataStaticArray>(&mAllocator);

    auto err = mServiceManager->GetAllServices(*services);
    if (!err.IsNone()) {
        LOG_ERR() << "Can't get current services: " << err;
    }

    for (const auto& [_, instance] : mCurrentInstances) {
        auto found = instances
                         .FindIf([&instance = instance](const InstanceInfo& info) {
                             auto compareInfo = info;

                             compareInfo.mPriority = instance.Info().mPriority;

                             return compareInfo == instance.Info();
                         })
                         .mError.IsNone();

        // Stop instance if: forceRestart or not in instances array or not active state or Aos version changed
        if (!forceRestart && found && instance.RunState() == InstanceRunStateEnum::eActive) {
            auto findService = services->FindIf([&instance = instance](const servicemanager::ServiceData& service) {
                return instance.Info().mInstanceIdent.mServiceID == service.mServiceID;
            });

            if (findService.mError.IsNone() && instance.GetServiceVersion() == findService.mValue->mVersion) {
                continue;
            }
        }

        err = mLaunchPool.AddTask([this, ident = instance.Info().mInstanceIdent](void*) mutable {
            auto err = StopInstance(ident);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't stop instance " << ident << ": " << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't stop instance " << instance << ": " << err;
        }
    }

    lock.Unlock();

    mLaunchPool.Wait();
}

void Launcher::StartInstances(const Array<InstanceInfo>& instances)
{
    UniqueLock lock {mMutex};

    LOG_DBG() << "Start instances";

    for (const auto& info : instances) {
        // Skip already started instances
        if (mCurrentInstances.At(info.mInstanceIdent).mError.IsNone()) {
            continue;
        }

        auto err = mLaunchPool.AddTask([this, &info](void*) {
            auto err = StartInstance(info);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't start instance " << info.mInstanceIdent << ": " << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't start instance " << info.mInstanceIdent << ": " << err;
        }
    }

    lock.Unlock();

    mLaunchPool.Wait();
}

void Launcher::CacheServices(const Array<InstanceInfo>& instances)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Cache services";

    mCurrentServices.Clear();

    for (const auto& instance : instances) {
        if (GetService(instance.mInstanceIdent.mServiceID).mError.IsNone()) {
            continue;
        }

        auto findService = mServiceManager->GetService(instance.mInstanceIdent.mServiceID);
        if (!findService.mError.IsNone()) {
            LOG_ERR() << "Can't get service " << instance.mInstanceIdent.mServiceID << ": " << findService.mError;
            continue;
        }

        auto err = mCurrentServices.Emplace(
            instance.mInstanceIdent.mServiceID, Service(findService.mValue, *mServiceManager, *mOCIManager));
        if (!err.IsNone()) {
            LOG_ERR() << "Can't cache service " << instance.mInstanceIdent.mServiceID << ": " << err;
            continue;
        }

        err = mCurrentServices.At(instance.mInstanceIdent.mServiceID).mValue.LoadSpecs();
        if (!err.IsNone()) {
            LOG_ERR() << "Can't load OCI spec for service " << instance.mInstanceIdent.mServiceID << ": " << err;
            continue;
        }
    }

    UpdateInstanceServices();
}

void Launcher::UpdateInstanceServices()
{
    // cppcheck-suppress unusedVariable
    for (auto& [_, instance] : mCurrentInstances) {
        auto findService = GetService(instance.Info().mInstanceIdent.mServiceID);
        if (!findService.mError.IsNone()) {
            LOG_ERR() << "Can't get service for instance " << instance << ": " << findService.mError;

            instance.SetService(nullptr);

            continue;
        }

        instance.SetService(&findService.mValue);
    }
}

Error Launcher::StartInstance(const InstanceInfo& info)
{
    UniqueLock lock {mMutex};

    if (mCurrentInstances.At(info.mInstanceIdent).mError.IsNone()) {
        return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
    }

    auto err
        = mCurrentInstances.Emplace(info.mInstanceIdent, Instance(info, *mOCIManager, *mRunner, *mResourceMonitor));
    if (!err.IsNone()) {
        return err;
    }

    auto& instance = mCurrentInstances.At(info.mInstanceIdent).mValue;

    auto findService = GetService(info.mInstanceIdent.mServiceID);

    instance.SetService(&findService.mValue, findService.mError);

    if (!findService.mError.IsNone()) {
        return findService.mError;
    }

    lock.Unlock();

    err = instance.Start();
    if (!err.IsNone()) {
        return err;
    }

    LOG_INF() << "Instance started: " << instance;

    return ErrorEnum::eNone;
}

Error Launcher::StopInstance(const InstanceIdent& ident)
{
    UniqueLock lock {mMutex};

    auto findInstance = mCurrentInstances.At(ident);
    if (!findInstance.mError.IsNone()) {
        return findInstance.mError;
    }

    auto instance = findInstance.mValue;

    mCurrentInstances.Remove(ident);

    lock.Unlock();

    auto err = instance.Stop();
    if (!err.IsNone()) {
        return err;
    }

    LOG_INF() << "Instance stopped: " << instance;

    return ErrorEnum::eNone;
}

void Launcher::OnConnect()
{
    LockGuard lock {mMutex};

    if (!mLaunchInProgress) {
        if (auto err = RunLastInstances(); !err.IsNone()) {
            LOG_ERR() << "Error running last instances: " << err;
        }
    }

    mConnected = true;
    mCondVar.NotifyOne();
}

void Launcher::OnDisconnect()
{
    LockGuard lock {mMutex};

    mConnected = false;
    mCondVar.NotifyOne();
}

} // namespace launcher
} // namespace sm
} // namespace aos
