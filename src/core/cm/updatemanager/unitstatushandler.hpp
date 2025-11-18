/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_UNITSTATUSHANDLER_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_UNITSTATUSHANDLER_HPP_

#include <core/cm/imagemanager/itf/itemstatusprovider.hpp>
#include <core/cm/launcher/itf/instancestatusprovider.hpp>
#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>
#include <core/cm/unitconfig/itf/unitconfig.hpp>
#include <core/common/iamclient/itf/identprovider.hpp>
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
                          private launcher::InstanceStatusListenerItf,
                          private iamclient::SubjectsListenerItf {
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
     * @param sender unit status sender.
     * @return Error.
     */
    Error Init(const Config& config, iamclient::IdentProviderItf& identProvider, unitconfig::UnitConfigItf& unitConfig,
        nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        imagemanager::ItemStatusProviderItf&   itemStatusProvider,
        launcher::InstanceStatusProviderItf& instanceStatusProvider, SenderItf& sender);

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
     * Sets cloud connection status.
     *
     * @param isConnected true if cloud connected.
     */
    void SetCloudConnected(bool isConnected);

private:
    static constexpr auto cAllocatorSize = sizeof(StaticArray<InstanceStatus, cMaxNumInstances>);

    // nodeinfoprovider::NodeInfoListenerItf implementation
    void OnNodeInfoChanged(const UnitNodeInfo& info) override;

    // imagemanager::ItemStatusListenerItf implementation
    void OnItemStatusChanged(const UpdateItemStatus& status) override;
    void OnItemRemoved(const String& itemID) override;

    // launcher::InstanceStatusListenerItf implementation
    void OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses) override;

    // iamclient::SubjectsListenerItf implementation
    void SubjectsChanged(const Array<StaticString<cIDLen>>& subjects) override;

    void ClearUnitStatus();
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
    launcher::InstanceStatusProviderItf*   mInstanceStatusProvider {};
    SenderItf*                             mSender {};

    Mutex                           mMutex;
    UnitStatus                      mUnitStatus;
    StaticAllocator<cAllocatorSize> mAllocator;
    bool                            mCloudConnected {};

    Timer    mTimer;
    bool     mTimerStarted {};
    Duration mUnitStatusSendTimeout {};
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
