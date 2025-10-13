/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGESTATUSPROVIDER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGESTATUSPROVIDER_HPP_

#include <core/common/types/common.hpp>

namespace aos::cm::imagemanager {

/**
 * Interface for receiving notification about update image statuses.
 */
class ImageStatusListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageStatusListenerItf() = default;

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
 * Interface to provide update image statuses.
 */
class ImageStatusProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageStatusProviderItf() = default;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error SubscribeListener(ImageStatusListenerItf& listener) = 0;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(ImageStatusListenerItf& listener) = 0;
};

} // namespace aos::cm::imagemanager

#endif
