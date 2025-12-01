/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_COMMUNICATION_ITF_COMMUNICATION_HPP_
#define AOS_CORE_CM_COMMUNICATION_ITF_COMMUNICATION_HPP_

#include <core/cm/alerts/itf/sender.hpp>
#include <core/cm/launcher/itf/sender.hpp>
#include <core/cm/monitoring/itf/sender.hpp>
#include <core/cm/smcontroller/itf/sender.hpp>
#include <core/cm/storagestate/itf/sender.hpp>
#include <core/cm/updatemanager/itf/sender.hpp>
#include <core/common/blobinfoprovider/itf/blobinfoprovider.hpp>
#include <core/common/cloudconnection/itf/cloudconnection.hpp>

namespace aos::cm::communication {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Communication interface.
 */
class CommunicationItf : public alerts::SenderItf,
                         public launcher::SenderItf,
                         public monitoring::SenderItf,
                         public smcontroller::SenderItf,
                         public storagestate::SenderItf,
                         public updatemanager::SenderItf,
                         public blobinfoprovider::ProviderItf,
                         public cloudconnection::CloudConnectionItf {
public:
    /**
     * Destructor.
     */
    virtual ~CommunicationItf() override = default;
};

/** @}*/

} // namespace aos::cm::communication

#endif
