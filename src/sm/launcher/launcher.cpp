/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/launcher.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/uuid.hpp"
#include "log.hpp"

namespace aos::sm::launcher {

using namespace runner;

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(const Config& config, servicemanager::ServiceManagerItf& serviceManager, runner::RunnerItf& runner,
    oci::OCISpecItf& ociManager, InstanceStatusReceiverItf& statusReceiver, StorageItf& storage,
    monitoring::ResourceMonitorItf& resourceMonitor, ConnectionPublisherItf& connectionPublisher)
{
    LOG_DBG() << "Init launcher";

    mConfig              = config;
    mConnectionPublisher = &connectionPublisher;
    mServiceManager      = &serviceManager;
    mRunner              = &runner;
    mOCIManager          = &ociManager;
    mStatusReceiver      = &statusReceiver;
    mStorage             = &storage;
    mResourceMonitor     = &resourceMonitor;

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

              auto runInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

              if (auto err = GetRunningInstances(*instances, *runInstances); !err.IsNone()) {
                  LOG_WRN() << "Error occurred while getting running instances: " << err;
              }

              ProcessInstances(*runInstances, forceRestart);

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

Error Launcher::OverrideEnvVars(
    const Array<cloudprotocol::EnvVarsInstanceInfo>& envVarsInfo, cloudprotocol::EnvVarsInstanceStatusArray& statuses)
{
    (void)envVarsInfo;
    (void)statuses;

    LOG_DBG() << "Override environment variables";

    return ErrorEnum::eNone;
}

Error Launcher::UpdateRunStatus(const Array<RunStatus>& instances)
{
    (void)instances;

    LOG_DBG() << "Update run status";

    return ErrorEnum::eNone;
}

Error Launcher::GetRunningInstances(const Array<InstanceInfo>& desiredInstances, Array<InstanceData>& runningInstances)
{
    LOG_DBG() << "Get running instances";

    auto currentInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllInstances(*currentInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& desiredInstance : desiredInstances) {
        Error                       err             = ErrorEnum::eNone;
        sm::launcher::InstanceData* currentInstance = nullptr;

        Tie(currentInstance, err) = currentInstances->FindIf([&desiredInstance](const InstanceData& instance) {
            return instance.mInstanceInfo.mInstanceIdent == desiredInstance.mInstanceIdent;
        });

        if (err.IsNone() && currentInstance) {
            // Update instance if parameters are changed
            if (currentInstance->mInstanceInfo != desiredInstance) {
                if (err = runningInstances.PushBack(*currentInstance); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }

                auto& updateInstance = runningInstances.Back().mValue;

                updateInstance.mInstanceInfo = desiredInstance;

                if (err = mStorage->UpdateInstance(updateInstance); !err.IsNone()) {
                    LOG_ERR() << "Can't update instance: instanceID=" << updateInstance.mInstanceID << ", err=" << err;
                }
            }

            currentInstances->Erase(currentInstance);

            continue;
        }

        const auto instanceID = uuid::UUIDToString(uuid::CreateUUID());

        if (err = runningInstances.PushBack({desiredInstance, instanceID}); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (err = mStorage->AddInstance(runningInstances.Back().mValue); !err.IsNone()) {
            LOG_ERR() << "Can't add instance: instanceID=" << instanceID << ", err=" << err;
        }
    }

    // Remove old instances

    for (const auto& currentInstance : *currentInstances) {
        if (auto err = mStorage->RemoveInstance(currentInstance.mInstanceID); !err.IsNone()) {
            LOG_ERR() << "Can't remove instance: instanceID=" << currentInstance.mInstanceID << ", err=" << err;
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

    auto instances = SharedPtr<const Array<InstanceData>>(&mAllocator, new (&mAllocator) InstanceDataStaticArray());

    auto err = mStorage->GetAllInstances(const_cast<Array<InstanceData>&>(*instances));
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

    auto err = mServiceManager->ProcessDesiredServices(services);
    if (!err.IsNone()) {
        LOG_ERR() << "Can't install services: " << err;
    }
}

void Launcher::ProcessLayers(const Array<LayerInfo>& layers)
{
    (void)layers;

    LOG_DBG() << "Process layers";
}

void Launcher::ProcessInstances(const Array<InstanceData>& instances, const bool forceRestart)
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

void Launcher::StopInstances(const Array<InstanceData>& instances, bool forceRestart)
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
                         .FindIf([&instance = instance](const InstanceData& info) {
                             auto compareInfo = info;

                             compareInfo.mInstanceInfo.mPriority = instance.Info().mPriority;

                             return compareInfo.mInstanceID == instance.InstanceID()
                                 && compareInfo.mInstanceInfo == instance.Info();
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

        StaticString<cInstanceIDLen> instanceID {instance.InstanceID()};

        err = mLaunchPool.AddTask([this, instanceID = Move(instanceID)](void*) mutable {
            auto err = StopInstance(instanceID);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't stop instance " << instanceID << ": " << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't stop instance " << instance << ": " << err;
        }
    }

    lock.Unlock();

    mLaunchPool.Wait();
}

void Launcher::StartInstances(const Array<InstanceData>& instances)
{
    UniqueLock lock {mMutex};

    LOG_DBG() << "Start instances";

    for (const auto& info : instances) {
        // Skip already started instances
        if (mCurrentInstances.At(info.mInstanceID).mError.IsNone()) {
            continue;
        }

        auto err = mLaunchPool.AddTask([this, &info](void*) {
            auto err = StartInstance(info);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't start instance: id=" << info.mInstanceID
                          << ", ident=" << info.mInstanceInfo.mInstanceIdent << ": " << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't start instance: id=" << info.mInstanceID
                      << ", ident=" << info.mInstanceInfo.mInstanceIdent << ": " << err;
        }
    }

    lock.Unlock();

    mLaunchPool.Wait();
}

void Launcher::CacheServices(const Array<InstanceData>& instances)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Cache services";

    mCurrentServices.Clear();

    for (const auto& instance : instances) {
        const auto& serviceID = instance.mInstanceInfo.mInstanceIdent.mServiceID;

        if (GetService(serviceID).mError.IsNone()) {
            continue;
        }

        servicemanager::ServiceData service;

        auto err = mServiceManager->GetService(serviceID, service);
        if (!err.IsNone()) {
            LOG_ERR() << "Can't get service: serviceID=" << serviceID << ", err=" << err;
            continue;
        }

        err = mCurrentServices.Emplace(serviceID, Service(service, *mServiceManager, *mOCIManager));
        if (!err.IsNone()) {
            LOG_ERR() << "Can't cache service: serviceID=" << serviceID << ", err=" << err;
            continue;
        }

        err = mCurrentServices.At(serviceID).mValue.LoadSpecs();
        if (!err.IsNone()) {
            LOG_ERR() << "Can't load OCI spec for service " << serviceID << ": " << err;
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

Error Launcher::StartInstance(const InstanceData& info)
{
    UniqueLock lock {mMutex};

    if (mCurrentInstances.At(info.mInstanceID).mError.IsNone()) {
        return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
    }

    auto err = mCurrentInstances.Emplace(
        info.mInstanceID, Instance(info.mInstanceInfo, info.mInstanceID, *mOCIManager, *mRunner, *mResourceMonitor));
    if (!err.IsNone()) {
        return err;
    }

    auto& instance = mCurrentInstances.At(info.mInstanceID).mValue;

    auto findService = GetService(info.mInstanceInfo.mInstanceIdent.mServiceID);

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

Error Launcher::StopInstance(const String& instanceID)
{
    UniqueLock lock {mMutex};

    auto findInstance = mCurrentInstances.At(instanceID);
    if (!findInstance.mError.IsNone()) {
        return findInstance.mError;
    }

    auto instance = findInstance.mValue;

    mCurrentInstances.Remove(instanceID);

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

} // namespace aos::sm::launcher
