/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_SMCLIENT_ITF_SMCLIENT_HPP_
#define AOS_CORE_SM_SMCLIENT_ITF_SMCLIENT_HPP_

#include <core/common/alerts/itf/sender.hpp>
#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/logging/itf/sender.hpp>
#include <core/common/monitoring/itf/sender.hpp>
#include <core/sm/imagemanager/itf/blobinfoprovider.hpp>
#include <core/sm/launcher/itf/sender.hpp>

namespace aos::sm::smclient {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * SM client.
 */
class SMClientItf : public aos::alerts::SenderItf,
                    public aos::monitoring::SenderItf,
                    public aos::logging::SenderItf,
                    public launcher::SenderItf,
                    public imagemanager::BlobInfoProviderItf,
                    public cloudconnection::CloudConnectionItf {
public:
    /**
     * Destructor.
     */
    virtual ~SMClientItf() = default;
};

/** @}*/

} // namespace aos::sm::smclient

#endif
