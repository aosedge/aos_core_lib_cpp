/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_SMCONTROLLER_ITF_SMCONTROLLER_HPP_
#define AOS_CORE_CM_SMCONTROLLER_ITF_SMCONTROLLER_HPP_

#include <core/cm/launcher/itf/instancerunner.hpp>
#include <core/cm/launcher/itf/monitoringprovider.hpp>
#include <core/cm/launcher/itf/nodeenvvarhandler.hpp>
#include <core/cm/networkmanager/itf/nodenetwork.hpp>
#include <core/cm/unitconfig/itf/nodeconfighandler.hpp>

#include "logprovider.hpp"

namespace aos::cm::smcontroller {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * SM controller.
 */
class SMControllerItf : public unitconfig::NodeConfigHandlerItf,
                        public launcher::InstanceRunnerItf,
                        public launcher::NodeEnvVarHandlerItf,
                        public launcher::MonitoringProviderItf,
                        public networkmanager::NodeNetworkItf,
                        public LogProviderItf { };

/** @}*/

} // namespace aos::cm::smcontroller

#endif
