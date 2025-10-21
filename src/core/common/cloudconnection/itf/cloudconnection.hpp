/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDCONNECTION_HPP_
#define AOS_CORE_COMMON_CLOUDCONNECTION_HPP_

#include <core/common/tools/error.hpp>

namespace aos {

/**
 * Interface for objects that need to respond to connection events.
 */
class ConnectionListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ConnectionListenerItf() = default;

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
 * Cloud connection interface.
 */
class CloudConnectionItf {
public:
    virtual ~CloudConnectionItf() { }

    /**
     * Subscribes to cloud connection events.
     *
     * @param listener listener reference.
     */
    virtual Error Subscribe(ConnectionListenerItf& listener) = 0;

    /**
     * Unsubscribes from cloud connection events.
     *
     * @param listener listener reference.
     */
    virtual void Unsubscribe(ConnectionListenerItf& listener) = 0;
};

} // namespace aos

#endif
