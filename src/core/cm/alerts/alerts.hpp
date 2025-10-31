/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_ALERTS_ALERTS_HPP_
#define AOS_CORE_COMMON_ALERTS_ALERTS_HPP_

#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>
#include <core/common/types/alerts.hpp>

#include <core/cm/communication/itf/communication.hpp>
#include <core/cm/config.hpp>

#include "config.hpp"
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
class Alerts : public smcontroller::ReceiverItf, private ConnectionListenerItf {
public:
    /**
     * Initializes alerts.
     *
     * @param config configuration object.
     * @param sender alerts sender object.
     * @return Error.
     */
    Error Init(const alerts::Config& config, updatemanager::SenderItf& sender);

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

private:
    static constexpr auto cAllocatorSize = sizeof(AlertVariant) + sizeof(aos::Alerts);

    Error                  SendAlerts();
    bool                   IsDuplicated(const AlertVariant& alert);
    UniquePtr<aos::Alerts> CreatePackage();
    void                   ShrinkCache(size_t count);

    StaticAllocator<cAllocatorSize>             mAllocator;
    alerts::Config                              mConfig;
    updatemanager::SenderItf*                   mSender {};
    StaticArray<AlertVariant, cAlertsCacheSize> mAlerts;
    Mutex                                       mMutex;
    Timer                                       mSendTimer;
    bool                                        mIsRunning {};
    bool                                        mIsConnected {};
    size_t                                      mSkippedAlerts {};
    size_t                                      mDuplicatedAlerts {};
};

} // namespace aos::cm::alerts

#endif
