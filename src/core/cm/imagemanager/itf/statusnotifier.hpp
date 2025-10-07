/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_STATUSLISTENER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_STATUSLISTENER_HPP_

#include <core/common/types/common.hpp>

namespace aos::cm::imagemanager {

/**
 * Interface for receiving notification about update items statuses.
 */
class StatusListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~StatusListenerItf() = default;

    /**
     * Notifies about image status change.
     *
     * @param id updated image id.
     * @param status updated image status.
     */
    virtual void OnImageStatusChanged(const String& id, const ImageStatus& status) = 0;

    /**
     * Notifies about update item removal.
     *
     * @param id removed update item id.
     */
    virtual void OnUpdateItemRemoved(const String& id) = 0;
};

/**
 * Interface for notifying about update item statuses.
 */
class StatusNotifierItf {
public:
    /**
     * Destructor.
     */
    virtual ~StatusNotifierItf() = default;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error SubscribeListener(StatusListenerItf& listener) = 0;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(StatusListenerItf& listener) = 0;
};

} // namespace aos::cm::imagemanager

#endif
