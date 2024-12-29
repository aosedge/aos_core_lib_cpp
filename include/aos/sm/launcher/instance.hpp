
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
#include "aos/sm/config.hpp"
#include "aos/sm/launcher/config.hpp"
#include "aos/sm/launcher/service.hpp"
#include "aos/sm/layermanager.hpp"
#include "aos/sm/networkmanager.hpp"
#include "aos/sm/runner.hpp"

namespace aos::sm::launcher {

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
     * Prepares root FS for Aos service.
     *
     * @param rootfsPath path to service root FS.
     * @param mountPointDir mount point directory.
     * @param mounts mounts to prepare.
     * @param layers layers to prepare.
     * @return Error.
     */
    virtual Error PrepareServiceRootFS(const String& rootfsPath, const String& mountPointDir,
        const Array<oci::Mount>& mounts, const Array<StaticString<cFilePathLen>>& layers)
        = 0;

    /**
     * Releases Aos service root FS.
     *
     * @param runtimeDir service runtime directory.
     * @return Error.
     */
    virtual Error ReleaseServiceRootFS(const String& runtimeDir) = 0;

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
     */
    Instance() = default;

    /**
     * Creates instance.
     *
     * @param config launcher configuration.
     * @param instanceInfo instance info.
     * @param instanceID instance ID.
     * @param serviceManager service manager.
     * @param layerManager layer manager.
     * @param networkManager network manager.
     * @param runner runner instance.
     * @param resourceMonitor resource monitor.
     * @param ociManager OCI manager.
     * @param hostWhiteoutsDir host whiteouts directory.
     */
    Instance(const Config& config, const InstanceInfo& instanceInfo, const String& instanceID,
        servicemanager::ServiceManagerItf& serviceManager, layermanager::LayerManagerItf& layerManager,
        networkmanager::NetworkManagerItf& networkManager, runner::RunnerItf& runner, RuntimeItf& runtime,
        monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager, const String& hostWhiteoutsDir);

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
     * @param service service.
     */
    void SetService(const Service* service);

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
            return mService->Data().mVersion;
        }

        return "";
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
        = (sizeof(oci::RuntimeSpec)
              + Max(sizeof(networkmanager::NetworkParams), sizeof(monitoring::InstanceMonitorParams),
                  sizeof(LayersStaticArray) + sizeof(image::ImageParts)))
        * AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES;
    static constexpr auto cNumAllocations  = 4 * AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES;
    static constexpr auto cRuntimeSpecFile = "config.json";
    static constexpr auto cMountPointsDir  = "mounts";
    static constexpr auto cRootFSDir       = "rootfs";

    Error SetupNetwork();
    Error CreateRuntimeSpec(oci::RuntimeSpec& runtimeSpec);
    Error SetupMonitoring();
    Error PrepareRootFS(oci::RuntimeSpec& runtimeSpec);

    static StaticAllocator<cAllocatorSize, cNumAllocations> sAllocator;

    const Config&                      mConfig;
    StaticString<cInstanceIDLen>       mInstanceID;
    InstanceInfo                       mInstanceInfo;
    servicemanager::ServiceManagerItf& mServiceManager;
    layermanager::LayerManagerItf&     mLayerManager;
    networkmanager::NetworkManagerItf& mNetworkManager;
    runner::RunnerItf&                 mRunner;
    RuntimeItf&                        mRuntime;
    monitoring::ResourceMonitorItf&    mResourceMonitor;
    oci::OCISpecItf&                   mOCIManager;
    const String&                      mHostWhiteoutsDir;

    StaticString<cFilePathLen> mRuntimeDir;
    const Service*             mService = nullptr;
    InstanceRunState           mRunState;
    Error                      mRunError;
};

} // namespace aos::sm::launcher

#endif
