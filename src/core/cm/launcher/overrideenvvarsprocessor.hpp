/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_OVERRIDEENVVARSPROCESSOR_HPP_
#define AOS_CORE_CM_LAUNCHER_OVERRIDEENVVARSPROCESSOR_HPP_

#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>
#include <core/common/types/envvars.hpp>
#include <core/common/types/instance.hpp>

#include "itf/sender.hpp"
#include "itf/storage.hpp"

#include "config.hpp"

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Override environment variables listener interface.
 */
class OverrideEnvVarsListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~OverrideEnvVarsListenerItf() = default;

    /**
     * Notifies that the override env vars has changed (new request or TTL expiry).
     */
    virtual void OnOverrideEnvVarsChanged() = 0;
};

/**
 * RAII accessor that keeps the processor mutex locked while the override env vars are accessed.
 */
class OverrideEnvVarsAccessor {
public:
    /**
     * Locks the mutex and binds the override env vars for the accessor lifetime.
     *
     * @param mutex processor mutex.
     * @param envVars override environment variables.
     */
    OverrideEnvVarsAccessor(Mutex& mutex, const OverrideEnvVarsRequest& envVars)
        : mLock(mutex)
        , mEnvVars(envVars)
    {
    }

    /**
     * Returns the locked override environment variables.
     *
     * @return const OverrideEnvVarsRequest&.
     */
    const OverrideEnvVarsRequest& operator*() const { return mEnvVars; }

private:
    LockGuard<Mutex>              mLock;
    const OverrideEnvVarsRequest& mEnvVars;
};

/**
 * Environment variables instance status with node ID.
 */
struct EnvVarsInstanceStatusWithNode {
    StaticString<cIDLen>  mNodeID;
    EnvVarsInstanceStatus mStatus;
};

/**
 * Processes override environment variables requests.
 */
class OverrideEnvVarsProcessor {
public:
    /**
     * Initializes override env vars processor.
     *
     * @param config launcher configuration.
     * @param storage storage interface.
     * @param envVarStatusSender override env vars status sender interface.
     * @param listener override env vars change listener.
     * @return Error.
     */
    Error Init(
        const Config& config, StorageItf& storage, SenderItf& envVarStatusSender, OverrideEnvVarsListenerItf& listener);

    /**
     * Starts override env vars processor.
     *
     * The change is reported via the return value instead of the listener: on start the caller holds the launcher
     * mutex (which the listener also takes, so notifying would deadlock) and the worker thread is not running yet.
     *
     * @return RetWithError<bool> true if the restored override set changed due to expired variables.
     */
    RetWithError<bool> Start();

    /**
     * Stops override env vars processor.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Overrides environment variables.
     *
     * @param envVars requested override environment variables.
     * @return RetWithError<bool> true if the override env vars changed.
     */
    RetWithError<bool> OverrideEnvVars(const OverrideEnvVarsRequest& envVars);

    /**
     * Returns an RAII accessor that keeps the mutex locked while the current override env vars are read.
     *
     * @return OverrideEnvVarsAccessor.
     */
    OverrideEnvVarsAccessor GetOverrideEnvVars() { return OverrideEnvVarsAccessor(mMutex, mOverrideEnvVars); }

    /**
     * Stores instance env var statuses for the given node, replacing any previously stored statuses for that node.
     *
     * @param nodeID node ID.
     * @param statuses instance statuses.
     * @return Error.
     */
    Error AddStatuses(const String& nodeID, const Array<InstanceStatus>& statuses);

    /**
     * Sends a full snapshot of stored env var statuses if they changed since the last send.
     */
    void SendStatuses();

private:
    static void RemoveExpiredVariables(OverrideEnvVarsRequest& envVars, const Time& now);
    static bool HasExpiredVariables(const OverrideEnvVarsRequest& envVars, const Time& now);

    void               OnTTLTimerTick();
    RetWithError<bool> ProcessOverrideEnvVars(const OverrideEnvVarsRequest& envVars);

    Duration                    mCheckPeriod {};
    StorageItf*                 mStorage {};
    SenderItf*                  mEnvVarStatusSender {};
    OverrideEnvVarsListenerItf* mListener {};

    Mutex mMutex;
    Timer mTimer;

    OverrideEnvVarsRequest                                       mOverrideEnvVars;
    StaticArray<EnvVarsInstanceStatusWithNode, cMaxNumInstances> mNodeEnvVarStatuses;
    OverrideEnvVarsStatuses                                      mCurrentEnvVarStatuses;
    OverrideEnvVarsStatuses                                      mNewEnvVarStatuses;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
