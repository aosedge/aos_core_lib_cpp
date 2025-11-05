/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_UPDATEMANAGER_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_UPDATEMANAGER_HPP_

#include <core/cm/imagemanager/itf/imagemanager.hpp>
#include <core/common/cloudconnection/itf/cloudconnection.hpp>

#include "itf/updatemanager.hpp"
#include "unitstatushandler.hpp"

namespace aos::cm::updatemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Update manager.
 */
class UpdateManager : public UpdateManagerItf, private cloudconnection::ConnectionListenerItf {
public:
    /**
     * Initializes update manager.
     *
     * @param config update manager configuration.
     * @param identProvider identity provider.
     * @param unitConfig unit config interface.
     * @param nodeInfoProvider node info provider.
     * @param imageManager image manager.
     * @param instanceStatusProvider instance status provider.
     * @param cloudConnection cloud connection.
     * @param sender unit status sender.
     * @return Error.
     */
    Error Init(const Config& config, iamclient::IdentProviderItf& identProvider, unitconfig::UnitConfigItf& unitConfig,
        nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider, imagemanager::ImageManagerItf& imageManager,
        launcher::InstanceStatusProviderItf& instanceStatusProvider,
        cloudconnection::CloudConnectionItf& cloudConnection, SenderItf& sender);

    /**
     * Starts update manager.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops update manager.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Processes desired status.
     *
     * @param desiredStatus desired status.
     * @return Error.
     */
    Error ProcessDesiredStatus(const DesiredStatus& desiredStatus) override;

private:
    // cloudconnection::ConnectionListenerItf implementation
    void OnConnect() override;
    void OnDisconnect() override;

    void Run();

    cloudconnection::CloudConnectionItf* mCloudConnection {};

    UnitStatusHandler mUnitStatusHandler;

    Mutex               mMutex;
    ConditionalVariable mCondVar;
    Thread<>            mThread;

    bool mIsRunning {};
    bool mSendUnitStatus {};
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
