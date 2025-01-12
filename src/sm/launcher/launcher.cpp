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

// cppcheck-suppress constParameter
Error Launcher::Init(const Config& config, iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    servicemanager::ServiceManagerItf& serviceManager, layermanager::LayerManagerItf& layerManager,
    resourcemanager::ResourceManagerItf& resourceManager, networkmanager::NetworkManagerItf& networkManager,
    iam::permhandler::PermHandlerItf& permHandler, runner::RunnerItf& runner, RuntimeItf& runtime,
    monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager,
    InstanceStatusReceiverItf& statusReceiver, ConnectionPublisherItf& connectionPublisher, StorageItf& storage)
{
    LOG_DBG() << "Init launcher";

    mConfig              = config;
    mConnectionPublisher = &connectionPublisher;
    mLayerManager        = &layerManager;
    mNetworkManager      = &networkManager;
    mOCIManager          = &ociManager;
    mPermHandler         = &permHandler;
    mResourceManager     = &resourceManager;
    mResourceMonitor     = &resourceMonitor;
    mRunner              = &runner;
    mRuntime             = &runtime;
    mServiceManager      = &serviceManager;
    mStatusReceiver      = &statusReceiver;
    mStorage             = &storage;

    Error err;

    if (err = nodeInfoProvider.GetNodeInfo(mNodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mConfig.mHostBinds.IsEmpty()) {
        for (const auto& bind : cDefaultHostFSBinds) {
            if (err = mConfig.mHostBinds.PushBack(bind); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    mHostWhiteoutsDir = FS::JoinPath(mConfig.mWorkDir, cHostFSWhiteoutsDir);

    if (err = mRuntime->CreateHostFSWhiteouts(mHostWhiteoutsDir, mConfig.mHostBinds); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (Tie(mOnlineTime, err) = mStorage->GetOnlineTime(); !err.IsNone()) {
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

    if (auto err = mTimer.Create(
            mConfig.mRemoveOutdatedPeriod,
            [this](void*) {
                if (auto err = HandleOfflineTTLs(); !err.IsNone()) {
                    LOG_ERR() << "Error handling offline TTLs: err=" << err;
                }
            },
            false);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    LOG_DBG() << "Stop launcher";

    Error stopError;

    if (auto err = StopCurrentInstances(); !err.IsNone() && stopError.IsNone()) {
        stopError = err;
    }

    {
        LockGuard lock {mMutex};

        mConnectionPublisher->Unsubscribe(*this);

        if (auto err = mTimer.Stop(); !err.IsNone() && stopError.IsNone()) {
            stopError = AOS_ERROR_WRAP(err);
        }
    }

    mThread.Join();

    return stopError;
}

Error Launcher::RunInstances(const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
    const Array<InstanceInfo>& instances, bool forceRestart)
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << (forceRestart ? "Restart instances" : "Run instances");

        if (mLaunchInProgress) {
            return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
        }

        mLaunchInProgress = true;
    }

    // Wait in case previous request is not yet finished
    mThread.Join();

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    auto err
        = mThread.Run([this, instances = MakeShared<const InstanceInfoStaticArray>(&mAllocator, instances),
                          services = MakeShared<const ServiceInfoStaticArray>(&mAllocator, services),
                          layers   = MakeShared<const LayerInfoStaticArray>(&mAllocator, layers), forceRestart](void*) {
              if (auto err = ProcessLayers(*layers); !err.IsNone()) {
                  LOG_ERR() << "Can't process layers: err=" << err;
              }

              if (auto err = ProcessServices(*services); !err.IsNone()) {
                  LOG_ERR() << "Can't process services: err=" << err;
              }

              if (auto err = ProcessInstances(*instances, forceRestart); !err.IsNone()) {
                  LOG_ERR() << "Can't process instances: err=" << err;
              }

              LockGuard lock {mMutex};

              if (auto err = SendRunStatus(); !err.IsNone()) {
                  LOG_ERR() << "Can't send run status: err=" << err;
              }

              mLaunchInProgress = false;
              mCondVar.NotifyOne();

              ShowResourceUsageStats();
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

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Launcher::ShowResourceUsageStats()
{
    LOG_DBG() << "Instances allocator: allocated=" << Instance::GetAllocator().MaxAllocatedSize()
              << ", maxSize=" << Instance::GetAllocator().MaxSize();
    LOG_DBG() << "Launcher allocator: allocated=" << mAllocator.MaxAllocatedSize()
              << ", maxSize=" << mAllocator.MaxSize();
#if AOS_CONFIG_THREAD_STACK_USAGE
    LOG_DBG() << "Thread stack usage: size=" << mThread.GetStackUsage();
    LOG_DBG() << "Launch pool stack usage: size=" << mLaunchPool.GetStackUsage();
#endif
}

Error Launcher::FillCurrentInstance(const Array<InstanceData>& instances)
{
    for (const auto& instance : instances) {
        Error err;
        bool  instanceStarted = false;

        Tie(instanceStarted, err) = Instance::IsInstanceStarted(instance.mInstanceID);
        if (!err.IsNone()) {
            LOG_WRN() << "Can't check instance started: instanceID=" << instance.mInstanceID << ", err=" << err;
        }

        if (!instanceStarted) {
            continue;
        }

        servicemanager::ServiceData* service;

        Tie(service, err) = GetService(instance.mInstanceInfo.mInstanceIdent.mServiceID);
        if (!err.IsNone()) {
            LOG_ERR() << "Can't get service: instanceID=" << instance.mInstanceID << ", err=" << err;

            continue;
        }

        if (err = mCurrentInstances.EmplaceBack(mConfig, instance.mInstanceInfo, instance.mInstanceID, *mServiceManager,
                *mLayerManager, *mResourceManager, *mNetworkManager, *mPermHandler, *mRunner, *mRuntime,
                *mResourceMonitor, *mOCIManager, mHostWhiteoutsDir, mNodeInfo);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto currentInstance = mCurrentInstances.Back();
        if (!currentInstance.mError.IsNone()) {
            return AOS_ERROR_WRAP(currentInstance.mError);
        }

        currentInstance.mValue.SetService(service);
    }

    return ErrorEnum::eNone;
}

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

    if (auto err = mThread.Run([this](void*) {
            if (auto err = ProcessLastInstances(); !err.IsNone()) {
                LOG_ERR() << "Can't process last instances: err=" << err;
            }

            LockGuard lock {mMutex};

            if (auto err = SendRunStatus(); !err.IsNone()) {
                LOG_ERR() << "Can't send run status: err=" << err;
            }

            mLaunchInProgress = false;
            mCondVar.NotifyOne();

            ShowResourceUsageStats();
        });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::ProcessServices(const Array<ServiceInfo>& services)
{
    LOG_DBG() << "Process services";

    auto err = mServiceManager->ProcessDesiredServices(services);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::ProcessLayers(const Array<LayerInfo>& layers)
{
    LOG_DBG() << "Process layers";

    auto err = mLayerManager->ProcessDesiredLayers(layers);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::ProcessLastInstances()
{
    LOG_DBG() << "Process last instances";

    auto startInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllInstances(*startInstances); !err.IsNone()) {
        return err;
    }

    CacheServices(*startInstances);

    if (auto err = FillCurrentInstance(*startInstances); !err.IsNone()) {
        LOG_ERR() << "Can't fill current instances: err=" << err;
    }

    auto stopInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    if (auto err = GetStopInstances(*startInstances, *stopInstances, true); !err.IsNone()) {
        LOG_WRN() << "Error occurred while getting stop instances: err=" << err;
    }

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't run launcher thread pool: err=" << err;
    }

    StopInstances(*stopInstances);
    StartInstances(*startInstances);

    if (auto err = mLaunchPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Can't shutdown launcher thread pool: err=" << err;
    }

    return ErrorEnum::eNone;
}

Error Launcher::ProcessInstances(const Array<InstanceInfo>& instances, const bool forceRestart)
{
    LOG_DBG() << "Process instances: restart=" << forceRestart;

    auto startInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    if (auto err = GetStartInstances(instances, *startInstances); !err.IsNone()) {
        return err;
    }

    auto stopInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    if (auto err = GetStopInstances(*startInstances, *stopInstances, forceRestart); !err.IsNone()) {
        return err;
    }

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StopInstances(*stopInstances);
    CacheServices(*startInstances);
    StartInstances(*startInstances);

    if (auto err = mLaunchPool.Shutdown(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::ProcessStopInstances(const Array<InstanceData>& instances)
{
    LOG_DBG() << "Process stop instances";

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StopInstances(instances);

    if (auto err = mLaunchPool.Shutdown(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::SendRunStatus()
{
    auto status = MakeUnique<InstanceStatusStaticArray>(&mAllocator);

    for (const auto& instance : mCurrentInstances) {
        if (instance.RunError().IsNone()) {
            LOG_DBG() << "Instance status: instanceID=" << instance
                      << ", serviceVersion=" << instance.GetServiceVersion() << ", runState=" << instance.RunState();
        } else {
            LOG_ERR() << "Instance status: instanceID=" << instance
                      << ", serviceVersion=" << instance.GetServiceVersion() << ", runState=" << instance.RunState()
                      << ", err=" << instance.RunError();
        }

        if (auto err = status->PushBack({instance.Info().mInstanceIdent, instance.GetServiceVersion(),
                instance.RunState(), instance.RunError()});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    LOG_DBG() << "Send run status";

    if (auto err = mStatusReceiver->InstancesRunStatus(*status); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::SendOutdatedInstancesStatus(const Array<InstanceData>& instances)
{
    auto status = MakeUnique<InstanceStatusStaticArray>(&mAllocator);

    for (const auto& instance : instances) {
        StaticString<cVersionLen> serviceVersion;

        auto service = GetService(instance.mInstanceInfo.mInstanceIdent.mServiceID);
        if (service.mError.IsNone()) {
            LOG_ERR() << "Can't get service: serviceID=" << instance.mInstanceInfo.mInstanceIdent.mServiceID
                      << ", err=" << service.mError;
        } else {
            serviceVersion = service.mValue->mVersion;
        }

        auto runState = InstanceRunState(InstanceRunStateEnum::eFailed);
        auto runErr   = Error(ErrorEnum::eFailed, "offline timeout");

        LOG_ERR() << "Instance status: instanceID=" << instance.mInstanceID << ", serviceVersion=" << serviceVersion
                  << ", runState=" << runState << ", err=" << runErr;

        if (auto err = status->PushBack({instance.mInstanceInfo.mInstanceIdent, serviceVersion, runState, runErr});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    LOG_DBG() << "Send update status";

    if (auto err = mStatusReceiver->InstancesUpdateStatus(*status); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetStartInstances(
    const Array<InstanceInfo>& desiredInstances, Array<InstanceData>& runningInstances) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get start instances";

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

Error Launcher::GetStopInstances(
    const Array<InstanceData>& startInstances, Array<InstanceData>& stopInstances, bool forceRestart) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get stop instances";

    UniquePtr<servicemanager::ServiceDataStaticArray> services;

    if (!forceRestart) {
        services = MakeUnique<servicemanager::ServiceDataStaticArray>(&mAllocator);

        if (auto err = mServiceManager->GetAllServices(*services); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& instance : mCurrentInstances) {
        auto found = startInstances
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

        if (auto err = stopInstances.EmplaceBack(InstanceData {instance.Info(), instance.InstanceID()});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetCurrentInstances(Array<InstanceData>& instances) const
{
    for (const auto& instance : mCurrentInstances) {
        if (auto err = instances.EmplaceBack(InstanceData {instance.Info(), instance.InstanceID()}); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

void Launcher::StartInstances(const Array<InstanceData>& instances)
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Start instances";

        for (const auto& info : instances) {
            // Skip already started instances
            if (GetInstance(info.mInstanceID).mError.IsNone()) {
                LOG_WRN() << "Instance already started: instanceID=" << info.mInstanceID
                          << ", ident=" << info.mInstanceInfo.mInstanceIdent;

                continue;
            }

            if (auto err = mLaunchPool.AddTask([this, &info](void*) {
                    auto err = StartInstance(info);
                    if (!err.IsNone()) {
                        LOG_ERR() << "Can't start instance: instanceID=" << info.mInstanceID
                                  << ", ident=" << info.mInstanceInfo.mInstanceIdent << ", err=" << err;
                    }
                });
                !err.IsNone()) {
                LOG_ERR() << "Can't start instance: instanceID=" << info.mInstanceID
                          << ", ident=" << info.mInstanceInfo.mInstanceIdent << ", err=" << err;
            }
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Launch pool wait error: err=" << err;
    }
}

void Launcher::StopInstances(const Array<InstanceData>& instances)
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Stop instances";

        for (const auto& info : instances) {
            // Skip already stopped instances
            if (GetInstance(info.mInstanceID).mError.Is(ErrorEnum::eNotFound)) {
                LOG_WRN() << "Instance already stopped: instanceID=" << info.mInstanceID
                          << ", ident=" << info.mInstanceInfo.mInstanceIdent;

                continue;
            }

            if (auto err = mLaunchPool.AddTask([this, &info](void*) {
                    auto err = StopInstance(info.mInstanceID);
                    if (!err.IsNone()) {
                        LOG_ERR() << "Can't stop instance: instanceID=" << info.mInstanceID
                                  << ", ident=" << info.mInstanceInfo.mInstanceIdent << ", err=" << err;
                    }
                });
                !err.IsNone()) {
                LOG_ERR() << "Can't stop instance: instanceID=" << info.mInstanceID
                          << ", ident=" << info.mInstanceInfo.mInstanceIdent << ", err=" << err;
            }
        }
    }

    if (auto err = mLaunchPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Launch pool wait error: err=" << err;
    }
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
    Instance* instance = nullptr;

    {
        LockGuard lock {mMutex};

        if (GetInstance(info.mInstanceID).mError.IsNone()) {
            return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
        }

        if (auto err = mCurrentInstances.EmplaceBack(mConfig, info.mInstanceInfo, info.mInstanceID, *mServiceManager,
                *mLayerManager, *mResourceManager, *mNetworkManager, *mPermHandler, *mRunner, *mRuntime,
                *mResourceMonitor, *mOCIManager, mHostWhiteoutsDir, mNodeInfo);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        instance = &mCurrentInstances[mCurrentInstances.Size() - 1];

        auto findService = GetService(info.mInstanceInfo.mInstanceIdent.mServiceID);
        if (!findService.mError.IsNone()) {
            instance->SetService(nullptr);
            instance->SetRunError(findService.mError);

            return findService.mError;
        }

        instance->SetService(findService.mValue);
    }

    if (auto err = instance->Start(); !err.IsNone()) {
        LockGuard lock {mMutex};

        instance->SetRunError(err);

        return err;
    }

    LOG_INF() << "Instance started: instanceID=" << *instance;

    return ErrorEnum::eNone;
}

Error Launcher::StopInstance(const String& instanceID)
{
    Instance* instance = nullptr;

    {
        LockGuard lock {mMutex};

        Error err;

        Tie(instance, err) = GetInstance(instanceID);
        if (!err.IsNone()) {
            return err;
        }

        mCurrentInstances.RemoveIf([&instanceID](const Instance& inst) { return inst.InstanceID() == instanceID; });
    }

    if (auto err = instance->Stop(); !err.IsNone()) {
        return err;
    }

    LOG_INF() << "Instance stopped: instanceID=" << *instance;

    return ErrorEnum::eNone;
}

void Launcher::OnConnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Cloud connected";

    mOnlineTime = Time::Now();

    if (auto err = mStorage->SetOnlineTime(mOnlineTime); !err.IsNone()) {
        LOG_ERR() << "Can't set online time: err=" << err;
    }

    mConnected = true;
}

void Launcher::OnDisconnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Cloud disconnected";

    if (mConnected) {
        mOnlineTime = Time::Now();

        if (auto err = mStorage->SetOnlineTime(mOnlineTime); err != ErrorEnum::eNone) {
            LOG_ERR() << "Can't set online time: err=" << err;
        }
    }

    mConnected = false;
}

Error Launcher::StopCurrentInstances()
{
    auto stopInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    if (auto err = GetCurrentInstances(*stopInstances); !err.IsNone()) {
        LOG_WRN() << "Error occurred while getting current instances: err=" << err;
    }

    if (auto err = ProcessStopInstances(*stopInstances); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetOutdatedInstances(Array<InstanceData>& instances)
{
    auto now = Time::Now();

    for (const auto& instance : mCurrentInstances) {
        if (instance.GetOfflineTTL() && now.Add(instance.GetOfflineTTL()) < now) {
            if (auto err = instances.EmplaceBack(InstanceData {instance.Info(), instance.InstanceID()});
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::HandleOfflineTTLs()
{
    auto outdatedInstances = SharedPtr<Array<InstanceData>>(&mAllocator, new (&mAllocator) InstanceDataStaticArray());

    {
        UniqueLock lock {mMutex};

        if (mConnected) {
            mOnlineTime = Time::Now();

            if (auto err = mStorage->SetOnlineTime(mOnlineTime); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            return ErrorEnum::eNone;
        }

        mCondVar.Wait(lock, [this] { return !mLaunchInProgress; });

        if (auto err = GetOutdatedInstances(*outdatedInstances); !err.IsNone()) {
            LOG_ERR() << "Can't get outdated instances: err=" << err;
        }

        if (outdatedInstances->IsEmpty()) {
            return ErrorEnum::eNone;
        }

        mLaunchInProgress = true;
    }

    mThread.Join();

    auto err = mThread.Run([this, outdatedInstances](void*) mutable {
        if (auto err = ProcessStopInstances(*outdatedInstances); !err.IsNone()) {
            LOG_ERR() << "Can't process stop instances: err=" << err;
        }

        LockGuard lock {mMutex};

        if (auto err = SendOutdatedInstancesStatus(*outdatedInstances); !err.IsNone()) {
            LOG_ERR() << "Can't send outdated instances status: err=" << err;
        }

        mLaunchInProgress = false;
        mCondVar.NotifyOne();

        ShowResourceUsageStats();
    });
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
