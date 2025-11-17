/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_ITEMSTATUSPROVIDER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_ITEMSTATUSPROVIDER_HPP_

#include <core/common/types/unitstatus.hpp>

namespace aos::cm::imagemanager {

/**
 * Interface for receiving notification about update image statuses.
 */
class ItemStatusListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ItemStatusListenerItf() = default;

    /**
     * Notifies about items statuses change.
     *
     * @param statuses update items statuses.
     */
    virtual void OnItemsStatusesChanged(const Array<UpdateItemStatus>& statuses) = 0;

    /**
     * Notifies about item removal.
     *
     * @param id removed item id.
     */
    virtual void OnItemRemoved(const String& id) = 0;
};

/**
 * Interface to provide update item statuses.
 */
class ItemStatusProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ItemStatusProviderItf() = default;

    /**
     * Retrieves update items statuses.
     *
     * @param[out] statuses list of update items statuses.
     * @return Error.
     */
    virtual Error GetUpdateItemsStatuses(Array<UpdateItemStatus>& statuses) = 0;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error SubscribeListener(ItemStatusListenerItf& listener) = 0;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(ItemStatusListenerItf& listener) = 0;
};

} // namespace aos::cm::imagemanager

#endif
