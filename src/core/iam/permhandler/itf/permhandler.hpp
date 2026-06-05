/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_PERMHANDLER_ITF_PERMHANDLER_HPP_
#define AOS_CORE_IAM_PERMHANDLER_ITF_PERMHANDLER_HPP_

#include <core/common/iamclient/itf/permhandler.hpp>
#include <core/common/iamclient/itf/permprovider.hpp>

namespace aos::iam::permhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Permission handler interface.
 */
class PermHandlerItf : public iamclient::PermHandlerItf, public iamclient::PermProviderItf { };

/** @}*/

} // namespace aos::iam::permhandler

#endif
