
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
#include "aos/sm/networkmanager.hpp"
#include "aos/sm/runner.hpp"

namespace aos::sm::launcher {

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
     * @param networkManager network manager.
     * @param runner runner instance.
     * @param resourceMonitor resource monitor.
     * @param ociManager OCI manager.
     */
    Instance(const Config& config, const InstanceInfo& instanceInfo, const String& instanceID,
        networkmanager::NetworkManagerItf& networkManager, runner::RunnerItf& runner,
        monitoring::ResourceMonitorItf& resourceMonitor, oci::OCISpecItf& ociManager);

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
    static constexpr auto cRuntimeDir    = AOS_CONFIG_LAUNCHER_RUNTIME_DIR;
    static constexpr auto cAllocatorSize = Max(sizeof(oci::RuntimeSpec), sizeof(networkmanager::NetworkParams),
                                               sizeof(monitoring::InstanceMonitorParams))
        * AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES;
    static constexpr auto cRuntimeSpecFile = "config.json";
    static constexpr auto cMountPointsDir  = "mounts";

    Error SetupNetwork();
    Error CreateRuntimeSpec();
    Error SetupMonitoring();

    static StaticAllocator<cAllocatorSize> sAllocator;

    const Config&                      mConfig;
    StaticString<cInstanceIDLen>       mInstanceID;
    InstanceInfo                       mInstanceInfo;
    networkmanager::NetworkManagerItf& mNetworkManager;
    runner::RunnerItf&                 mRunner;
    monitoring::ResourceMonitorItf&    mResourceMonitor;
    oci::OCISpecItf&                   mOCIManager;

    StaticString<cFilePathLen> mRuntimeDir;
    const Service*             mService = nullptr;
    InstanceRunState           mRunState;
    Error                      mRunError;
};

} // namespace aos::sm::launcher

#endif
