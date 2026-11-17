/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_STATUSNOTIFIER_HPP_
#define AOS_CORE_SM_LAUNCHER_STATUSNOTIFIER_HPP_

#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>

#include "instanceregistry.hpp"
#include "itf/sender.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Sends the instances statuses to the CM and notifies the status listeners.
 */
class StatusNotifier {
public:
    /**
     * Initializes status notifier.
     *
     * @param allocator allocator to use for temporary objects.
     * @param sender sender.
     */
    void Init(AllocatorItf& allocator, SenderItf& sender);

    /**
     * Subscribes status listener.
     *
     * @param listener status listener.
     * @return Error eAlreadyExist if the listener is already subscribed.
     */
    Error Subscribe(instancestatusprovider::ListenerItf& listener);

    /**
     * Unsubscribes status listener.
     *
     * @param listener status listener.
     * @return Error eNotFound if the listener isn't subscribed.
     */
    Error Unsubscribe(instancestatusprovider::ListenerItf& listener);

    /**
     * Sends the statuses of all node instances to the CM. The failures are logged.
     *
     * @param instances node instances.
     */
    void SendNodeStatuses(const InstanceRegistry& instances);

    /**
     * Sends the changed instances statuses to the CM.
     *
     * @param statuses changed instances statuses.
     * @return Error.
     */
    Error SendUpdateStatuses(const Array<InstanceStatus>& statuses);

    /**
     * Notifies the status listeners about the changed instances statuses.
     *
     * @param statuses changed instances statuses.
     */
    void NotifyListeners(const Array<InstanceStatus>& statuses);

private:
    static constexpr auto cMaxNumListeners = 4;

    AllocatorItf*                                                       mAllocator {};
    SenderItf*                                                          mSender {};
    Mutex                                                               mMutex;
    StaticArray<instancestatusprovider::ListenerItf*, cMaxNumListeners> mListeners;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
