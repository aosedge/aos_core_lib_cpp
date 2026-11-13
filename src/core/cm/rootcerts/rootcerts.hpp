/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_ROOTCERTS_ROOTCERTS_HPP_
#define AOS_CORE_CM_ROOTCERTS_ROOTCERTS_HPP_

#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/iamclient/itf/certhandler.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>

#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>

#include "itf/rootcerts.hpp"
#include "itf/sender.hpp"

namespace aos::cm::rootcerts {

/**
 * Root certificates handler.
 */
class RootCerts : public RootCertsItf,
                  private nodeinfoprovider::NodeInfoListenerItf,
                  private cloudconnection::ConnectionListenerItf {
public:
    /**
     * Initializes root certificates handler.
     *
     * @param allocator allocator.
     * @param certHandler certificate handler.
     * @param nodeInfoProvider node info provider.
     * @param cloudConnection cloud connection.
     * @param sender unit root certificates sender.
     * @return Error.
     */
    Error Init(AllocatorItf& allocator, iamclient::CertHandlerItf& certHandler,
        nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider, cloudconnection::CloudConnectionItf& cloudConnection,
        SenderItf& sender);

    /**
     * Starts root certificates handler.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops root certificates handler.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Processes desired unit root certificates.
     *
     * @param desiredRootCerts desired unit root certificates.
     * @return Error.
     */
    Error ProcessDesiredRootCertificates(const DesiredUnitRootCertificates& desiredRootCerts) override;

private:
    void OnNodeInfoChanged(const UnitNodeInfo& info) override;
    void OnConnect() override;
    void OnDisconnect() override;

    Error FillNodeRootCertificates(const String& nodeID, NodeRootCertificates& nodeCerts);
    Error SendFullSnapshot();
    Error SendRootCertUpdate(const String& nodeID);

    AllocatorItf*                          mAllocator {};
    iamclient::CertHandlerItf*             mCertHandler {};
    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    cloudconnection::CloudConnectionItf*   mCloudConnection {};
    SenderItf*                             mSender {};

    Mutex mMutex;
    bool  mCloudConnected {};
};

} // namespace aos::cm::rootcerts

#endif
