/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_ITF_ITEMSTATUSPROVIDER_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_ITF_ITEMSTATUSPROVIDER_HPP_

#include <core/common/types/unitstatus.hpp>

namespace aos::common::imagemanager {

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
     * Notifies about item status change.
     *
     * @param status update item status.
     */
    virtual void OnItemStatusChanged(const UpdateItemStatus& status) = 0;

    /**
     * Notifies about item removal from cache.
     *
     * @param id removed item id.
     * @param version removed item version.
     */
    virtual void OnItemRemovedFromCache(const String& id, const String& version) = 0;
};

/**
 * Interface to provide deployable item statuses.
 */
class ItemStatusProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ItemStatusProviderItf() = default;

    /**
     * Retrieves deployable items statuses.
     *
     * @param[out] statuses list of deployable items statuses.
     * @return Error.
     */
    virtual Error GetItemsStatuses(Array<UpdateItemStatus>& statuses) = 0;

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

} // namespace aos::common::imagemanager

#endif
