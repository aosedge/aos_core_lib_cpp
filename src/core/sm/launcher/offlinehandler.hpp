/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_OFFLINEHANDLER_HPP_
#define AOS_CORE_SM_LAUNCHER_OFFLINEHANDLER_HPP_

#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/time.hpp>
#include <core/common/tools/timer.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Receives the requests to check the offline TTLs of the instances.
 */
class OfflineCheckListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~OfflineCheckListenerItf() = default;

    /**
     * Requests to stop the instances whose offline TTL expired and to arm the offline handler again.
     */
    virtual void OnOfflineCheck() = 0;
};

/**
 * Tracks the time the node is offline: the cloud disconnect and each armed deadline request an offline check.
 */
class OfflineHandler : private cloudconnection::ConnectionListenerItf {
public:
    /**
     * Starts offline handler: subscribes to the cloud connection and requests the check if the node is offline.
     *
     * @param cloudConnection cloud connection.
     * @param listener offline check listener.
     * @return Error.
     */
    Error Start(cloudconnection::CloudConnectionItf& cloudConnection, OfflineCheckListenerItf& listener);

    /**
     * Stops offline handler and waits for the check request in progress.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Returns whether the instance with the offline TTL must be stopped.
     *
     * @param offlineTTL offline TTL of the instance, zero if it has none.
     * @return bool.
     */
    bool IsExpired(Duration offlineTTL) const;

    /**
     * Requests the next check when the earliest offline TTL expires. Does nothing while the node is online.
     *
     * @param minOfflineTTL minimal offline TTL of the running instances.
     */
    void Arm(const Optional<Duration>& minOfflineTTL);

private:
    void OnConnect() override;
    void OnDisconnect() override;

    cloudconnection::CloudConnectionItf* mCloudConnection {};
    OfflineCheckListenerItf*             mListener {};
    mutable Mutex                        mMutex;
    Optional<Time>                       mOfflineTime;
    Timer                                mTimer;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
