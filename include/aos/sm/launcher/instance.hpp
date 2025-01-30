
/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_INSTANCE_HPP_
#define AOS_INSTANCE_HPP_

#include "aos/common/monitoring/monitoring.hpp"
#include "aos/common/tools/allocator.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/iam/permhandler.hpp"
#include "aos/sm/config.hpp"
#include "aos/sm/launcher/config.hpp"
#include "aos/sm/layermanager.hpp"
#include "aos/sm/networkmanager.hpp"
#include "aos/sm/resourcemanager.hpp"
#include "aos/sm/runner.hpp"
#include "aos/sm/servicemanager.hpp"

namespace aos::sm::launcher {

/**
 * Enable cgroup v2.
 */
constexpr auto cGroupV2 = AOS_CONFIG_LAUNCHER_CGROUP_V2;

/**
 * Runtime interface.
 */
class RuntimeItf {
public:
    /**
     * Creates host FS whiteouts.
     *
     * @param path path to whiteouts.
     * @param hostBinds host binds.
     * @return Error.
     */
    virtual Error CreateHostFSWhiteouts(const String& path, const Array<StaticString<cFilePathLen>>& hostBinds) = 0;

    /**
     * Creates mount points.
     *
     * @param mountPointDir mount point directory.
     * @param mounts mounts to create.
     * @return Error.
     */
    virtual Error CreateMountPoints(const String& mountPointDir, const Array<Mount>& mounts) = 0;

    /**
     * Mounts root FS for Aos service.
     *
     * @param rootfsPath path to service root FS.
     * @param layers layers to mount.
     * @return Error.
     */
    virtual Error MountServiceRootFS(const String& rootfsPath, const Array<StaticString<cFilePathLen>>& layers) = 0;

    /**
     * Umounts Aos service root FS.
     *
     * @param rootfsPath path to service root FS.
     * @return Error.
     */
    virtual Error UmountServiceRootFS(const String& rootfsPath) = 0;

    /**
     * Prepares Aos service storage directory.
     *
     * @param path service storage directory.
     * @param uid user ID.
     * @param gid group ID.
     * @return Error.
     */
    virtual Error PrepareServiceStorage(const String& path, uint32_t uid, uint32_t gid) = 0;

    /**
     * Prepares Aos service state file.
     *
     * @param path service state file path.
     * @param uid user ID.
     * @param gid group ID.
     * @return Error.
     */
    virtual Error PrepareServiceState(const String& path, uint32_t uid, uint32_t gid) = 0;

    /**
     * Prepares directory for network files.
     *
     * @param path network directory path.
     * @return Error.
     */
    virtual Error PrepareNetworkDir(const String& path) = 0;

    /**
     * Returns absolute path of FS item.
     *
     * @param path path to convert.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetAbsPath(const String& path) = 0;

    /**
     * Returns GID by group name.
     *
     * @param groupName group name.
     * @return RetWithError<uint32_t>.
     */
    virtual RetWithError<uint32_t> GetGIDByName(const String& groupName) = 0;

    /**
     * Populates host devices.
     *
     * @param devicePath device path.
     * @param[out] devices OCI devices.
     * @return Error.
     */
    virtual Error PopulateHostDevices(const String& devicePath, Array<oci::LinuxDevice>& devices) = 0;

    /**
     * Destroys runtime interface.
     */
    virtual ~RuntimeItf() = default;
};

/**
 * Launcher instance.
 */
class Instance {
public:
    /**
     * Creates instance.
     *
     * @param config launcher configuration.
     * @param instanceInfo instance info.
     * @param instanceID instance ID.
     * @param serviceManager service manager.
     * @param layerManager layer manager.
     * @param resourceManager resource manager.
     * @param networkManager network manager.
     * @param permHandler permission handler.
     * @param runner runner instance.
     * @param resourceMonitor resource monitor.
     * @param ociManager OCI manager.
     * @param hostWhiteoutsDir host whiteouts directory.
     * @param nodeInfo node info.
     */
    Instance(const Config& config, const InstanceInfo& instanceInfo, const String& instanceID,
        servicemanager::ServiceManagerItf& serviceManager, layermanager::LayerManagerItf& layerManager,
        resourcemanager::ResourceManagerItf& resourceManager, networkmanager::NetworkManagerItf& networkManager,
        iam::permhandler::PermHandlerItf& permHandler, runner::RunnerItf& runner, RuntimeItf& runtime,
        monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager, const String& hostWhiteoutsDir,
        const NodeInfo& nodeInfo);

    /**
     * Starts instance.
     *
     * @return Error
     */
    Error Start();

    /**
     * Stops instance.
     *
     * @return Error
     */
    Error Stop();

    /**
     * Returns instance ID.
     *
     * @return const String& instance ID.
     */
    const String& InstanceID() const { return mInstanceID; };

    /**
     * Returns instance info.
     *
     * @return instance info.
     */
    const InstanceInfo& Info() const { return mInstanceInfo; };

    /**
     * Sets corresponding service.
     *
     * @param service service data.
     */
    void SetService(const servicemanager::ServiceData* service);

    /**
     * Sets run state.
     *
     * @param runState run state.
     */
    void SetRunState(InstanceRunState runState) { mRunState = runState; }

    /**
     * Sets run error.
     *
     * @param error run error.
     */
    void SetRunError(const Error& error)
    {
        mRunState = InstanceRunStateEnum::eFailed;
        mRunError = error;
    }

    /**
     * Returns instance run state.
     *
     * @return const InstanceRunState& run state.
     */
    const InstanceRunState& RunState() const { return mRunState; };

    /**
     * Returns instance error.
     *
     * @return const Error& run error.
     */
    const Error& RunError() const { return mRunError; };

    /**
     * Returns instance service version.
     *
     * @return StaticString<cVersionLen> version.
     */
    StaticString<cVersionLen> GetServiceVersion() const
    {
        if (mService) {
            return mService->mVersion;
        }

        return "";
    };

    /**
     * Returns instance offline TTL.
     *
     * @return Duration.
     */
    Duration GetOfflineTTL() const { return mOfflineTTL; };

    /**
     * Returns instance override env vars.
     *
     * @return const EnvVarsArray&.
     */
    const EnvVarsArray& GetOverrideEnvVars() const { return mOverrideEnvVars; };

    /**
     * Sets instance override env vars.
     *
     * @param envVars
     */
    void SetOverrideEnvVars(const Array<StaticString<cEnvVarNameLen>>& envVars) { mOverrideEnvVars = envVars; };

    /**
     * Returns instances allocator.
     */
    static Allocator& GetAllocator() { return sAllocator; };

    /**
     * Checks if instance is started.
     *
     * @param instanceID instance ID.
     * @return RetWithError<bool>.
     */
    static RetWithError<bool> IsInstanceStarted(const String& instanceID)
    {
        return FS::DirExist(FS::JoinPath(cRuntimeDir, instanceID));
    };

    /**
     * Compares instances.
     *
     * @param instance instance to compare.
     * @return bool.
     */
    bool operator==(const Instance& instance) const { return mInstanceInfo == instance.mInstanceInfo; }

    /**
     * Compares instance info.
     *
     * @param instance instance to compare.
     * @return bool.
     */
    bool operator!=(const Instance& instance) const { return !operator==(instance); }

    /**
     * Compares instance with instance info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfo& info) const { return mInstanceInfo == info; }

    /**
     * Compares instance with instance info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& info) const { return !operator==(info); }

    /**
     * Outputs instance to log.
     *
     * @param log log to output.
     * @param instance instance.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const Instance& instance) { return log << instance.mInstanceID; }

private:
    using LayersStaticArray = StaticArray<StaticString<cFilePathLen>, cMaxNumLayers + 1>;

    static constexpr auto cRuntimeDir = AOS_CONFIG_LAUNCHER_RUNTIME_DIR;
    static constexpr auto cAllocatorSize
        = (sizeof(oci::RuntimeSpec) + sizeof(image::ImageParts)
              + Max(sizeof(networkmanager::InstanceNetworkParameters), sizeof(monitoring::InstanceMonitorParams),
                  sizeof(oci::ImageSpec) + sizeof(oci::ServiceConfig) + sizeof(EnvVarsArray),
                  sizeof(LayersStaticArray) + sizeof(layermanager::LayerData), sizeof(Mount) + sizeof(ResourceInfo),
                  sizeof(Mount) + sizeof(DeviceInfo) + sizeof(StaticArray<oci::LinuxDevice, cMaxNumHostDevices>)))
        * AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES;
    static constexpr auto cNumAllocations  = 8 * AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES;
    static constexpr auto cRuntimeSpecFile = "config.json";
    static constexpr auto cMountPointsDir  = "mounts";
    static constexpr auto cRootFSDir       = "rootfs";
    static constexpr auto cCgroupsPath     = "/system.slice/system-aos\\x2dservice.slice";
    static constexpr auto cLinuxOS         = "linux";

    static constexpr auto cEnvAosServiceID     = "AOS_SERVICE_ID";
    static constexpr auto cEnvAosSubjectID     = "AOS_SUBJECT_ID";
    static constexpr auto cEnvAosInstanceIndex = "AOS_INSTANCE_INDEX";
    static constexpr auto cEnvAosInstanceID    = "AOS_INSTANCE_ID";
    static constexpr auto cEnvAosSecret        = "AOS_SECRET";

    static constexpr auto cDefaultCPUPeriod = 100000;
    static constexpr auto cMinCPUQuota      = 1000;

    static constexpr auto cStatePartitionName   = "state";
    static constexpr auto cStoragePartitionName = "storage";

    static constexpr auto cInstanceStateFile  = "/state.dat";
    static constexpr auto cInstanceStorageDir = "/storage";

    Error  BindHostDirs(oci::RuntimeSpec& runtimeSpec);
    Error  CreateAosEnvVars(oci::RuntimeSpec& runtimeSpec);
    Error  ApplyImageConfig(const oci::ImageSpec& imageSpec, oci::RuntimeSpec& runtimeSpec);
    size_t GetNumCPUCores() const;
    Error  SetResources(const Array<StaticString<cResourceNameLen>>& resources, oci::RuntimeSpec& runtimeSpec);
    Error  SetDevices(const Array<oci::ServiceDevice>& devices, oci::RuntimeSpec& runtimeSpec);
    Error  ApplyServiceConfig(const oci::ServiceConfig& serviceConfig, oci::RuntimeSpec& runtimeSpec);
    Error  ApplyStateStorage(oci::RuntimeSpec& runtimeSpec);
    Error  CreateLinuxSpec(
         const oci::ImageSpec& imageSpec, const oci::ServiceConfig& serviceConfig, oci::RuntimeSpec& runtimeSpec);
    Error CreateVMSpec(const String& serviceFSPath, const oci::ImageSpec& imageSpec, oci::RuntimeSpec& runtimeSpec);
    Error CreateRuntimeSpec(
        const image::ImageParts& imageParts, const oci::ServiceConfig& serviceConfig, oci::RuntimeSpec& runtimeSpec);
    Error SetupMonitoring();
    Error AddNetworkHostsFromResources(const Array<StaticString<cResourceNameLen>>& resources, Array<Host>& hosts);
    Error SetupNetwork(const oci::ServiceConfig& serviceConfig);
    Error PrepareRootFS(const image::ImageParts& imageParts, const Array<Mount>& mounts);

    StaticString<cFilePathLen> GetFullStatePath(const String& path) const
    {
        return FS::JoinPath(mConfig.mStateDir, path);
    }

    StaticString<cFilePathLen> GetFullStoragePath(const String& path) const
    {
        return FS::JoinPath(mConfig.mStorageDir, path);
    }

    static StaticAllocator<cAllocatorSize, cNumAllocations> sAllocator;

    const Config&                        mConfig;
    StaticString<cInstanceIDLen>         mInstanceID;
    InstanceInfo                         mInstanceInfo;
    servicemanager::ServiceManagerItf&   mServiceManager;
    layermanager::LayerManagerItf&       mLayerManager;
    resourcemanager::ResourceManagerItf& mResourceManager;
    networkmanager::NetworkManagerItf&   mNetworkManager;
    iam::permhandler::PermHandlerItf&    mPermHandler;
    runner::RunnerItf&                   mRunner;
    RuntimeItf&                          mRuntime;
    monitoring::ResourceMonitorItf&      mResourceMonitor;
    oci::OCISpecItf&                     mOCIManager;
    const String&                        mHostWhiteoutsDir;
    const NodeInfo&                      mNodeInfo;

    StaticString<cFilePathLen>         mRuntimeDir;
    const servicemanager::ServiceData* mService = nullptr;
    InstanceRunState                   mRunState;
    Error                              mRunError;
    bool                               mPermissionsRegistered = false;
    Duration                           mOfflineTTL            = 0;
    EnvVarsArray                       mOverrideEnvVars;
};

} // namespace aos::sm::launcher

#endif
