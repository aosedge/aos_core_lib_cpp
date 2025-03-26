/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CONNECTIONSUBSC_HPP_
#define AOS_CONNECTIONSUBSC_HPP_

#include "aos/common/tools/error.hpp"

namespace aos {
/**
 * Interface for objects that need to respond to connection events.
 */
class ConnectionSubscriberItf {
public:
    /**
     * Destructor.
     */
    virtual ~ConnectionSubscriberItf() = default;

    /**
     * Notifies publisher is connected.
     */
    virtual void OnConnect() = 0;

    /**
     * Notifies publisher is disconnected.
     */
    virtual void OnDisconnect() = 0;
};

/**
 * Interface for objects that support subscription from ConnectionSubscriberItf objects.
 */
class ConnectionPublisherItf {
public:
    virtual ~ConnectionPublisherItf() { }

    /**
     * Subscribes to cloud connection events.
     *
     * @param subscriber subscriber reference.
     */
    virtual aos::Error Subscribe(ConnectionSubscriberItf& subscriber) = 0;

    /**
     * Unsubscribes from cloud connection events.
     *
     * @param subscriber subscriber reference.
     */
    virtual void Unsubscribe(ConnectionSubscriberItf& subscriber) = 0;
};

} // namespace aos

#endif
