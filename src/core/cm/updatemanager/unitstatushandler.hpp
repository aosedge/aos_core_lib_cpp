/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_UNITSTATUSHANDLER_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_UNITSTATUSHANDLER_HPP_

#include <core/cm/imagemanager/itf/itemstatusprovider.hpp>
#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>
#include <core/cm/unitconfig/itf/unitconfig.hpp>
#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/iamclient/itf/identprovider.hpp>
#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/timer.hpp>

#include "config.hpp"
#include "itf/sender.hpp"

namespace aos::cm::updatemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Unit status handler.
 */
class UnitStatusHandler : private nodeinfoprovider::NodeInfoListenerItf,
                          private imagemanager::ItemStatusListenerItf,
                          private instancestatusprovider::ListenerItf,
                          private iamclient::SubjectsListenerItf,
                          private cloudconnection::ConnectionListenerItf {
public:
    /**
     * Initializes unit status handler.
     *
     * @param config update manager configuration.
     * @param identProvider identity provider.
     * @param unitConfig unit config interface.
     * @param nodeInfoProvider node info provider.
     * @param itemStatusProvider item status provider.
     * @param instanceStatusProvider instance status provider.
     * @param cloudConnection cloud connection.
     * @param sender unit status sender.
     * @return Error.
     */
    Error Init(const Config& config, iamclient::IdentProviderItf& identProvider, unitconfig::UnitConfigItf& unitConfig,
        nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        imagemanager::ItemStatusProviderItf&   itemStatusProvider,
        instancestatusprovider::ProviderItf&   instanceStatusProvider,
        cloudconnection::CloudConnectionItf& cloudConnection, SenderItf& sender);

    /**
     * Starts unit status handler.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops unit status handler.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Sends full unit status.
     *
     * @return Error
     */
    Error SendFullUnitStatus();

    /**
     * Sets update unit config status.
     *
     * @param status unit config status.
     */
    Error SetUpdateUnitConfigStatus(const UnitConfigStatus& status);

    /**
     * Sets update node status.
     *
     * @param nodeID node ID.
     * @param updateErr update error.
     */
    Error SetUpdateNodeStatus(const String& nodeID, const Error& updateErr);

private:
    static constexpr auto cAllocatorSize = sizeof(StaticArray<InstanceStatus, cMaxNumInstances>);

    // nodeinfoprovider::NodeInfoListenerItf implementation
    void OnNodeInfoChanged(const UnitNodeInfo& info) override;

    // imagemanager::ItemStatusListenerItf implementation
    void OnItemsStatusesChanged(const Array<UpdateItemStatus>& statuses) override;
    void OnItemRemoved(const String& itemID) override;

    // instancestatusprovider::ListenerItf implementation
    void OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses) override;

    // iamclient::SubjectsListenerItf implementation
    void SubjectsChanged(const Array<StaticString<cIDLen>>& subjects) override;

    // cloudconnection::ConnectionListenerItf implementation
    void OnConnect() override;
    void OnDisconnect() override;

    void ClearUnitStatus();
    void ClearUpdateStatuses();
    void StartTimer();

    Error SetUnitConfigStatus();
    Error SetNodesInfo();
    Error SetUpdateItemsStatus();
    Error SetInstancesStatus();
    Error SetUnitSubjects();
    void  LogUnitStatus();

    iamclient::IdentProviderItf*           mIdentProvider {};
    unitconfig::UnitConfigItf*             mUnitConfig {};
    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    imagemanager::ItemStatusProviderItf*   mItemStatusProvider {};
    instancestatusprovider::ProviderItf*   mInstanceStatusProvider {};
    cloudconnection::CloudConnectionItf*   mCloudConnection {};
    SenderItf*                             mSender {};

    Mutex                           mMutex;
    UnitStatus                      mUnitStatus;
    StaticAllocator<cAllocatorSize> mAllocator;
    bool                            mCloudConnected {};

    Timer    mTimer;
    bool     mTimerStarted {};
    Duration mUnitStatusSendTimeout {};

    Optional<UnitConfigStatus>             mUpdateUnitConfigStatus;
    StaticMap<String, Error, cMaxNumNodes> mUpdateNodeStatuses;
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
