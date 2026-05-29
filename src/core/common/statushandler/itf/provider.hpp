/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_STATUSHANDLER_ITF_PROVIDER_HPP_
#define AOS_CORE_COMMON_STATUSHANDLER_ITF_PROVIDER_HPP_

#include <core/common/statushandler/itf/listener.hpp>

namespace aos::statushandler {

/**
 * Interface for providing instance statuses.
 */
class ProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ProviderItf() = default;

    /**
     * Returns statuses of all instances.
     *
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    virtual Error GetAllInstancesStatuses(Array<InstanceStatus>& statuses) = 0;

    /**
     * Returns statuses of node instances.
     *
     * @param nodeID node identifier.
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    virtual Error GetNodeInstancesStatuses(const String& nodeID, Array<InstanceStatus>& statuses) = 0;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error SubscribeListener(ListenerItf& listener) = 0;

    /**
     * Unsubscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(ListenerItf& listener) = 0;
};

} // namespace aos::statushandler

#endif
