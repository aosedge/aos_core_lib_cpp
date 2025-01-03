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

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

const char* const cDefaultHostFSBinds[] = {"bin", "sbin", "lib", "lib64", "usr"};

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(const Config& config, iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    servicemanager::ServiceManagerItf& serviceManager, layermanager::LayerManagerItf& layerManager,
    resourcemanager::ResourceManagerItf& resourceManager, networkmanager::NetworkManagerItf& networkManager,
    iam::permhandler::PermHandlerItf& permHandler, runner::RunnerItf& runner, RuntimeItf& runtime,
    monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager,
    InstanceStatusReceiverItf& statusReceiver, ConnectionPublisherItf& connectionPublisher, StorageItf& storage)
{
    LOG_DBG() << "Init launcher";

    (void)permHandler;
    (void)resourceManager;

    mConfig              = config;
    mConnectionPublisher = &connectionPublisher;
    mLayerManager        = &layerManager;
    mNetworkManager      = &networkManager;
    mOCIManager          = &ociManager;
    mResourceMonitor     = &resourceMonitor;
    mRunner              = &runner;
    mRuntime             = &runtime;
    mServiceManager      = &serviceManager;
    mStatusReceiver      = &statusReceiver;
    mStorage             = &storage;

    if (auto err = nodeInfoProvider.GetNodeInfo(mNodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mConfig.mHostBinds.IsEmpty()) {
        for (const auto& bind : cDefaultHostFSBinds) {
            if (auto err = mConfig.mHostBinds.PushBack(bind); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    mHostWhiteoutsDir = FS::JoinPath(mConfig.mWorkDir, cHostFSWhiteoutsDir);

    if (auto err = mRuntime->CreateHostFSWhiteouts(mHostWhiteoutsDir, mConfig.mHostBinds); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

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
        LOG_ERR() << "Error running last instances: err=" << err;
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
                  LOG_WRN() << "Error occurred while getting running instances: err=" << err;
              }

              ProcessInstances(*runInstances, forceRestart);

              LockGuard lock {mMutex};

              SendRunStatus();

              mLaunchInProgress = false;

              Instance::ShowAllocatorStats();

              LOG_DBG() << "Launcher allocator: size=" << mAllocator.MaxSize()
                        << ", maxAllocated=" << mAllocator.MaxAllocatedSize();
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

Error Launcher::UpdateRunStatus(const Array<runner::RunStatus>& instances)
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

        Instance::ShowAllocatorStats();

        LOG_DBG() << "Launcher allocator: size=" << mAllocator.MaxSize()
                  << ", maxAllocated=" << mAllocator.MaxAllocatedSize();
#if AOS_CONFIG_THREAD_STACK_USAGE
        LOG_DBG() << "Stack usage: size=" << mThread.GetStackUsage();
#endif
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
        LOG_ERR() << "Can't install services: err=" << err;
    }
}

void Launcher::ProcessLayers(const Array<LayerInfo>& layers)
{
    LOG_DBG() << "Process layers";

    auto err = mLayerManager->ProcessDesiredLayers(layers);
    if (!err.IsNone()) {
        LOG_ERR() << "Can't install layers: err=" << err;
    }
}

void Launcher::ProcessInstances(const Array<InstanceData>& instances, const bool forceRestart)
{
    LOG_DBG() << "Process instances: restart=" << forceRestart;

    auto err = mLaunchPool.Run();
    if (!err.IsNone()) {
        LOG_ERR() << "Can't run launcher thread pool: err=" << err;
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

    for (const auto& instance : mCurrentInstances) {
        LOG_DBG() << "Instance status: instance=" << instance << ", serviceVersion=" << instance.GetServiceVersion()
                  << ", runState=" << instance.RunState() << ", err=" << instance.RunError();

        status->PushBack(
            {instance.Info().mInstanceIdent, instance.GetServiceVersion(), instance.RunState(), instance.RunError()});
    }

    LOG_DBG() << "Send run status";

    auto err = mStatusReceiver->InstancesRunStatus(*status);
    if (!err.IsNone()) {
        LOG_ERR() << "Sending run status error: err=" << err;
    }
}

void Launcher::StopInstances(const Array<InstanceData>& instances, bool forceRestart)
{
    UniqueLock lock {mMutex};

    LOG_DBG() << "Stop instances";

    auto services = MakeUnique<servicemanager::ServiceDataStaticArray>(&mAllocator);

    auto err = mServiceManager->GetAllServices(*services);
    if (!err.IsNone()) {
        LOG_ERR() << "Can't get current services: err=" << err;
    }

    for (const auto& instance : mCurrentInstances) {
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
                LOG_ERR() << "Can't stop instance: instanceID=" << instanceID << ", err=" << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't stop instance: instance=" << instance << ", err=" << err;
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
        if (GetInstance(info.mInstanceID).mError.IsNone()) {
            continue;
        }

        auto err = mLaunchPool.AddTask([this, &info](void*) {
            auto err = StartInstance(info);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't start instance: instanceID=" << info.mInstanceID
                          << ", ident=" << info.mInstanceInfo.mInstanceIdent << ", err=" << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't start instance: instanceID=" << info.mInstanceID
                      << ", ident=" << info.mInstanceInfo.mInstanceIdent << ", err=" << err;
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

        servicemanager::ServiceData serviceData;

        if (auto err = mServiceManager->GetService(serviceID, serviceData); !err.IsNone()) {
            LOG_ERR() << "Can't get service: serviceID=" << serviceID << ", err=" << err;
            continue;
        }

        if (auto err = mCurrentServices.EmplaceBack(serviceData); !err.IsNone()) {
            LOG_ERR() << "Can't cache service: serviceID=" << serviceID << ", err=" << err;
            continue;
        }
    }

    UpdateInstanceServices();

    LOG_DBG() << "Services cached: count=" << mCurrentServices.Size();
}

void Launcher::UpdateInstanceServices()
{
    for (auto& instance : mCurrentInstances) {
        auto findService = GetService(instance.Info().mInstanceIdent.mServiceID);
        if (!findService.mError.IsNone()) {
            LOG_ERR() << "Can't get service for instance " << instance << ": " << findService.mError;

            instance.SetService(nullptr);

            continue;
        }

        instance.SetService(findService.mValue);
    }
}

Error Launcher::StartInstance(const InstanceData& info)
{
    UniqueLock lock {mMutex};

    if (GetInstance(info.mInstanceID).mError.IsNone()) {
        return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
    }

    if (auto err
        = mCurrentInstances.EmplaceBack(mConfig, info.mInstanceInfo, info.mInstanceID, *mServiceManager, *mLayerManager,
            *mNetworkManager, *mRunner, *mRuntime, *mResourceMonitor, *mOCIManager, mHostWhiteoutsDir, mNodeInfo);
        !err.IsNone()) {
        return err;
    }

    auto& instance = mCurrentInstances[mCurrentInstances.Size() - 1];

    auto findService = GetService(info.mInstanceInfo.mInstanceIdent.mServiceID);
    if (!findService.mError.IsNone()) {
        instance.SetService(nullptr);
        instance.SetRunError(findService.mError);

        return findService.mError;
    }

    instance.SetService(findService.mValue);

    lock.Unlock();

    if (auto err = instance.Start(); !err.IsNone()) {
        instance.SetRunError(err);

        return err;
    }

    LOG_INF() << "Instance started: instanceID=" << instance;

    return ErrorEnum::eNone;
}

Error Launcher::StopInstance(const String& instanceID)
{
    UniqueLock lock {mMutex};

    auto [instance, err] = GetInstance(instanceID);
    if (!err.IsNone()) {
        return err;
    }

    mCurrentInstances.RemoveIf([&instanceID](const Instance& inst) { return inst.InstanceID() == instanceID; });

    lock.Unlock();

    if (err = instance->Stop(); !err.IsNone()) {
        return err;
    }

    LOG_INF() << "Instance stopped: instanceID=" << *instance;

    return ErrorEnum::eNone;
}

void Launcher::OnConnect()
{
    LockGuard lock {mMutex};

    if (!mLaunchInProgress) {
        if (auto err = RunLastInstances(); !err.IsNone()) {
            LOG_ERR() << "Error running last instances: err=" << err;
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
