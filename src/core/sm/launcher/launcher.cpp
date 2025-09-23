/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/uuid.hpp>

#include "launcher.hpp"

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
// cppcheck-suppress constParameterReference
Error Launcher::Init(const Config& config, iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    servicemanager::ServiceManagerItf& serviceManager, layermanager::LayerManagerItf& layerManager,
    resourcemanager::ResourceManagerItf& resourceManager, networkmanager::NetworkManagerItf& networkManager,
    iam::permhandler::PermHandlerItf& permHandler, runner::RunnerItf& runner, RuntimeItf& runtime,
    monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager,
    InstanceStatusReceiverItf& statusReceiver, ConnectionPublisherItf& connectionPublisher, StorageItf& storage,
    crypto::UUIDItf& uuidProvider)
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
    mUUIDProvider        = &uuidProvider;

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

    mHostWhiteoutsDir = fs::JoinPath(mConfig.mWorkDir, cHostFSWhiteoutsDir);

    if (err = mRuntime->CreateHostFSWhiteouts(mHostWhiteoutsDir, mConfig.mHostBinds); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (Tie(mOnlineTime, err) = mStorage->GetOnlineTime(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = mStorage->GetOverrideEnvVars(mCurrentEnvVars); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LOG_DBG() << "Start launcher";

    if (auto err = mConnectionPublisher->Subscribe(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = RunLastInstances(); !err.IsNone()) {
        LOG_ERR() << "Error running last instances: err=" << err;
    }

    if (auto err = mTimer.Start(
            mConfig.mRemoveOutdatedPeriod,
            [this](void*) {
                if (auto err = HandleOfflineTTLs(); !err.IsNone()) {
                    LOG_ERR() << "Error handling offline TTLs: err=" << err;
                }

                if (auto err = UpdateInstancesEnvVars(); !err.IsNone()) {
                    LOG_ERR() << "Error updating environment variables: err=" << err;
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

    mConnectionPublisher->Unsubscribe(*this);

    if (auto err = mTimer.Stop(); !err.IsNone() && stopError.IsNone()) {
        stopError = AOS_ERROR_WRAP(err);
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

              if (auto err = SendRunStatus(); !err.IsNone()) {
                  LOG_ERR() << "Can't send run status: err=" << err;
              }

              FinishLaunch();
          });
    if (!err.IsNone()) {
        FinishLaunch();

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetCurrentRunStatus(Array<InstanceStatus>& instances) const
{
    UniqueLock lock {mMutex};

    LOG_DBG() << "Get current run status";

    if (auto err = mCondVar.Wait(lock, [this] { return !mLaunchInProgress; }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instance : mCurrentInstances) {
        if (instance.GetError().IsNone()) {
            LOG_DBG() << "Run instance status: instanceID=" << instance
                      << ", serviceVersion=" << instance.GetServiceVersion() << ", state=" << instance.GetState();
        } else {
            LOG_ERR() << "Run instance status: instanceID=" << instance
                      << ", serviceVersion=" << instance.GetServiceVersion() << ", state=" << instance.GetState()
                      << ", err=" << instance.GetError();
        }

        if (auto err = instances.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = instance.ToInstanceStatus(instances.Back()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::OverrideEnvVars(const Array<EnvVarsInstanceInfo>& envVarsInfo, Array<EnvVarsInstanceStatus>& statuses)
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Override environment variables";

        if (auto err = SetEnvVars(envVarsInfo, statuses); !err.IsNone()) {
            return err;
        }
    }

    if (auto err = UpdateInstancesEnvVars(); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error Launcher::UpdateRunStatus(const Array<runner::RunStatus>& instances)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update run status";

    auto status = MakeUnique<InstanceStatusStaticArray>(&mAllocator);

    for (const auto& instance : instances) {
        auto currentInstance = mCurrentInstances.FindIf(
            [&instance](const auto& currentInstance) { return currentInstance.InstanceID() == instance.mInstanceID; });
        if (currentInstance == mCurrentInstances.end()) {
            LOG_WRN() << "Not running instance status received: instanceID=" << instance.mInstanceID;

            continue;
        }

        if (currentInstance->GetState() != instance.mState) {
            currentInstance->SetState(instance.mState);
            currentInstance->SetError(instance.mError);

            if (!mLaunchInProgress) {
                if (currentInstance->GetError().IsNone()) {
                    LOG_DBG() << "Update instance status: instanceID=" << *currentInstance
                              << ", serviceVersion=" << currentInstance->GetServiceVersion()
                              << ", state=" << currentInstance->GetState();
                } else {
                    LOG_ERR() << "Update instance status: instanceID=" << *currentInstance
                              << ", serviceVersion=" << currentInstance->GetServiceVersion()
                              << ", state=" << currentInstance->GetState() << ", err=" << currentInstance->GetError();
                }

                if (auto err = status->EmplaceBack(); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }

                if (auto err = currentInstance->ToInstanceStatus(status->Back()); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }
            }
        }
    }

    if (!status->IsEmpty()) {
        LOG_DBG() << "Send update status";

        if (auto err = mStatusReceiver->InstancesUpdateStatus(*status); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

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

        auto service = GetService(instance.mInstanceInfo.mInstanceIdent.mItemID);
        if (service == mCurrentServices.end()) {
            LOG_ERR() << "Service not found: instanceID=" << instance.mInstanceID;

            continue;
        }

        if (err = mCurrentInstances.EmplaceBack(instance.mInstanceInfo, instance.mInstanceID, *service, mConfig,
                *mServiceManager, *mLayerManager, *mResourceManager, *mNetworkManager, *mPermHandler, *mRunner,
                *mRuntime, *mResourceMonitor, *mOCIManager, mHostWhiteoutsDir, mNodeInfo);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::RunLastInstances()
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Run last instances";

        if (mLaunchInProgress) {
            return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
        }

        mLaunchInProgress = true;
    }

    // Wait in case previous request is not yet finished
    mThread.Join();

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    if (auto err = mThread.Run([this](void*) {
            if (auto err = ProcessLastInstances(); !err.IsNone()) {
                LOG_ERR() << "Can't process last instances: err=" << err;
            }

            FinishLaunch();
        });
        !err.IsNone()) {
        FinishLaunch();

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::ProcessServices(const Array<ServiceInfo>& services)
{
    LOG_DBG() << "Process services";

    auto serviceStatuses = MakeUnique<ServiceStatusStaticArray>(&mAllocator);

    auto err = mServiceManager->ProcessDesiredServices(services, *serviceStatuses);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::ProcessLayers(const Array<LayerInfo>& layers)
{
    LOG_DBG() << "Process layers";

    auto layerStatuses = MakeUnique<LayerStatusStaticArray>(&mAllocator);

    auto err = mLayerManager->ProcessDesiredLayers(layers, *layerStatuses);
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

    if (auto err = GetCurrentInstances(*stopInstances); !err.IsNone()) {
        LOG_WRN() << "Error occurred while getting current instances: err=" << err;
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

Error Launcher::ProcessInstances(const Array<InstanceInfo>& instances, bool forceRestart)
{
    LOG_DBG() << "Process instances: restart=" << forceRestart;

    auto desiredInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    if (auto err = GetDesiredInstancesData(instances, *desiredInstances); !err.IsNone()) {
        return err;
    }

    auto startInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);
    auto stopInstances  = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    CacheServices(*desiredInstances);

    if (auto err = CalculateInstances(*desiredInstances, forceRestart, *startInstances, *stopInstances);
        !err.IsNone()) {
        return err;
    }

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StopInstances(*stopInstances);
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

Error Launcher::ProcessRestartInstances(const Array<InstanceData>& instances)
{
    LOG_DBG() << "Process restart instances";

    if (auto err = mLaunchPool.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    RestartInstances(instances);

    if (auto err = mLaunchPool.Shutdown(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::SendRunStatus()
{
    auto status = MakeUnique<InstanceStatusStaticArray>(&mAllocator);

    for (const auto& instance : mCurrentInstances) {
        if (instance.GetError().IsNone()) {
            LOG_DBG() << "Run instance status: instanceID=" << instance
                      << ", serviceVersion=" << instance.GetServiceVersion() << ", state=" << instance.GetState();
        } else {
            LOG_ERR() << "Run instance status: instanceID=" << instance
                      << ", serviceVersion=" << instance.GetServiceVersion() << ", state=" << instance.GetState()
                      << ", err=" << instance.GetError();
        }

        if (auto err = status->EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = instance.ToInstanceStatus(status->Back()); !err.IsNone()) {
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
    LockGuard lock {mMutex};

    auto status = MakeUnique<InstanceStatusStaticArray>(&mAllocator);

    for (const auto& instance : instances) {
        StaticString<cVersionLen> serviceVersion;

        auto service = GetService(instance.mInstanceInfo.mInstanceIdent.mItemID);
        if (service == mCurrentServices.end()) {
            LOG_ERR() << "Service not found: serviceID=" << instance.mInstanceInfo.mInstanceIdent.mItemID;
        } else {
            serviceVersion = service->mVersion;
        }

        auto state       = InstanceState(InstanceStateEnum::eFailed);
        auto instanceErr = Error(ErrorEnum::eFailed, "offline timeout");

        LOG_ERR() << "Instance status: instanceID=" << instance.mInstanceID << ", serviceVersion=" << serviceVersion
                  << ", state=" << state << ", err=" << instanceErr;

        if (auto err = status->EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        status->Back().mInstanceIdent = instance.mInstanceInfo.mInstanceIdent;
        status->Back().mVersion       = serviceVersion;
        status->Back().mState         = state;
        status->Back().mError         = instanceErr;
    }

    LOG_DBG() << "Send update status";

    if (auto err = mStatusReceiver->InstancesUpdateStatus(*status); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetDesiredInstancesData(
    const Array<InstanceInfo>& desiredInstancesInfo, Array<InstanceData>& desiredInstancesData) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get desired instances data";

    auto currentInstances = MakeUnique<InstanceDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllInstances(*currentInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instanceInfo : desiredInstancesInfo) {
        auto currentInstance = currentInstances->FindIf([&instanceInfo](const InstanceData& instance) {
            return instance.mInstanceInfo.mInstanceIdent == instanceInfo.mInstanceIdent;
        });
        if (currentInstance == currentInstances->end()) {
            auto [uuid, err] = mUUIDProvider->CreateUUIDv4();
            if (!err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            const auto instanceID = uuid::UUIDToString(uuid);

            err = desiredInstancesData.EmplaceBack(instanceInfo, instanceID);
            if (!err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            err = mStorage->AddInstance(desiredInstancesData.Back());
            if (!err.IsNone()) {
                LOG_ERR() << "Can't add instance" << Log::Field("instanceID", instanceID) << Log::Field(err);
            }

            continue;
        }

        if (auto err = desiredInstancesData.EmplaceBack(instanceInfo, currentInstance->mInstanceID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        // Update instance if parameters are changed
        if (currentInstance->mInstanceInfo != instanceInfo) {
            if (auto err = mStorage->UpdateInstance(desiredInstancesData.Back()); !err.IsNone()) {
                LOG_ERR() << "Can't update instance: instanceID=" << desiredInstancesData.Back().mInstanceID
                          << ", err=" << err;
            }
        }

        currentInstances->Erase(currentInstance);
    }

    // Remove old instances

    for (const auto& currentInstance : *currentInstances) {
        if (auto err = mStorage->RemoveInstance(currentInstance.mInstanceID); !err.IsNone()) {
            LOG_ERR() << "Can't remove instance: instanceID=" << currentInstance.mInstanceID << ", err=" << err;
        }
    }

    // Sort by priority

    auto tmpValue = MakeUnique<InstanceData>(&mAllocator);

    desiredInstancesData.Sort(
        [](const InstanceData& instance1, const InstanceData& instance2) {
            return instance1.mInstanceInfo.mPriority > instance2.mInstanceInfo.mPriority;
        },
        *tmpValue);

    return ErrorEnum::eNone;
}

Error Launcher::CalculateInstances(const Array<InstanceData>& desiredInstancesData, bool forceRestart,
    Array<InstanceData>& startInstances, Array<InstanceData>& stopInstances)
{
    for (const auto& desiredInstance : desiredInstancesData) {
        auto currentInstance = mCurrentInstances.FindIf([&desiredInstance](const Instance& instance) {
            return instance.Info().mInstanceIdent == desiredInstance.mInstanceInfo.mInstanceIdent;
        });

        if (currentInstance == mCurrentInstances.end()) {
            if (auto err = startInstances.EmplaceBack(desiredInstance); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            continue;
        }

        auto compareInfo = MakeUnique<InstanceInfo>(&mAllocator, currentInstance->Info());

        compareInfo->mPriority = desiredInstance.mInstanceInfo.mPriority;

        auto service = GetService(desiredInstance.mInstanceInfo.mInstanceIdent.mItemID);

        if ((*compareInfo != desiredInstance.mInstanceInfo) || service == mCurrentServices.end()
            || service->mVersion != currentInstance->GetServiceVersion() || forceRestart) {
            if (auto err = stopInstances.EmplaceBack(currentInstance->Info(), currentInstance->InstanceID());
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (auto err = startInstances.EmplaceBack(desiredInstance); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    for (const auto& currentInstance : mCurrentInstances) {
        if (desiredInstancesData.FindIf([&currentInstance](const InstanceData& instance) {
                return instance.mInstanceInfo.mInstanceIdent == currentInstance.Info().mInstanceIdent;
            })
            == desiredInstancesData.end()) {
            if (auto err = stopInstances.EmplaceBack(currentInstance.Info(), currentInstance.InstanceID());
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetCurrentInstances(Array<InstanceData>& instances) const
{
    for (const auto& instance : mCurrentInstances) {
        if (auto err = instances.EmplaceBack(instance.Info(), instance.InstanceID()); !err.IsNone()) {
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
            if (GetInstance(info.mInstanceID) != mCurrentInstances.end()) {
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
            auto instance = GetInstance(info.mInstanceID);
            if (instance == mCurrentInstances.end()) {
                LOG_WRN() << "Instance already stopped: instanceID=" << info.mInstanceID
                          << ", ident=" << info.mInstanceInfo.mInstanceIdent;

                continue;
            }

            if (auto err = mLaunchPool.AddTask([this, &info, instance](void*) {
                    auto err = StopInstance(instance->InstanceID());
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

void Launcher::RestartInstances(const Array<InstanceData>& instances)
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Restart instances";

        for (const auto& info : instances) {
            if (auto err = mLaunchPool.AddTask([this, &info](void*) {
                    if (GetInstance(info.mInstanceID) != mCurrentInstances.end()) {
                        if (auto err = StopInstance(info.mInstanceID); !err.IsNone()) {
                            LOG_ERR() << "Can't stop instance: instanceID=" << info.mInstanceID
                                      << ", ident=" << info.mInstanceInfo.mInstanceIdent << ", err=" << err;
                        }
                    } else {
                        LOG_WRN() << "Instance already stopped: instanceID=" << info.mInstanceID
                                  << ", ident=" << info.mInstanceInfo.mInstanceIdent;
                    }

                    if (GetInstance(info.mInstanceID) == mCurrentInstances.end()) {
                        if (auto err = StartInstance(info); !err.IsNone()) {
                            LOG_ERR() << "Can't start instance: instanceID=" << info.mInstanceID
                                      << ", ident=" << info.mInstanceInfo.mInstanceIdent << ", err=" << err;
                        }
                    } else {
                        LOG_WRN() << "Instance already started: instanceID=" << info.mInstanceID
                                  << ", ident=" << info.mInstanceInfo.mInstanceIdent;
                    }
                });
                !err.IsNone()) {
                LOG_ERR() << "Can't restart instance: instanceID=" << info.mInstanceID
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
        const auto& serviceID = instance.mInstanceInfo.mInstanceIdent.mItemID;

        if (GetService(serviceID) != mCurrentServices.end()) {
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

    LOG_DBG() << "Services cached: count=" << mCurrentServices.Size();
}

Error Launcher::StartInstance(const InstanceData& info)
{
    Instance* instance = nullptr;

    {
        LockGuard lock {mMutex};

        if (GetInstance(info.mInstanceID) != mCurrentInstances.end()) {
            return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
        }

        auto service = GetService(info.mInstanceInfo.mInstanceIdent.mItemID);
        if (service == mCurrentServices.end()) {
            if (auto err
                = mCurrentInstances.EmplaceBack(info.mInstanceInfo, info.mInstanceID, servicemanager::ServiceData {},
                    mConfig, *mServiceManager, *mLayerManager, *mResourceManager, *mNetworkManager, *mPermHandler,
                    *mRunner, *mRuntime, *mResourceMonitor, *mOCIManager, mHostWhiteoutsDir, mNodeInfo);
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            auto err = Error(ErrorEnum::eNotFound, "service not found");

            mCurrentInstances.Back().SetState(InstanceStateEnum::eFailed);
            mCurrentInstances.Back().SetError(err);

            return AOS_ERROR_WRAP(err);
        }

        if (auto err = mCurrentInstances.EmplaceBack(info.mInstanceInfo, info.mInstanceID, *service, mConfig,
                *mServiceManager, *mLayerManager, *mResourceManager, *mNetworkManager, *mPermHandler, *mRunner,
                *mRuntime, *mResourceMonitor, *mOCIManager, mHostWhiteoutsDir, mNodeInfo);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        instance = &mCurrentInstances.Back();

        auto envVars = MakeUnique<EnvVarsStaticArray>(&mAllocator);

        if (auto err = GetInstanceEnvVars(info.mInstanceInfo.mInstanceIdent, *envVars); !err.IsNone()) {
            return err;
        }

        instance->SetOverrideEnvVars(*envVars);
    }

    if (auto err = instance->Start(); !err.IsNone()) {
        LockGuard lock {mMutex};

        instance->SetState(InstanceStateEnum::eFailed);
        instance->SetError(err);

        return AOS_ERROR_WRAP(err);
    }

    LOG_INF() << "Instance started: instanceID=" << *instance;

    return ErrorEnum::eNone;
}

Error Launcher::StopInstance(const String& instanceID)
{
    List<Instance>::Iterator instance;

    {
        LockGuard lock {mMutex};

        if (instance = GetInstance(instanceID); instance == mCurrentInstances.end()) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }
    }

    [[maybe_unused]] auto removeInstance = DeferRelease(&instance, [this, it = instance](Instance* instance) {
        (void)instance;

        LockGuard lock {mMutex};

        mCurrentInstances.Remove(it);
    });

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
    {
        UniqueLock lock {mMutex};

        if (auto err = mCondVar.Wait(lock, [this] { return !mLaunchInProgress; }); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        mLaunchInProgress = true;
    }

    auto finishLaunch = DeferRelease(&mLaunchInProgress, [this](bool*) { FinishLaunch(); });

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
        if (instance.GetOfflineTTL() && now.Add(instance.GetOfflineTTL().Nanoseconds()) < now) {
            if (auto err = instances.EmplaceBack(instance.Info(), instance.InstanceID()); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::HandleOfflineTTLs()
{
    SharedPtr<InstanceDataStaticArray> outdatedInstances;

    LOG_DBG() << "Handle offline TTLs";

    {
        UniqueLock lock {mMutex};

        if (mConnected) {
            mOnlineTime = Time::Now();

            if (auto err = mStorage->SetOnlineTime(mOnlineTime); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            return ErrorEnum::eNone;
        }

        if (auto err = mCondVar.Wait(lock, [this] { return !mLaunchInProgress; }); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        outdatedInstances = MakeShared<InstanceDataStaticArray>(&mAllocator);

        if (auto err = GetOutdatedInstances(*outdatedInstances); !err.IsNone()) {
            LOG_ERR() << "Can't get outdated instances: err=" << err;
        }

        if (outdatedInstances->IsEmpty()) {
            return ErrorEnum::eNone;
        }

        mLaunchInProgress = true;
    }

    mThread.Join();

    // cppcheck-suppress cstyleCast
    auto err = mThread.Run([this, outdatedInstances](void*) mutable {
        if (auto err = ProcessStopInstances(*outdatedInstances); !err.IsNone()) {
            LOG_ERR() << "Can't process stop instances: err=" << err;
        }

        if (auto err = SendOutdatedInstancesStatus(*outdatedInstances); !err.IsNone()) {
            LOG_ERR() << "Can't send outdated instances status: err=" << err;
        }

        FinishLaunch();
    });
    if (!err.IsNone()) {
        FinishLaunch();

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::SetEnvVars(const Array<EnvVarsInstanceInfo>& envVarsInfo, Array<EnvVarsInstanceStatus>& statuses)
{
    (void)statuses;

    auto now = Time::Now();

    for (const auto& envVarInfo : envVarsInfo) {

        for (const auto& envVar : envVarInfo.mVariables) {
            LOG_DBG() << "Override env var: filter=" << static_cast<InstanceFilter>(envVarInfo)
                      << ", name=" << envVar.mName << ", value=" << envVar.mValue
                      << ", ttl=" << (envVar.mTTL.HasValue() ? *envVar.mTTL : Time());

            if (envVar.mTTL.HasValue() && *envVar.mTTL < now) {
                Error envVarErr(ErrorEnum::eTimeout, "environment variable expired");

                LOG_ERR() << "Error overriding environment variable: name=" << envVar.mName << ", err=" << envVarErr;
            }
        }
    }

    if (mCurrentEnvVars == envVarsInfo) {
        return ErrorEnum::eNone;
    }

    mCurrentEnvVars = envVarsInfo;

    if (auto err = mStorage->SetOverrideEnvVars(envVarsInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::RemoveOutdatedEnvVars()
{
    LOG_DBG() << "Remove outdated env vars";

    auto now     = Time::Now();
    auto updated = false;

    for (auto& instanceEnvVars : mCurrentEnvVars) {
        if (instanceEnvVars.mVariables.RemoveIf([&now](const EnvVarInfo& envVar) {
                if (envVar.mTTL.HasValue() && *envVar.mTTL < now) {
                    LOG_DBG() << "Remove outdated env var: name=" << envVar.mName << ", value=" << envVar.mValue;

                    return true;
                }

                return false;
            })) {
            updated = true;
        }
    }

    mCurrentEnvVars.RemoveIf(
        [](const EnvVarsInstanceInfo& instanceEnvVars) { return instanceEnvVars.mVariables.IsEmpty(); });

    if (updated) {
        if (auto err = mStorage->SetOverrideEnvVars(mCurrentEnvVars); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetInstanceEnvVars(const InstanceIdent& instanceIdent, Array<StaticString<cEnvVarLen>>& envVars) const
{
    for (const auto& currentEnvVar : mCurrentEnvVars) {
        if (static_cast<InstanceFilter>(currentEnvVar).Match(instanceIdent)) {
            for (const auto& envVar : currentEnvVar.mVariables) {
                StaticString<cEnvVarLen> fullEnvVar;

                if (auto err = fullEnvVar.Format("%s=%s", envVar.mName.CStr(), envVar.mValue.CStr()); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }

                if (auto err = envVars.PushBack(fullEnvVar); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetEnvChangedInstances(Array<InstanceData>& instances) const
{
    for (auto& instance : mCurrentInstances) {
        auto envVars = MakeUnique<EnvVarsStaticArray>(&mAllocator);

        if (auto err = GetInstanceEnvVars(instance.Info().mInstanceIdent, *envVars); !err.IsNone()) {
            LOG_ERR() << "Can't get instance env vars: instanceID=" << instance.InstanceID() << ", err=" << err;

            continue;
        }

        if (*envVars == instance.GetOverrideEnvVars()) {
            continue;
        }

        if (auto err = instances.EmplaceBack(instance.Info(), instance.InstanceID()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::SendEnvChangedInstancesStatus(const Array<InstanceData>& instances)
{
    LockGuard lock {mMutex};

    auto status = MakeUnique<InstanceStatusStaticArray>(&mAllocator);

    for (const auto& instanceData : instances) {
        auto instance = GetInstance(instanceData.mInstanceID);
        if (instance == mCurrentInstances.end()) {
            LOG_ERR() << "Can't get instance: instanceID=" << instanceData.mInstanceID
                      << ", err=" << Error(ErrorEnum::eNotFound);

            continue;
        }

        if (instance->GetError().IsNone()) {
            LOG_DBG() << "Run instance status: instanceID=" << *instance
                      << ", serviceVersion=" << instance->GetServiceVersion() << ", state=" << instance->GetState();
        } else {
            LOG_ERR() << "Run instance status: instanceID=" << *instance
                      << ", serviceVersion=" << instance->GetServiceVersion() << ", state=" << instance->GetState()
                      << ", err=" << instance->GetError();
        }

        if (auto err = status->EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = instance->ToInstanceStatus(status->Back()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    LOG_DBG() << "Send update status";

    if (auto err = mStatusReceiver->InstancesUpdateStatus(*status); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::UpdateInstancesEnvVars()
{
    SharedPtr<InstanceDataStaticArray> restartInstances;

    {
        UniqueLock lock {mMutex};

        if (auto err = RemoveOutdatedEnvVars(); !err.IsNone()) {
            return err;
        }

        if (auto err = mCondVar.Wait(lock, [this] { return !mLaunchInProgress; }); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        restartInstances = MakeShared<InstanceDataStaticArray>(&mAllocator);

        if (auto err = GetEnvChangedInstances(*restartInstances); !err.IsNone()) {
            return err;
        }

        if (restartInstances->IsEmpty()) {
            return ErrorEnum::eNone;
        }

        mLaunchInProgress = true;
    }

    mThread.Join();

    // cppcheck-suppress cstyleCast
    auto err = mThread.Run([this, restartInstances](void*) mutable {
        if (auto err = ProcessRestartInstances(*restartInstances); !err.IsNone()) {
            LOG_ERR() << "Can't process stop instances: err=" << err;
        }

        if (auto err = SendEnvChangedInstancesStatus(*restartInstances); !err.IsNone()) {
            LOG_ERR() << "Can't send env changed instances status: err=" << err;
        }

        FinishLaunch();
    });
    if (!err.IsNone()) {
        FinishLaunch();

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Launcher::FinishLaunch()
{
    LockGuard lock {mMutex};

    mLaunchInProgress = false;
    mCondVar.NotifyAll();

    ShowResourceUsageStats();
}

} // namespace aos::sm::launcher
