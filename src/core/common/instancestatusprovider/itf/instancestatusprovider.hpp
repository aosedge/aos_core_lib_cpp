/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_INSTANCESTATUSPROVIDER_ITF_INSTANCESTATUSPROVIDER_HPP_
#define AOS_CORE_COMMON_INSTANCESTATUSPROVIDER_ITF_INSTANCESTATUSPROVIDER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::instancestatusprovider {

/**
 * Interface for receiving notification about instance statuses.
 */
class ListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ListenerItf() = default;

    /**
     * Notifies about instances statuses change.
     *
     * @param statuses instances statuses.
     */
    virtual void OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses) = 0;
};

/**
 * Interface for notifying about instance statuses.
 */
class ProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ProviderItf() = default;

    /**
     * Returns current statuses of running instances.
     *
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    virtual Error GetInstancesStatuses(Array<InstanceStatus>& statuses) = 0;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error SubscribeListener(ListenerItf& listener) = 0;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(ListenerItf& listener) = 0;
};

} // namespace aos::instancestatusprovider

#endif
