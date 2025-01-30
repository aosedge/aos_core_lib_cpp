/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAUNCHER_HPP_
#define AOS_LAUNCHER_HPP_

#include <assert.h>

#include "aos/common/cloudprotocol/envvars.hpp"
#include "aos/common/connectionsubsc.hpp"
#include "aos/common/monitoring/monitoring.hpp"
#include "aos/common/ocispec/ocispec.hpp"
#include "aos/common/tools/list.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/types.hpp"
#include "aos/sm/config.hpp"
#include "aos/sm/launcher/config.hpp"
#include "aos/sm/launcher/instance.hpp"
#include "aos/sm/layermanager.hpp"
#include "aos/sm/networkmanager.hpp"
#include "aos/sm/runner.hpp"
#include "aos/sm/servicemanager.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Instance launcher interface.
 */
class LauncherItf {
public:
    /**
     * Runs specified instances.
     *
     * @param instances instance info array.
     * @param forceRestart forces restart already started instance.
     * @return Error.
     */
    virtual Error RunInstances(const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
        const Array<InstanceInfo>& instances, bool forceRestart)
        = 0;

    /**
     * Returns current run status.
     *
     * @param instances instances status.
     * @return Error.
     */
    virtual Error GetCurrentRunStatus(Array<InstanceStatus>& instances) const = 0;

    /**
     * Overrides environment variables for specified instances.
     *
     * @param envVarsInfo environment variables info.
     * @param statuses[out] environment variables statuses.
     * @return Error
     */
    virtual Error OverrideEnvVars(const Array<cloudprotocol::EnvVarsInstanceInfo>& envVarsInfo,
        Array<cloudprotocol::EnvVarsInstanceStatus>&                               statuses)
        = 0;
};

/**
 * Interface to send instances run status.
 */
class InstanceStatusReceiverItf {
public:
    /**
     * Sends instances run status.
     *
     * @param instances instances status array.
     * @return Error.
     */
    virtual Error InstancesRunStatus(const Array<InstanceStatus>& instances) = 0;

    /**
     * Sends instances update status.
     * @param instances instances status array.
     *
     * @return Error.
     */
    virtual Error InstancesUpdateStatus(const Array<InstanceStatus>& instances) = 0;
};

/**
 * Instance data.
 */
struct InstanceData {
public:
    InstanceInfo                 mInstanceInfo;
    StaticString<cInstanceIDLen> mInstanceID;

    /**
     * Default constructor.
     */
    InstanceData() = default;

    /**
     * Creates instance data.
     *
     * @param info instance info.
     * @param instanceID instance ID.
     */
    InstanceData(const InstanceInfo& info, const String& instanceID)
        : mInstanceInfo(info)
        , mInstanceID(instanceID)
    {
    }

    /**
     * Compares instance data.
     *
     * @param data instance data to compare.
     * @return bool.
     */
    bool operator==(const InstanceData& data) const
    {
        return mInstanceInfo == data.mInstanceInfo && mInstanceID == data.mInstanceID;
    }

    /**
     * Compares instance data.
     *
     * @param data instance data to compare.
     * @return bool.
     */
    bool operator!=(const InstanceData& data) const { return !operator==(data); }
};

using InstanceDataStaticArray = StaticArray<InstanceData, cMaxNumInstances>;

/**
 * Launcher storage interface.
 */
class StorageItf {
public:
    /**
     * Adds new instance to storage.
     *
     * @param instance instance to add.
     * @return Error.
     */
    virtual Error AddInstance(const InstanceData& instance) = 0;

    /**
     * Updates previously stored instance.
     *
     * @param instance instance to update.
     * @return Error.
     */
    virtual Error UpdateInstance(const InstanceData& instance) = 0;

    /**
     * Removes previously stored instance.
     *
     * @param instanceID instance ID to remove.
     * @return Error.
     */
    virtual Error RemoveInstance(const String& instanceID) = 0;

    /**
     * Returns all stored instances.
     *
     * @param instances array to return stored instances.
     * @return Error.
     */
    virtual Error GetAllInstances(Array<InstanceData>& instances) = 0;

    /**
     * Returns operation version.
     *
     * @return RetWithError<uint64_t>.
     */
    virtual RetWithError<uint64_t> GetOperationVersion() const = 0;

    /**
     * Sets operation version.
     *
     * @param version operation version.
     * @return Error.
     */
    virtual Error SetOperationVersion(uint64_t version) = 0;

    /**
     * Returns instances's override environment variables array.
     *
     * @param envVarsInstanceInfos[out] instances's override environment variables array.
     * @return Error.
     */
    virtual Error GetOverrideEnvVars(Array<cloudprotocol::EnvVarsInstanceInfo>& envVarsInstanceInfos) const = 0;

    /**
     * Sets instances's override environment variables array.
     *
     * @param envVarsInstanceInfos instances's override environment variables array.
     * @return Error.
     */
    virtual Error SetOverrideEnvVars(const Array<cloudprotocol::EnvVarsInstanceInfo>& envVarsInstanceInfos) = 0;

    /**
     * Returns online time.
     *
     * @return RetWithError<Time>.
     */
    virtual RetWithError<Time> GetOnlineTime() const = 0;

    /**
     * Sets online time.
     *
     * @param time online time.
     * @return Error.
     */
    virtual Error SetOnlineTime(const Time& time) = 0;

    /**
     * Destroys storage interface.
     */
    virtual ~StorageItf() = default;
};

/**
 * Launches service instances.
 */
class Launcher : public LauncherItf,
                 public runner::RunStatusReceiverItf,
                 private ConnectionSubscriberItf,
                 private NonCopyable {
public:
    /**
     * Creates launcher instance.
     */
    Launcher() = default;

    /**
     * Destroys launcher instance.
     */
    ~Launcher() = default;

    /**
     * Initializes launcher.
     *
     * @param config launcher configuration.
     * @param nodeInfoProvider node info provider.
     * @param serviceManager service manager instance.
     * @param layerManager layer manager instance.
     * @param resourceManager resource manager instance.
     * @param networkManager network manager instance.
     * @param permHandler permission handler instance.
     * @param runner runner instance.
     * @param runtime runtime instance.
     * @param resourceMonitor resource monitor instance.
     * @param ociManager OCI manager instance.
     * @param statusReceiver status receiver instance.
     * @param connectionPublisher connection publisher instance.
     * @param storage storage instance.
     * @return Error.
     */
    Error Init(const Config& config, iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        servicemanager::ServiceManagerItf& serviceManager, layermanager::LayerManagerItf& layerManager,
        resourcemanager::ResourceManagerItf& resourceManager, networkmanager::NetworkManagerItf& networkManager,
        iam::permhandler::PermHandlerItf& permHandler, runner::RunnerItf& runner, RuntimeItf& runtime,
        monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager,
        InstanceStatusReceiverItf& statusReceiver, ConnectionPublisherItf& connectionPublisher, StorageItf& storage);

    /**
     * Starts launcher.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops launcher.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Runs specified instances.
     *
     * @param services services info array.
     * @param layers layers info array.
     * @param instances instances info array.
     * @param forceRestart forces restart already started instance.
     * @return Error.
     */
    Error RunInstances(const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
        const Array<InstanceInfo>& instances, bool forceRestart = false) override;

    /**
     * Returns current run status.
     *
     * @param instances instances status.
     * @return Error.
     */
    Error GetCurrentRunStatus(Array<InstanceStatus>& instances) const override;

    /**
     * Overrides environment variables for specified instances.
     *
     * @param envVarsInfo environment variables info.
     * @param statuses[out] environment variables statuses.
     * @return Error
     */
    Error OverrideEnvVars(const Array<cloudprotocol::EnvVarsInstanceInfo>& envVarsInfo,
        Array<cloudprotocol::EnvVarsInstanceStatus>&                       statuses) override;

    /**
     * Updates run instances status.
     *
     * @param instances instances state.
     * @return Error.
     */
    Error UpdateRunStatus(const Array<runner::RunStatus>& instances) override;

    /**
     * Notifies publisher is connected.
     */
    void OnConnect() override;

    /**
     * Notifies publisher is disconnected.
     */
    void OnDisconnect() override;

    /**
     * Defines current operation version
     * if new functionality doesn't allow existing services to work properly,
     * this value should be increased.
     * It will force to remove all services and their storages before first start.
     */
    static constexpr uint32_t cOperationVersion = 10;

private:
    static constexpr auto cNumLaunchThreads = AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES;
    static constexpr auto cThreadTaskSize   = AOS_CONFIG_LAUNCHER_THREAD_TASK_SIZE;
    static constexpr auto cThreadStackSize  = AOS_CONFIG_LAUNCHER_THREAD_STACK_SIZE;

    static constexpr auto cHostFSWhiteoutsDir = "whiteouts";

    static constexpr auto cAllocatorSize
        = Max(sizeof(InstanceInfoStaticArray) + sizeof(InstanceDataStaticArray) * 2 + sizeof(ServiceInfoStaticArray)
                + sizeof(LayerInfoStaticArray) + sizeof(servicemanager::ServiceDataStaticArray)
                + sizeof(InstanceStatusStaticArray) + sizeof(servicemanager::ServiceData) + sizeof(InstanceData),
            sizeof(EnvVarsArray) + sizeof(InstanceStatusStaticArray) + sizeof(InstanceDataStaticArray));

    void  ShowResourceUsageStats();
    Error ProcessLastInstances();
    Error ProcessInstances(const Array<InstanceInfo>& instances, bool forceRestart = false);
    Error ProcessServices(const Array<ServiceInfo>& services);
    Error ProcessLayers(const Array<LayerInfo>& layers);
    Error ProcessStopInstances(const Array<InstanceData>& instances);
    Error ProcessRestartInstances(const Array<InstanceData>& instances);
    Error SendRunStatus();
    Error SendOutdatedInstancesStatus(const Array<InstanceData>& instances);
    void  StopInstances(const Array<InstanceData>& instances);
    void  StartInstances(const Array<InstanceData>& instances);
    void  RestartInstances(const Array<InstanceData>& instances);
    void  CacheServices(const Array<InstanceData>& instances);
    void  UpdateInstanceServices();
    Error GetStartInstances(const Array<InstanceInfo>& desiredInstances, Array<InstanceData>& startInstances) const;
    Error GetStopInstances(
        const Array<InstanceData>& startInstances, Array<InstanceData>& stopInstances, bool forceRestart) const;
    Error GetCurrentInstances(Array<InstanceData>& instances) const;
    Error StartInstance(const InstanceData& info);
    Error StopInstance(const String& instanceID);
    Error FillCurrentInstance(const Array<InstanceData>& instances);
    Error RunLastInstances();
    Error StopCurrentInstances();
    Error GetOutdatedInstances(Array<InstanceData>& instances);
    Error HandleOfflineTTLs();
    Error SetEnvVars(const Array<cloudprotocol::EnvVarsInstanceInfo>& envVarsInfo,
        Array<cloudprotocol::EnvVarsInstanceStatus>&                  statuses);
    Error GetInstanceEnvVars(const InstanceIdent& instanceIdent, Array<StaticString<cEnvVarNameLen>>& envVars) const;
    Error RemoveOutdatedEnvVars();
    Error GetEnvChangedInstances(Array<InstanceData>& instance) const;
    Error SendEnvChangedInstancesStatus(const Array<InstanceData>& instances);
    Error UpdateInstancesEnvVars();

    servicemanager::ServiceData* GetService(const String& serviceID)
    {
        return mCurrentServices.FindIf(
            [&serviceID](const servicemanager::ServiceData& service) { return serviceID == service.mServiceID; });
    }

    List<Instance>::Iterator GetInstance(const String& instanceID)
    {
        return mCurrentInstances.FindIf(
            [&instanceID](const Instance& instance) { return instanceID == instance.InstanceID(); });
    }

    Config                               mConfig;
    ConnectionPublisherItf*              mConnectionPublisher {};
    InstanceStatusReceiverItf*           mStatusReceiver {};
    layermanager::LayerManagerItf*       mLayerManager {};
    networkmanager::NetworkManagerItf*   mNetworkManager {};
    iam::permhandler::PermHandlerItf*    mPermHandler {};
    monitoring::ResourceMonitorItf*      mResourceMonitor {};
    oci::OCISpecItf*                     mOCIManager {};
    resourcemanager::ResourceManagerItf* mResourceManager {};
    runner::RunnerItf*                   mRunner {};
    servicemanager::ServiceManagerItf*   mServiceManager {};
    StorageItf*                          mStorage {};
    RuntimeItf*                          mRuntime {};

    mutable StaticAllocator<cAllocatorSize> mAllocator;

    bool                                      mLaunchInProgress = false;
    mutable Mutex                             mMutex;
    Thread<cThreadTaskSize, cThreadStackSize> mThread;
    ThreadPool<cNumLaunchThreads, Max(cMaxNumInstances * 2, cMaxNumServices, cMaxNumLayers), cThreadTaskSize,
        cThreadStackSize>
                                mLaunchPool;
    bool                        mConnected = false;
    mutable ConditionalVariable mCondVar;

    StaticArray<servicemanager::ServiceData, cMaxNumServices> mCurrentServices;
    StaticList<Instance, cMaxNumInstances>                    mCurrentInstances;
    cloudprotocol::EnvVarsInstanceInfoArray                   mCurrentEnvVars;
    StaticString<cFilePathLen>                                mHostWhiteoutsDir;
    NodeInfo                                                  mNodeInfo;
    Time                                                      mOnlineTime;
    Timer                                                     mTimer;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
