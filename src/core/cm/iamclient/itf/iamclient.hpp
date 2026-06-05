/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IAMCLIENT_ITF_IAMCLIENT_HPP_
#define AOS_CORE_CM_IAMCLIENT_ITF_IAMCLIENT_HPP_

#include <core/common/iamclient/itf/certhandler.hpp>
#include <core/common/iamclient/itf/certprovider.hpp>
#include <core/common/iamclient/itf/nodehandler.hpp>
#include <core/common/iamclient/itf/nodeinfoprovider.hpp>
#include <core/common/iamclient/itf/provisioning.hpp>

namespace aos::cm::iamclient {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * IAM client interface.
 */
class IAMClientItf : public aos::iamclient::CertHandlerItf,
                     public aos::iamclient::CertProviderItf,
                     public aos::iamclient::NodeHandlerItf,
                     public aos::iamclient::NodeInfoProviderItf,
                     public aos::iamclient::ProvisioningItf {
public:
    /**
     * Destructor.
     */
    virtual ~IAMClientItf() = default;
};

/** @}*/

} // namespace aos::cm::iamclient

#endif
