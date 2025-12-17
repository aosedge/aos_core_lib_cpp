/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_ALERTS_ALERTS_HPP_
#define AOS_CORE_COMMON_ALERTS_ALERTS_HPP_

#include <core/common/alerts/itf/sender.hpp>
#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>
#include <core/common/types/alerts.hpp>

#include <core/cm/config.hpp>

#include "config.hpp"
#include "itf/provider.hpp"
#include "itf/receiver.hpp"
#include "itf/sender.hpp"

namespace aos::cm::alerts {

/**
 * Alerts cache size.
 */
constexpr auto cAlertsCacheSize = AOS_CONFIG_CM_ALERTS_CACHE_SIZE;

/**
 * Alerts.
 */
class Alerts : public ReceiverItf,
               public AlertsProviderItf,
               public aos::alerts::SenderItf,
               private cloudconnection::ConnectionListenerItf {
public:
    /**
     * Initializes alerts.
     *
     * @param config configuration object.
     * @param sender alerts sender object.
     * @return Error.
     */
    Error Init(const alerts::Config& config, cm::alerts::SenderItf& sender);

    /**
     * Starts alerts module.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops alerts module.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Receives alert.
     *
     * @param alert alert.
     * @return Error.
     */
    Error OnAlertReceived(const AlertVariant& alert) override;

    /**
     * Notifies publisher is connected.
     */
    void OnConnect() override;

    /**
     * Notifies publisher is disconnected.
     */
    void OnDisconnect() override;

    /**
     * Sends alert data.
     *
     * @param alert alert variant.
     * @return Error.
     */
    Error SendAlert(const AlertVariant& alert) override;

    /**
     * Subscribes alerts listener to specified tags.
     *
     * @param tags alert tags to subscribe to.
     * @param listener alerts listener to subscribe.
     * @return Error.
     */
    Error SubscribeListener(const Array<AlertTag>& tags, AlertsListenerItf& listener) override;

    /**
     * Unsubscribes alerts listener.
     *
     * @param listener alerts listener to unsubscribe.
     * @return Error.
     */
    Error UnsubscribeListener(AlertsListenerItf& listener) override;

private:
    static constexpr auto cAllocatorSize     = sizeof(AlertVariant) + sizeof(aos::Alerts);
    static constexpr auto cListenersMaxCount = 4;
    static constexpr auto cAlertTagsCount    = static_cast<size_t>(AlertTagEnum::eNumAlertTags);

    using ListenersArray = StaticArray<AlertsListenerItf*, cListenersMaxCount>;

    Error                  HandleAlert(const AlertVariant& alert);
    Error                  SendAlerts();
    bool                   IsDuplicated(const AlertVariant& alert);
    UniquePtr<aos::Alerts> CreatePackage();
    void                   ShrinkCache(size_t count);
    void                   NotifyListeners(const AlertVariant& alert);

    StaticAllocator<cAllocatorSize>                      mAllocator;
    alerts::Config                                       mConfig;
    cm::alerts::SenderItf*                               mSender {};
    StaticArray<AlertVariant, cAlertsCacheSize>          mAlerts;
    StaticMap<AlertTag, ListenersArray, cAlertTagsCount> mListeners;
    Mutex                                                mMutex;
    Timer                                                mSendTimer;
    bool                                                 mIsRunning {};
    bool                                                 mIsConnected {};
    size_t                                               mSkippedAlerts {};
    size_t                                               mDuplicatedAlerts {};
};

} // namespace aos::cm::alerts

#endif
