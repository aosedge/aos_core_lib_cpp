/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_SMCLIENT_ITF_CONNECTION_HPP_
#define AOS_CORE_SM_SMCLIENT_ITF_CONNECTION_HPP_

#include <core/common/tools/error.hpp>

namespace aos::sm::smclient {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Listener interface for connection events.
 * Components implement this to synchronize state on (re)connect.
 */
class ConnectListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ConnectListenerItf() = default;

    /**
     * Called when connection is established or re-established.
     */
    virtual void OnConnect() = 0;
};

/**
 * Connection interface for subscribing to connection events.
 */
class ConnectionItf {
public:
    /**
     * Destructor.
     */
    virtual ~ConnectionItf() = default;

    /**
     * Subscribes to connection events.
     *
     * @param listener listener reference.
     * @return Error.
     */
    virtual Error SubscribeListener(ConnectListenerItf& listener) = 0;

    /**
     * Unsubscribes from connection events.
     *
     * @param listener listener reference.
     * @return Error.
     */
    virtual Error UnsubscribeListener(ConnectListenerItf& listener) = 0;
};

/** @}*/

} // namespace aos::sm::smclient

#endif
